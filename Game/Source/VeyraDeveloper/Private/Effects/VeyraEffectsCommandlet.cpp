// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Effects/VeyraEffectsCommandlet.h"

#if WITH_EDITOR
#include "Dom/JsonObject.h"
#include "Effects/VeyraEffectsEnum.h"
#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraEmitter.h"
#include "NiagaraExternalSystemEditorUtilities.h"
#include "NiagaraRibbonRendererProperties.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraSystem.h"
#include "NiagaraTypes.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogVeyraEffects, Log, All);

#if WITH_EDITOR
namespace
{
	/**
	 * The generator version this code is; the spec must name it. Version 3: a system made of named emitters, each from
	 * an emitter template with materials and sprite settings of its own (ADR-068 §4).
	 */
	constexpr int32 GeneratorVersion = 3;

	/**
	 * A module input the spec sets to a value of its own (1 number a float, 3 a vector, 4 a colour), or to an expression
	 * of the system's parameters (as a size in terms of the user scale, or a whole number for a count), or to one of its
	 * enum's values by its display name as authored, never a translation (a static switch, such as a mode that shows the
	 * input it governs). It is set on the
	 * one emitter it names, or on every emitter that has it when it names none.
	 */
	struct FEffectInput
	{
		FName Script;
		FName Module;
		FName Input;
		TArray<double> Value;
		FString Expression;
		FString Enum;
		FName Emitter;
	};

	/**
	 * Value set to the entry of Reference's enum input whose display name, as authored, is Display; false if it has
	 * none. Its display names are matched by their source text, so an editor in any culture builds the same system.
	 */
	bool EnumValue(const FNiagaraExt_StackItemReference& Reference, const FString& Display, FNiagaraExt_StackInputValue& Value, FNiagaraExternalEditContext& Edit)
	{
		FNiagaraExt_StackInputTopology Topology;
		UNiagaraExternalEditUtilities::GetStackInputTopology(Reference, Topology, Edit);
		UEnum* Enum = Topology.Type.GetEnum();
		if (!Enum)
		{
			return false;
		}
		TArray<FText> DisplayNames;
		for (int32 Index = 0; Index < Enum->NumEnums(); ++Index)
		{
			DisplayNames.Add(Enum->GetDisplayNameTextByIndex(Index));
		}
		const int32 Index = VeyraEffects::FindByAuthoredName(DisplayNames, Display);
		if (Index == INDEX_NONE)
		{
			return false;
		}
		Value.InitializeAs<FNiagaraExt_StackInputData_Enum>();
		FNiagaraExt_StackInputData_Enum& Entry = Value.GetMutable<FNiagaraExt_StackInputData_Enum>();
		Entry.Enum = Enum;
		Entry.EnumName = Enum->GetNameByIndex(Index);
		Entry.DisplayName = DisplayNames[Index];
		return true;
	}

	/**
	 * One emitter of a system made of named emitters: its name in the system, the engine emitter template it starts from,
	 * the generated materials its sprites and ribbons draw with (the system's where it names none), and settings of its
	 * sprite renderers, by their property names as the renderer's data spells them (as Alignment).
	 */
	struct FEmitterSpec
	{
		FString Name;
		FString Template;
		FString Material;
		FString RibbonMaterial;
		TSharedPtr<FJsonObject> Sprite;
	};

	/**
	 * One system of the spec: its name; the engine system template or emitter template it starts from, or the named
	 * emitters it is made of; the inputs it sets; and the generated materials its sprites and its ribbons draw with.
	 */
	struct FEffectSpec
	{
		FString Name;
		FString Template;
		FString Emitter;
		TArray<FEmitterSpec> Emitters;
		FString Material;
		FString RibbonMaterial;
		/** Settings of every sprite renderer whose emitter gives none of its own. */
		TSharedPtr<FJsonObject> Sprite;
		TArray<FEffectInput> Inputs;

		/** User floats of its own beyond the colour and scale every system has, as a beam's length (ADR-072 §4), and their defaults. */
		TArray<TPair<FString, double>> UserFloats;
	};

	/** What an emitter's renderers of one class draw with: a generated material, and settings of their own. */
	struct FRendererSetup
	{
		FString Material;
		TSharedPtr<FJsonObject> Properties;
	};

