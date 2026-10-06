// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Effects/VeyraEffectsCommandlet.h"

#if WITH_EDITOR
#include "Dom/JsonObject.h"
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
	/** The generator version this code is; the spec must name it. */
	constexpr int32 GeneratorVersion = 2;

	/**
	 * A module input the spec sets to a value of its own (1 number a float, 3 a vector, 4 a colour), or to an expression
	 * of the system's parameters (as a size in terms of the user scale, or a whole number for a count). It is set on the
	 * one emitter it names, or on every emitter that has it when it names none.
	 */
	struct FEffectInput
	{
		FName Script;
		FName Module;
		FName Input;
		TArray<double> Value;
		FString Expression;
		FName Emitter;
	};

	/**
	 * One system of the spec: its name, the engine system or emitter template it starts from, the inputs it sets, and
	 * the generated materials its sprites and its ribbons draw with.
	 */
	struct FEffectSpec
	{
		FString Name;
		FString Template;
		FString Emitter;
		FString Material;
		FString RibbonMaterial;
		TArray<FEffectInput> Inputs;
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
		else
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
			UNiagaraExternalEditUtilities::SetStackInputData(Reference, Value, Edit);
			Set += Edit.Errors.Num() == Errors ? 1 : 0;
		}
		return Set;
	}

	/** How many renderers of RendererClass System's emitters have. */
	int32 CountRenderers(UNiagaraSystem& System, const FNiagaraExt_SystemSummary& Summary, UClass* RendererClass, FNiagaraExternalEditContext& Edit)
	{
		int32 Count = 0;
		for (const FNiagaraExt_EmitterSummary& Emitter : Summary.Emitters)
		{
			FNiagaraExt_EmitterTopology Topology;
			UNiagaraExternalEditUtilities::GetEmitterTopology(FNiagaraExt_StackItemReference(&System, Emitter.EmitterName), Topology, Edit);
			for (int32 Index = 0; Index < Topology.RendererClasses.Num(); ++Index)
			{
				Count += Topology.RendererClasses[Index] == RendererClass ? 1 : 0;
			}
		}
		return Count;
	}

	/** Draws every renderer of RendererClass (sprites or ribbons, whose material is their own "Material") with Material; how many it set. */
	int32 SetRendererMaterial(UNiagaraSystem& System, const FNiagaraExt_SystemSummary& Summary, UClass* RendererClass, const FString& Material,
		FNiagaraExternalEditContext& Edit)
	{
		int32 Set = 0;
		for (const FNiagaraExt_EmitterSummary& Emitter : Summary.Emitters)
		{
			FNiagaraExt_EmitterTopology Topology;
			UNiagaraExternalEditUtilities::GetEmitterTopology(FNiagaraExt_StackItemReference(&System, Emitter.EmitterName), Topology, Edit);
			for (int32 Index = 0; Index < Topology.RendererClasses.Num(); ++Index)
			{
				if (Topology.RendererClasses[Index] != RendererClass)
				{
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
				Properties->SetStringField(TEXT("Material"), Material);
				const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Data.PropertyValues);
				FJsonSerializer::Serialize(Properties.ToSharedRef(), Writer);
				UNiagaraExternalEditUtilities::SetRendererData(Renderer, Data, Edit);
				++Set;
			}
		}
		return Set;
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
		if (!Object || !Object->TryGetStringField(TEXT("name"), Effect.Name) || bTemplate == bEmitter)
		{
			UE_LOG(LogVeyraEffects, Error, TEXT("%s: each system needs a name and either a system template or an emitter template."), *SpecFile);
			return 1;
		}
		Object->TryGetStringField(TEXT("material"), Effect.Material);
		Object->TryGetStringField(TEXT("ribbonMaterial"), Effect.RibbonMaterial);
		const TArray<TSharedPtr<FJsonValue>>* Inputs = nullptr;
		if (Object->TryGetArrayField(TEXT("inputs"), Inputs))
		{
			for (const TSharedPtr<FJsonValue>& InputValue : *Inputs)
			{
				const TSharedPtr<FJsonObject> InputObject = InputValue->AsObject();
				FString Script, Module, Input, Expression, Emitter;
				const TArray<TSharedPtr<FJsonValue>>* Numbers = nullptr;
				const bool bNumbers = InputObject && InputObject->TryGetArrayField(TEXT("value"), Numbers);
				const bool bExpression = InputObject && InputObject->TryGetStringField(TEXT("expression"), Expression) && !Expression.IsEmpty();
				if (!InputObject || !InputObject->TryGetStringField(TEXT("script"), Script) || !InputObject->TryGetStringField(TEXT("module"), Module)
					|| !InputObject->TryGetStringField(TEXT("input"), Input) || bNumbers == bExpression
					|| (bNumbers && !(Numbers->Num() == 1 || Numbers->Num() == 3 || Numbers->Num() == 4)))
				{
					UE_LOG(LogVeyraEffects, Error, TEXT("%s: %s: each input needs a script, a module, an input and either a value of 1, 3 or 4 numbers or an expression."),
						*SpecFile, *Effect.Name);
					return 1;
				}
				FEffectInput& Set = Effect.Inputs.Add_GetRef({ FName(*Script), FName(*Module), FName(*Input) });
				Set.Expression = Expression;
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
	if (FParse::Value(*Params, TEXT("Only="), OnlyList))
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
		if (!Template && !EmitterTemplate)
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
			if (SetInput(*System, Summary, Input, Edit) == 0)
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
		struct FRendererMaterial
		{
			UClass* Class;
			const TCHAR* What;
			const TCHAR* Field;
			const FString& Material;
		};
		for (const FRendererMaterial& Renderers : { FRendererMaterial{ UNiagaraSpriteRendererProperties::StaticClass(), TEXT("sprite"), TEXT("material"), Effect.Material },
				 FRendererMaterial{ UNiagaraRibbonRendererProperties::StaticClass(), TEXT("ribbon"), TEXT("ribbonMaterial"), Effect.RibbonMaterial } })
		{
			const int32 Count = CountRenderers(*System, Summary, Renderers.Class, Edit);
			if (Renderers.Material.IsEmpty() && Count > 0)
			{
				UE_LOG(LogVeyraEffects, Error, TEXT("%s: %d %s renderer(s) would draw with the engine's default material, which the scene's exposure shows black; name a generated one in its spec's %s."),
					*Effect.Name, Count, Renderers.What, Renderers.Field);
				return 1;
			}
			if (!Renderers.Material.IsEmpty() && (!LoadObject<UMaterialInterface>(nullptr, *Renderers.Material) || Count == 0
				|| SetRendererMaterial(*System, Summary, Renderers.Class, Renderers.Material, Edit) != Count))
			{
				LogErrors(Edit, Effect.Name);
				UE_LOG(LogVeyraEffects, Error, TEXT("%s: its %s %s does not load, it has no %s renderer to draw with it, or not every one took it."), *Effect.Name,
					Renderers.Field, *Renderers.Material, Renderers.What);
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
