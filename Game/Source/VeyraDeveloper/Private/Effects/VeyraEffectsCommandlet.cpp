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
#include "NiagaraExternalSystemEditorUtilities.h"
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
	constexpr int32 GeneratorVersion = 1;

	struct FEffectSpec
	{
		FString Name;
		FString Template;
	};

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
	const TSharedPtr<FJsonObject>* UserVariables = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Systems = nullptr;
	if (Spec->GetIntegerField(TEXT("schemaVersion")) != 1 || Spec->GetIntegerField(TEXT("generatorVersion")) != GeneratorVersion
		|| !Spec->TryGetStringField(TEXT("destination"), Destination) || !Destination.StartsWith(TEXT("/Game/Veyra/UI/"))
		|| !Spec->TryGetObjectField(TEXT("userVariables"), UserVariables) || !(*UserVariables)->TryGetStringField(TEXT("color"), ColorName)
		|| !Spec->TryGetArrayField(TEXT("systems"), Systems) || Systems->IsEmpty())
	{
		UE_LOG(LogVeyraEffects, Error, TEXT("%s: needs schema 1 for generator %d, a destination under /Game/Veyra/UI/ (always cooked), the user colour's name and systems."),
			*SpecFile, GeneratorVersion);
		return 1;
	}
	TArray<FEffectSpec> Effects;
	for (const TSharedPtr<FJsonValue>& Value : *Systems)
	{
		const TSharedPtr<FJsonObject> Object = Value->AsObject();
		FEffectSpec& Effect = Effects.AddDefaulted_GetRef();
		if (!Object || !Object->TryGetStringField(TEXT("name"), Effect.Name) || !Object->TryGetStringField(TEXT("template"), Effect.Template))
		{
			UE_LOG(LogVeyraEffects, Error, TEXT("%s: each system needs a name and a template."), *SpecFile);
			return 1;
		}
	}

	const FString Saved = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Effects"));
	IFileManager::Get().MakeDirectory(*Saved, /*Tree*/ true);
	const bool bDescribe = FParse::Param(*Params, TEXT("Describe"));
	const FNiagaraTypeDefinition ColorType = FNiagaraTypeDefinition::GetColorDef();
	const FName UserColor(*(TEXT("User.") + ColorName));
	const TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetNumberField(TEXT("generatorVersion"), GeneratorVersion);
	const FTCHARToUTF8 SpecBytes(*SpecText);
	Report->SetStringField(TEXT("specSha1"), FSHA1::HashBuffer(SpecBytes.Get(), SpecBytes.Length()).ToString());
	TArray<TSharedPtr<FJsonValue>> Built;
	for (const FEffectSpec& Effect : Effects)
	{
		UNiagaraSystem* Template = LoadObject<UNiagaraSystem>(nullptr, *Effect.Template);
		if (!Template)
		{
			UE_LOG(LogVeyraEffects, Error, TEXT("%s: its template %s does not load."), *Effect.Name, *Effect.Template);
			return 1;
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
		// One colour, the side's, set by the presentation as it spawns the system.
		FNiagaraExternalEditContext Edit(System);
		FNiagaraExt_UserVariable Color;
		Color.Name = UserColor;
		Color.Type = ColorType;
		const FLinearColor White = FLinearColor::White;
		Color.DefaultValue.Set(ColorType, FNiagaraVariant(&White, sizeof(White)));
		Color.Description = FText::FromString(TEXT("The side colour the presentation gives this effect (ADR-063 §4)."));
		UNiagaraExternalEditUtilities::AddUserVariable(System, Color, Edit);
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
		System->RequestCompile(/*bForce*/ false);
		System->WaitForCompilationComplete(/*bIncludingGPUShaders*/ true);
		FNiagaraExt_SystemCompileState State;
		UNiagaraExternalEditUtilities::GetSystemCompileState(System, State, Edit);
		if (LogErrors(Edit, Effect.Name) || State.bHasErrors)
		{
			UE_LOG(LogVeyraEffects, Error, TEXT("%s does not compile."), *Effect.Name);
			return 1;
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
		Entry->SetStringField(TEXT("template"), Effect.Template);
		Entry->SetNumberField(TEXT("linkedColorInputs"), Linked);
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