	/** Sets Input on the emitter it names, or on every emitter of System that has it; how many it set. */
	int32 SetInput(UNiagaraSystem& System, const FNiagaraExt_SystemSummary& Summary, const FEffectInput& Input, FNiagaraExternalEditContext& Edit)
	{
		FNiagaraExt_StackInputValue Value;
		// A local value is the instanced struct of its type (FNiagaraExt_StackInputValue's AdditionalTypes).
		if (!Input.Expression.IsEmpty())
		{
			Value.InitializeAs<FNiagaraExt_StackInputData_HlslExpression>();
			Value.GetMutable<FNiagaraExt_StackInputData_HlslExpression>().HlslExpression = Input.Expression;
		}
		else if (Input.Value.Num() == 1)
		{
			FNiagaraFloat Number;
			Number.Value = static_cast<float>(Input.Value[0]);
			Value.InitializeAs(FNiagaraFloat::StaticStruct(), reinterpret_cast<const uint8*>(&Number));
		}
		else if (Input.Value.Num() == 3)
		{
			const FVector3f Vector(Input.Value[0], Input.Value[1], Input.Value[2]);
			Value.InitializeAs(TVariantStructure<FVector3f>::Get(), reinterpret_cast<const uint8*>(&Vector));
		}
		else if (Input.Value.Num() == 4)
		{
			const FLinearColor Color(Input.Value[0], Input.Value[1], Input.Value[2], Input.Value[3]);
			Value.InitializeAs(TBaseStructure<FLinearColor>::Get(), reinterpret_cast<const uint8*>(&Color));
		}
		else if (Input.Enum.IsEmpty())
		{
			return 0;
		}
		int32 Set = 0;
		for (const FNiagaraExt_EmitterSummary& Emitter : Summary.Emitters)
		{
			if (!Input.Emitter.IsNone() && Emitter.EmitterName != Input.Emitter)
			{
				continue;
			}
			FNiagaraExt_StackItemReference Reference(&System, Emitter.EmitterName, Input.Script, Input.Module);
			Reference.InputNameStack.Add(Input.Input);
			const int32 Errors = Edit.Errors.Num();
			// An enum's entries are its type's, which only this emitter's input can say.
			if (!Input.Enum.IsEmpty() && !EnumValue(Reference, Input.Enum, Value, Edit))
			{
				continue;
			}
			UNiagaraExternalEditUtilities::SetStackInputData(Reference, Value, Edit);
			Set += Edit.Errors.Num() == Errors ? 1 : 0;
		}
		return Set;
	}

	/**
	 * Draws every renderer of RendererClass (sprites or ribbons, whose material is their own "Material") with what SetupOf
	 * gives its emitter, its material and its settings; how many it set. Missing counts those whose emitter has no
	 * material, which would keep the engine's default; Used names the emitters that drew with one.
	 */
	int32 SetRenderers(UNiagaraSystem& System, const FNiagaraExt_SystemSummary& Summary, UClass* RendererClass, TFunctionRef<FRendererSetup(FName)> SetupOf,
		int32& Missing, TSet<FName>& Used, FNiagaraExternalEditContext& Edit)
	{
		int32 Set = 0;
		for (const FNiagaraExt_EmitterSummary& Emitter : Summary.Emitters)
		{
			FNiagaraExt_EmitterTopology Topology;
			UNiagaraExternalEditUtilities::GetEmitterTopology(FNiagaraExt_StackItemReference(&System, Emitter.EmitterName), Topology, Edit);
			const FRendererSetup Setup = SetupOf(Emitter.EmitterName);
			for (int32 Index = 0; Index < Topology.RendererClasses.Num(); ++Index)
			{
				if (Topology.RendererClasses[Index] != RendererClass)
				{
					continue;
				}
				if (Setup.Material.IsEmpty())
				{
					++Missing;
					continue;
				}
				FNiagaraExt_StackItemReference Renderer(&System, Emitter.EmitterName);
				Renderer.RendererIndex = Index;
				FNiagaraExt_RendererData Data;
				UNiagaraExternalEditUtilities::GetRendererData(Renderer, Data, Edit);
				TSharedPtr<FJsonObject> Properties;
				if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Data.PropertyValues), Properties) || !Properties)
				{
					continue;
				}
				Properties->SetStringField(TEXT("Material"), Setup.Material);
				if (Setup.Properties)
				{
					for (const TPair<FString, TSharedPtr<FJsonValue>>& Property : Setup.Properties->Values)
					{
						Properties->SetField(Property.Key, Property.Value);
					}
				}
				const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Data.PropertyValues);
				FJsonSerializer::Serialize(Properties.ToSharedRef(), Writer);
				UNiagaraExternalEditUtilities::SetRendererData(Renderer, Data, Edit);
				Used.Add(Emitter.EmitterName);
				++Set;
			}
		}
		return Set;
	}

	/** Every renderer of System's emitters, its class and its data as the renderer spells it, to write a spec against. */
	TArray<TSharedPtr<FJsonValue>> DescribeRenderers(UNiagaraSystem& System, const FNiagaraExt_SystemSummary& Summary, FNiagaraExternalEditContext& Edit)
	{
		TArray<TSharedPtr<FJsonValue>> Renderers;
		for (const FNiagaraExt_EmitterSummary& Emitter : Summary.Emitters)
		{
			FNiagaraExt_EmitterTopology Topology;
			UNiagaraExternalEditUtilities::GetEmitterTopology(FNiagaraExt_StackItemReference(&System, Emitter.EmitterName), Topology, Edit);
			for (int32 Index = 0; Index < Topology.RendererClasses.Num(); ++Index)
			{
				FNiagaraExt_StackItemReference Renderer(&System, Emitter.EmitterName);
				Renderer.RendererIndex = Index;
				FNiagaraExt_RendererData Data;
				UNiagaraExternalEditUtilities::GetRendererData(Renderer, Data, Edit);
				const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
				Entry->SetStringField(TEXT("emitter"), Emitter.EmitterName.ToString());
				Entry->SetStringField(TEXT("class"), GetNameSafe(Topology.RendererClasses[Index]));
				TSharedPtr<FJsonObject> Properties;
				if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Data.PropertyValues), Properties) && Properties)
				{
					Entry->SetObjectField(TEXT("properties"), Properties);
				}
				Renderers.Add(MakeShared<FJsonValueObject>(Entry));
			}
		}
		return Renderers;
	}

	/** The input every particle starts its colour from: a module's own "Color", as the templates' Initialize Particle has. */
	const FName BaseColorInput(TEXT("Color"));

	/**
	 * Every enabled, editable base colour input of Emitter's particles. Colours a module scales by, or sends on, are
	 * left as the template has them: linking those too would tint the colour twice.
	 */
	TArray<FNiagaraExt_StackItemReference> ColorInputsOf(UNiagaraSystem& System, const FNiagaraExt_EmitterTopology& Emitter)
	{
		TArray<FNiagaraExt_StackItemReference> Inputs;
		for (const FNiagaraExt_ScriptStackTopology* Stack : { &Emitter.ParticleSpawnScript, &Emitter.ParticleUpdateScript })
		{
			for (const FNiagaraExt_ModuleTopology& Module : Stack->Modules)
			{
				for (const FNiagaraExt_StackInputTopology& Input : Module.Inputs)
				{
					if (Module.Enabled && Input.bIsEditable && Input.Type == FNiagaraTypeDefinition::GetColorDef() && Input.Name == BaseColorInput)
					{
						FNiagaraExt_StackItemReference Reference(&System, Emitter.EmitterName, Stack->ScriptName, Module.ModuleName);
						Reference.InputNameStack.Add(Input.Name);
						Inputs.Add(Reference);
					}
				}
			}
		}
		return Inputs;
	}

	bool LogErrors(const FNiagaraExternalEditContext& Context, const FString& What)
	{
		for (const FText& Error : Context.Errors)
		{
			UE_LOG(LogVeyraEffects, Error, TEXT("%s: %s"), *What, *Error.ToString());
		}
		return Context.HasErrors();
	}

	bool WriteJson(const FString& File, const TSharedRef<FJsonObject>& Object)
	{
		FString Text;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
		return FJsonSerializer::Serialize(Object, Writer) && FFileHelper::SaveStringToFile(Text + TEXT("\n"), *File);
	}
}
#endif

UVeyraEffectsCommandlet::UVeyraEffectsCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UVeyraEffectsCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	const FString SpecFile = FPaths::Combine(FPaths::ProjectDir(), TEXT("ArtSource/Presentation/Effects.json"));
	FString SpecText;
	TSharedPtr<FJsonObject> Spec;
	if (!FFileHelper::LoadFileToString(SpecText, *SpecFile) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(SpecText), Spec) || !Spec)
	{
		UE_LOG(LogVeyraEffects, Error, TEXT("%s does not read as JSON."), *SpecFile);
		return 1;
	}
	FString Destination;
	FString ColorName;
	FString ScaleName;
	const TSharedPtr<FJsonObject>* UserVariables = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Systems = nullptr;
	if (Spec->GetIntegerField(TEXT("schemaVersion")) != 1 || Spec->GetIntegerField(TEXT("generatorVersion")) != GeneratorVersion
		|| !Spec->TryGetStringField(TEXT("destination"), Destination) || !Destination.StartsWith(TEXT("/Game/Veyra/UI/"))
		|| !Spec->TryGetObjectField(TEXT("userVariables"), UserVariables) || !(*UserVariables)->TryGetStringField(TEXT("color"), ColorName)
		|| !(*UserVariables)->TryGetStringField(TEXT("scale"), ScaleName) || !Spec->TryGetArrayField(TEXT("systems"), Systems) || Systems->IsEmpty())
	{
		UE_LOG(LogVeyraEffects, Error, TEXT("%s: needs schema 1 for generator %d, a destination under /Game/Veyra/UI/ (always cooked), the user colour's and scale's names and systems."),
			*SpecFile, GeneratorVersion);
		return 1;
	}
	TArray<FEffectSpec> Effects;
	for (const TSharedPtr<FJsonValue>& Value : *Systems)
	{
		const TSharedPtr<FJsonObject> Object = Value->AsObject();
		FEffectSpec& Effect = Effects.AddDefaulted_GetRef();
		const bool bTemplate = Object && Object->TryGetStringField(TEXT("template"), Effect.Template);
		const bool bEmitter = Object && Object->TryGetStringField(TEXT("emitter"), Effect.Emitter);
		const TArray<TSharedPtr<FJsonValue>>* EmitterList = nullptr;
		const bool bEmitters = Object && Object->TryGetArrayField(TEXT("emitters"), EmitterList) && !EmitterList->IsEmpty();
		// A name is the asset it builds, so two systems of one name would each rebuild the same package, the last winning.
		if (!Object || !Object->TryGetStringField(TEXT("name"), Effect.Name) || int32(bTemplate) + int32(bEmitter) + int32(bEmitters) != 1
			|| Effects.FilterByPredicate([&Effect](const FEffectSpec& Other) { return Other.Name == Effect.Name; }).Num() > 1)
		{
			UE_LOG(LogVeyraEffects, Error, TEXT("%s: each system needs a name of its own and one of a system template, an emitter template or a list of named emitters."), *SpecFile);
			return 1;
		}
		if (bEmitters)
		{
			for (const TSharedPtr<FJsonValue>& EmitterValue : *EmitterList)
			{
				const TSharedPtr<FJsonObject> EmitterObject = EmitterValue->AsObject();
				FEmitterSpec& Emitter = Effect.Emitters.AddDefaulted_GetRef();
				const TSharedPtr<FJsonObject>* Sprite = nullptr;
				if (!EmitterObject || !EmitterObject->TryGetStringField(TEXT("name"), Emitter.Name) || Emitter.Name.IsEmpty()
					|| !EmitterObject->TryGetStringField(TEXT("template"), Emitter.Template)
					|| Effect.Emitters.FilterByPredicate([&Emitter](const FEmitterSpec& Other) { return Other.Name == Emitter.Name; }).Num() > 1)
				{
					UE_LOG(LogVeyraEffects, Error, TEXT("%s: %s: each of its emitters needs a name of its own and an emitter template."), *SpecFile, *Effect.Name);
					return 1;
				}
				EmitterObject->TryGetStringField(TEXT("material"), Emitter.Material);
				EmitterObject->TryGetStringField(TEXT("ribbonMaterial"), Emitter.RibbonMaterial);
				if (EmitterObject->TryGetObjectField(TEXT("sprite"), Sprite))
				{
					Emitter.Sprite = *Sprite;
				}
			}
		}
		Object->TryGetStringField(TEXT("material"), Effect.Material);
		Object->TryGetStringField(TEXT("ribbonMaterial"), Effect.RibbonMaterial);
		const TSharedPtr<FJsonObject>* SystemSprite = nullptr;
		if (Object->TryGetObjectField(TEXT("sprite"), SystemSprite))
		{
			Effect.Sprite = *SystemSprite;
		}
		const TArray<TSharedPtr<FJsonValue>>* UserFloats = nullptr;
		if (Object->TryGetArrayField(TEXT("userFloats"), UserFloats))
		{
			for (const TSharedPtr<FJsonValue>& FloatValue : *UserFloats)
			{
				const TSharedPtr<FJsonObject> FloatObject = FloatValue->AsObject();
				FString FloatName;
				double Default = 0.0;
				if (!FloatObject || !FloatObject->TryGetStringField(TEXT("name"), FloatName) || FloatName.IsEmpty() || !FloatObject->TryGetNumberField(TEXT("default"), Default)
					|| FloatName == ColorName || FloatName == ScaleName)
				{
					UE_LOG(LogVeyraEffects, Error, TEXT("%s: %s: each user float needs a name of its own (not the colour's or scale's) and a default."), *SpecFile, *Effect.Name);
					return 1;
				}
				Effect.UserFloats.Emplace(FloatName, Default);
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* Inputs = nullptr;
		if (Object->TryGetArrayField(TEXT("inputs"), Inputs))
		{
			for (const TSharedPtr<FJsonValue>& InputValue : *Inputs)
			{
				const TSharedPtr<FJsonObject> InputObject = InputValue->AsObject();
				FString Script, Module, Input, Expression, Enum, Emitter;
				const TArray<TSharedPtr<FJsonValue>>* Numbers = nullptr;
				const bool bNumbers = InputObject && InputObject->TryGetArrayField(TEXT("value"), Numbers);
				const bool bExpression = InputObject && InputObject->TryGetStringField(TEXT("expression"), Expression) && !Expression.IsEmpty();
				const bool bEnum = InputObject && InputObject->TryGetStringField(TEXT("enum"), Enum) && !Enum.IsEmpty();
				if (!InputObject || !InputObject->TryGetStringField(TEXT("script"), Script) || !InputObject->TryGetStringField(TEXT("module"), Module)
					|| !InputObject->TryGetStringField(TEXT("input"), Input) || int32(bNumbers) + int32(bExpression) + int32(bEnum) != 1
					|| (bNumbers && !(Numbers->Num() == 1 || Numbers->Num() == 3 || Numbers->Num() == 4)))
				{
					UE_LOG(LogVeyraEffects, Error, TEXT("%s: %s: each input needs a script, a module, an input and one of a value of 1, 3 or 4 numbers, an expression or an enum's display name."),
						*SpecFile, *Effect.Name);
					return 1;
				}
				FEffectInput& Set = Effect.Inputs.Add_GetRef({ FName(*Script), FName(*Module), FName(*Input) });
				Set.Expression = Expression;
				Set.Enum = Enum;
				if (InputObject->TryGetStringField(TEXT("emitter"), Emitter))
				{
					if (Emitter.IsEmpty())
					{
						UE_LOG(LogVeyraEffects, Error, TEXT("%s: %s: an input's emitter, when given, names one."), *SpecFile, *Effect.Name);
						return 1;
					}
					Set.Emitter = FName(*Emitter);
				}
				if (bNumbers)
				{
					for (const TSharedPtr<FJsonValue>& Number : *Numbers)
					{
						Set.Value.Add(Number->AsNumber());
					}
				}
			}
		}
	}

	const FString Saved = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Effects"));
	IFileManager::Get().MakeDirectory(*Saved, /*Tree*/ true);
	const bool bDescribe = FParse::Param(*Params, TEXT("Describe"));
	// -Only=A,B builds just those systems; without it, every one.
	FString OnlyList;
	TArray<FString> Only;
	// The whole list: FParse::Value stops at the first comma unless told otherwise.
	if (FParse::Value(*Params, TEXT("Only="), OnlyList, /*bShouldStopOnSeparator*/ false))
	{
		OnlyList.ParseIntoArray(Only, TEXT(","));
		for (const FString& Name : Only)
		{
			if (!Effects.ContainsByPredicate([&Name](const FEffectSpec& Effect) { return Effect.Name == Name; }))
			{
				UE_LOG(LogVeyraEffects, Error, TEXT("-Only names %s, which the spec does not."), *Name);
				return 1;
			}
		}
		Effects.RemoveAll([&Only](const FEffectSpec& Effect) { return !Only.Contains(Effect.Name); });
	}
	const FNiagaraTypeDefinition ColorType = FNiagaraTypeDefinition::GetColorDef();
	const FName UserColor(*(TEXT("User.") + ColorName));
	const TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetNumberField(TEXT("generatorVersion"), GeneratorVersion);
	const FTCHARToUTF8 SpecBytes(*SpecText);
	Report->SetStringField(TEXT("specSha1"), FSHA1::HashBuffer(SpecBytes.Get(), SpecBytes.Length()).ToString());
	TArray<TSharedPtr<FJsonValue>> Built;
	for (const FEffectSpec& Effect : Effects)
	{
		// A system template is copied whole; an emitter template becomes the one emitter of an empty system.
		UNiagaraSystem* Template = Effect.Template.IsEmpty() ? nullptr : LoadObject<UNiagaraSystem>(nullptr, *Effect.Template);
		UNiagaraEmitter* EmitterTemplate = Effect.Emitter.IsEmpty() ? nullptr : LoadObject<UNiagaraEmitter>(nullptr, *Effect.Emitter);
		// A system of named emitters: each from its own template.
		TArray<TPair<FName, UNiagaraEmitter*>> Named;
		for (const FEmitterSpec& Emitter : Effect.Emitters)
		{
			UNiagaraEmitter* Loaded = LoadObject<UNiagaraEmitter>(nullptr, *Emitter.Template);
			if (!Loaded)
			{
				UE_LOG(LogVeyraEffects, Error, TEXT("%s: its emitter %s's template %s does not load."), *Effect.Name, *Emitter.Name, *Emitter.Template);
				return 1;
			}
			Named.Emplace(FName(*Emitter.Name), Loaded);
		}
		if (!Template && !EmitterTemplate && Named.IsEmpty())
		{
			UE_LOG(LogVeyraEffects, Error, TEXT("%s: its template %s%s does not load."), *Effect.Name, *Effect.Template, *Effect.Emitter);
			return 1;
		}
		if (bDescribe && !Template)
		{
			UE_LOG(LogVeyraEffects, Display, TEXT("VEYRA_EFFECT_DESCRIBED: %s starts from an emitter template, described once built."), *Effect.Name);
			continue;
		}
		if (bDescribe)
		{
			// What the template holds, to write a spec against.
			FNiagaraExternalEditContext Context(Template);
			FNiagaraExt_SystemSummary Summary;
			UNiagaraExternalEditUtilities::GetSystemSummary(Template, Summary, Context);
			const TSharedRef<FJsonObject> Described = MakeShared<FJsonObject>();
			TArray<TSharedPtr<FJsonValue>> Emitters;
			for (const FNiagaraExt_EmitterSummary& Emitter : Summary.Emitters)
			{
				FNiagaraExt_EmitterTopology Topology;
				UNiagaraExternalEditUtilities::GetEmitterTopology(FNiagaraExt_StackItemReference(Template, Emitter.EmitterName), Topology, Context);
				if (const TSharedPtr<FJsonObject> Json = FJsonObjectConverter::UStructToJsonObject(Topology))
				{
					Emitters.Add(MakeShared<FJsonValueObject>(Json));
				}
			}
			Described->SetArrayField(TEXT("emitters"), Emitters);
			if (LogErrors(Context, Effect.Name) || !WriteJson(FPaths::Combine(Saved, Effect.Name + TEXT("-template.json")), Described))
			{
				return 1;
			}
			UE_LOG(LogVeyraEffects, Display, TEXT("VEYRA_EFFECT_DESCRIBED: %s"), *Effect.Name);
			continue;
		}

		// Rebuilt from its template, never edited in place; a locked asset is the author's to unlock (ADR-006 §9).
		const FString Package = Destination / Effect.Name;
		const FString File = FPackageName::LongPackageNameToFilename(Package, FPackageName::GetAssetPackageExtension());
		if (IFileManager::Get().FileExists(*File))
		{
			if (IFileManager::Get().IsReadOnly(*File))
			{
				UE_LOG(LogVeyraEffects, Error, TEXT("%s is locked: acquire its Git LFS lock before rebuilding it."), *File);
				return 1;
			}
			IFileManager::Get().Delete(*File);
		}
		FNiagaraExternalEditContext Context;
		UNiagaraSystem* System = UNiagaraExternalEditUtilities::CreateNiagaraSystem(Effect.Name, Destination, Template, Context);
		if (LogErrors(Context, Effect.Name) || !System)
		{
			return 1;
		}
		FNiagaraExternalEditContext Edit(System);
		if (EmitterTemplate)
		{
			FNiagaraExt_EmitterTopology Added;
			UNiagaraExternalEditUtilities::AddEmitter(EmitterTemplate, FName(*Effect.Name), Added, Edit);
		}
		for (const TPair<FName, UNiagaraEmitter*>& Emitter : Named)
		{
			FNiagaraExt_EmitterTopology Added;
			UNiagaraExternalEditUtilities::AddEmitter(Emitter.Value, Emitter.Key, Added, Edit);
		}
		// One colour, the side's, set by the presentation as it spawns the system.
		FNiagaraExt_UserVariable Color;
		Color.Name = UserColor;
		Color.Type = ColorType;
		const FLinearColor White = FLinearColor::White;
		Color.DefaultValue.Set(ColorType, FNiagaraVariant(&White, sizeof(White)));
		Color.Description = FText::FromString(TEXT("The side colour the presentation gives this effect (ADR-063 §4)."));
		UNiagaraExternalEditUtilities::AddUserVariable(System, Color, Edit);
		// And one scale, the body's, that a spec's expressions may size the effect by.
		const FNiagaraTypeDefinition ScaleType = FNiagaraTypeDefinition::GetFloatDef();
		FNiagaraExt_UserVariable Scale;
		Scale.Name = FName(*(TEXT("User.") + ScaleName));
		Scale.Type = ScaleType;
		const float One = 1.0f;
		Scale.DefaultValue.Set(ScaleType, FNiagaraVariant(&One, sizeof(One)));
		Scale.Description = FText::FromString(TEXT("How large the presentation draws this effect, as the body it pours from is scaled (ADR-064 §1)."));
		UNiagaraExternalEditUtilities::AddUserVariable(System, Scale, Edit);
		// Any of its own, that the presentation sets from what it shows (a beam's length, ADR-072 §4).
		for (const TPair<FString, double>& UserFloat : Effect.UserFloats)
		{
			FNiagaraExt_UserVariable Own;
			Own.Name = FName(*(TEXT("User.") + UserFloat.Key));
			Own.Type = ScaleType;
			const float Default = static_cast<float>(UserFloat.Value);
			Own.DefaultValue.Set(ScaleType, FNiagaraVariant(&Default, sizeof(Default)));
			Own.Description = FText::FromString(TEXT("Set by the presentation from what the effect shows (ADR-072 §4)."));
			UNiagaraExternalEditUtilities::AddUserVariable(System, Own, Edit);
		}
		FNiagaraExt_SystemSummary Summary;
		UNiagaraExternalEditUtilities::GetSystemSummary(System, Summary, Edit);
		int32 Linked = 0;
		for (const FNiagaraExt_EmitterSummary& Emitter : Summary.Emitters)
		{
			FNiagaraExt_EmitterTopology Topology;
			UNiagaraExternalEditUtilities::GetEmitterTopology(FNiagaraExt_StackItemReference(System, Emitter.EmitterName), Topology, Edit);
			for (const FNiagaraExt_StackItemReference& Input : ColorInputsOf(*System, Topology))
			{
				FNiagaraExt_StackInputValue Value;
				Value.InitializeAs<FNiagaraExt_StackInputData_Linked>();
				FNiagaraExt_Variable& Link = Value.GetMutable<FNiagaraExt_StackInputData_Linked>().LinkedVariable;
				Link.Name = UserColor;
				Link.Type = ColorType;
				UNiagaraExternalEditUtilities::SetStackInputData(Input, Value, Edit);
				++Linked;
			}
		}
		if (LogErrors(Edit, Effect.Name))
		{
			return 1;
		}
		if (Linked == 0)
		{
			UE_LOG(LogVeyraEffects, Error, TEXT("%s: its template %s has no colour input to link; pick another, or describe it with -Describe."), *Effect.Name,
				*Effect.Template);
			return 1;
		}
		// What the built system holds, to write its inputs against.
		{
			const TSharedRef<FJsonObject> Described = MakeShared<FJsonObject>();
			TArray<TSharedPtr<FJsonValue>> Emitters;
			for (const FNiagaraExt_EmitterSummary& Emitter : Summary.Emitters)
			{
				FNiagaraExt_EmitterTopology Topology;
				UNiagaraExternalEditUtilities::GetEmitterTopology(FNiagaraExt_StackItemReference(System, Emitter.EmitterName), Topology, Edit);
				if (const TSharedPtr<FJsonObject> Json = FJsonObjectConverter::UStructToJsonObject(Topology))
				{
					Emitters.Add(MakeShared<FJsonValueObject>(Json));
				}
			}
			Described->SetArrayField(TEXT("emitters"), Emitters);
			WriteJson(FPaths::Combine(Saved, Effect.Name + TEXT("-built.json")), Described);
		}
		for (const FEffectInput& Input : Effect.Inputs)
		{
			const int32 Set = SetInput(*System, Summary, Input, Edit);
			if (Set > 0 && !Input.Enum.IsEmpty())
			{
				// A switch changes which inputs the stack shows, and a context keeps the stack it first read: the inputs after
				// it are set through a fresh one.
				Edit = FNiagaraExternalEditContext(System);
			}
			if (Set == 0)
			{
				LogErrors(Edit, Effect.Name);
				const FString Where = Input.Emitter.IsNone() ? FString(TEXT("no emitter has")) : FString::Printf(TEXT("its emitter %s has no"), *Input.Emitter.ToString());
				UE_LOG(LogVeyraEffects, Error, TEXT("%s: %s %s.%s.%s; see %s-built.json."), *Effect.Name, *Where, *Input.Script.ToString(),
					*Input.Module.ToString(), *Input.Input.ToString(), *Effect.Name);
				return 1;
			}
		}
		// Every sprite and ribbon draws with a generated material. The engine's default ones are unlit, with an emissive near 1,
		// which the Crucible's physical sun and manual exposure show black; the generated ones glow alike under any exposure.
		// A mesh renderer draws its mesh's own materials, so a spec keeps none spawning (the death burst's template has one).
		// An emitter of a system of named emitters draws with its own materials and sprite settings, or the system's materials.
		for (const bool bSprite : { true, false })
		{
			UClass* Class = bSprite ? UNiagaraSpriteRendererProperties::StaticClass() : UNiagaraRibbonRendererProperties::StaticClass();
			const TCHAR* What = bSprite ? TEXT("sprite") : TEXT("ribbon");
			const TCHAR* Field = bSprite ? TEXT("material") : TEXT("ribbonMaterial");
			auto SetupOf = [&Effect, bSprite](FName Emitter)
			{
				const FEmitterSpec* Own = Effect.Emitters.FindByPredicate([Emitter](const FEmitterSpec& Spec) { return FName(*Spec.Name) == Emitter; });
				const FString& OwnMaterial = Own ? (bSprite ? Own->Material : Own->RibbonMaterial) : FString();
				return FRendererSetup{ !OwnMaterial.IsEmpty() ? OwnMaterial : (bSprite ? Effect.Material : Effect.RibbonMaterial),
					!bSprite ? nullptr : Own && Own->Sprite ? Own->Sprite : Effect.Sprite };
			};
			// Every material it names must load before any renderer takes it.
			TArray<FString> Wanted = { bSprite ? Effect.Material : Effect.RibbonMaterial };
			for (const FEmitterSpec& Emitter : Effect.Emitters)
			{
				Wanted.Add(bSprite ? Emitter.Material : Emitter.RibbonMaterial);
			}
			for (const FString& Material : Wanted)
			{
				if (!Material.IsEmpty() && !LoadObject<UMaterialInterface>(nullptr, *Material))
				{
					UE_LOG(LogVeyraEffects, Error, TEXT("%s: its %s %s does not load."), *Effect.Name, Field, *Material);
					return 1;
				}
			}
			int32 Missing = 0;
			TSet<FName> Used;
			SetRenderers(*System, Summary, Class, SetupOf, Missing, Used, Edit);
			if (LogErrors(Edit, Effect.Name))
			{
				return 1;
			}
			if (Missing > 0)
			{
				UE_LOG(LogVeyraEffects, Error, TEXT("%s: %d %s renderer(s) would draw with the engine's default material, which the scene's exposure shows black; name a generated one in its spec's %s."),
					*Effect.Name, Missing, What, Field);
				return 1;
			}
			// A material named for renderers that are not there is a mistake in the spec.
			const bool bSystemNamed = !(bSprite ? Effect.Material : Effect.RibbonMaterial).IsEmpty();
			const FEmitterSpec* Unused = Effect.Emitters.FindByPredicate([&Used, bSprite](const FEmitterSpec& Emitter)
				{ return !(bSprite ? Emitter.Material : Emitter.RibbonMaterial).IsEmpty() && !Used.Contains(FName(*Emitter.Name)); });
			if ((bSystemNamed && Used.IsEmpty()) || Unused)
			{
				UE_LOG(LogVeyraEffects, Error, TEXT("%s: its spec names a %s for %s, which has no %s renderer."), *Effect.Name, Field,
					Unused ? *Unused->Name : TEXT("the system"), What);
				return 1;
			}
		}
		System->RequestCompile(/*bForce*/ false);
		System->WaitForCompilationComplete(/*bIncludingGPUShaders*/ true);
		FNiagaraExt_SystemCompileState State;
		UNiagaraExternalEditUtilities::GetSystemCompileState(System, State, Edit);
		if (LogErrors(Edit, Effect.Name) || State.bHasErrors)
		{
			UE_LOG(LogVeyraEffects, Error, TEXT("%s does not compile."), *Effect.Name);
			return 1;
		}
		// What every module's inputs hold once built (the template's own values beside those written here), to judge it by.
		{
			const TSharedRef<FJsonObject> Held = MakeShared<FJsonObject>();
			for (const FNiagaraExt_EmitterSummary& Emitter : Summary.Emitters)
			{
				TArray<FNiagaraExt_ModuleInputValues> Values;
				UNiagaraExternalEditUtilities::GetEmitterInputValues(FNiagaraExt_StackItemReference(System, Emitter.EmitterName), Values, Edit);
				// Each value as its struct's text: JSON conversion cannot see inside the instanced struct that holds it.
				const TSharedRef<FJsonObject> Modules = MakeShared<FJsonObject>();
				for (const FNiagaraExt_ModuleInputValues& Module : Values)
				{
					const TSharedRef<FJsonObject> Inputs = MakeShared<FJsonObject>();
					for (const FNiagaraExt_StackInputValueEntry& Input : Module.Inputs)
					{
						FString Text;
						if (const UScriptStruct* Kind = Input.Value.GetScriptStruct())
						{
							Text = Kind->GetName();
							Kind->ExportText(Text, Input.Value.GetMemory(), nullptr, nullptr, PPF_None, nullptr);
						}
						Inputs->SetStringField(Input.Name.ToString(), Text);
					}
					Modules->SetObjectField(Module.ModuleName.ToString(), Inputs);
				}
				Held->SetObjectField(Emitter.EmitterName.ToString(), Modules);
			}
			// And what each renderer draws with, as its data spells it, which a spec's sprite settings are written in.
			Held->SetArrayField(TEXT("renderers"), DescribeRenderers(*System, Summary, Edit));
			WriteJson(FPaths::Combine(Saved, Effect.Name + TEXT("-values.json")), Held);
		}
		FSavePackageArgs Save;
		Save.TopLevelFlags = RF_Public | RF_Standalone;
		Save.Error = GError;
		if (!UPackage::SavePackage(System->GetOutermost(), System, *File, Save))
		{
			UE_LOG(LogVeyraEffects, Error, TEXT("%s did not save to %s."), *Effect.Name, *File);
			return 1;
		}
		const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("asset"), System->GetPathName());
		Entry->SetStringField(TEXT("template"), Template ? Effect.Template : Effect.Emitter);
		Entry->SetNumberField(TEXT("linkedColorInputs"), Linked);
		Entry->SetStringField(TEXT("material"), Effect.Material);
		Entry->SetStringField(TEXT("ribbonMaterial"), Effect.RibbonMaterial);
		Built.Add(MakeShared<FJsonValueObject>(Entry));
		UE_LOG(LogVeyraEffects, Display, TEXT("VEYRA_EFFECT: %s (%d colour input(s) linked to %s)"), *System->GetPathName(), Linked, *UserColor.ToString());
	}
	if (!bDescribe)
	{
		Report->SetArrayField(TEXT("effects"), Built);
		if (!WriteJson(FPaths::Combine(Saved, TEXT("build.json")), Report))
		{
			return 1;
		}
	}
	UE_LOG(LogVeyraEffects, Display, TEXT("VEYRA_EFFECTS_PASSED: %d system(s)"), Effects.Num());
	return 0;
#else
	UE_LOG(LogVeyraEffects, Error, TEXT("The effects generator needs the editor."));
	return 1;
#endif
}
