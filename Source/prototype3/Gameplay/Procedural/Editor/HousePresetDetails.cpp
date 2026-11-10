#include "Gameplay/Procedural/Editor/HousePresetDetails.h"

#if WITH_EDITOR
#include "Gameplay/Procedural/HouseGenerationPreset.h"
#include "Gameplay/Procedural/HouseGenerator.h"
#include "Gameplay/Procedural/Encounter/HouseEncounter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailCustomization.h"
#include "IDetailPropertyRow.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	FText NoneText() { return NSLOCTEXT("HousePresetDetails", "None", "None (Custom)"); }

	/** Single-choice preset list. An empty FAssetData entry represents None (Custom controls). */
	class SHousePresetPicker : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SHousePresetPicker) {} SLATE_END_ARGS()

		void Construct(const FArguments&, TSharedRef<IPropertyHandle> InHandle)
		{
			Handle = InHandle;
			RefreshOptions();
			ChildSlot
			[
				SAssignNew(Combo, SComboBox<TSharedPtr<FAssetData>>)
				// Stay in the owning window; separate popup windows have crashed on Linux/Vulkan.
				.Method(EPopupMethod::UseCurrentWindow)
				.OptionsSource(&Options)
				.OnComboBoxOpening_Lambda([this] { RefreshOptions(); Combo->RefreshOptions(); })
				.IsEnabled_Lambda([this] { return Handle->IsEditable(); })
				.OnGenerateWidget_Lambda([](TSharedPtr<FAssetData> Asset) -> TSharedRef<SWidget>
				{
					return SNew(STextBlock).Font(IDetailLayoutBuilder::GetDetailFont())
						.Text(Asset.IsValid() && Asset->IsValid() ? FText::FromName(Asset->AssetName) : NoneText());
				})
				.OnSelectionChanged_Lambda([this](TSharedPtr<FAssetData> Asset, ESelectInfo::Type Reason)
				{
					if (!Asset.IsValid() || Reason == ESelectInfo::Direct) return;
					// Through the property handle, so undo, edit notifications and the Custom copy still run.
					UObject* Value = Asset->IsValid() ? Asset->GetAsset() : nullptr;
					Handle->SetValue(Value);
				})
				[
					SNew(STextBlock).Font(IDetailLayoutBuilder::GetDetailFont()).Text_Lambda([this]
					{
						UObject* Value = nullptr;
						if (Handle->GetValue(Value) == FPropertyAccess::MultipleValues)
							return NSLOCTEXT("HousePresetDetails", "Multiple", "Multiple Values");
						return Value ? FText::FromString(Value->GetName()) : NoneText();
					})
				]
			];
		}

	private:
		void RefreshOptions()
		{
			Options.Reset();
			Options.Add(MakeShared<FAssetData>());
			TArray<FAssetData> Assets;
			FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get()
				.GetAssetsByClass(UHouseGenerationPreset::StaticClass()->GetClassPathName(), Assets, true);
			Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.ToString() < B.AssetName.ToString(); });
			for (const FAssetData& Asset : Assets) Options.Add(MakeShared<FAssetData>(Asset));
		}

		TSharedPtr<IPropertyHandle> Handle;
		TSharedPtr<SComboBox<TSharedPtr<FAssetData>>> Combo;
		TArray<TSharedPtr<FAssetData>> Options;
	};

	class FHousePresetDetails : public IDetailCustomization
	{
	public:
		static TSharedRef<IDetailCustomization> Make() { return MakeShared<FHousePresetDetails>(); }
		virtual void CustomizeDetails(IDetailLayoutBuilder& Builder) override
		{
			const TSharedRef<IPropertyHandle> Handle = Builder.GetProperty(TEXT("Preset"));
			if (!Handle->IsValidHandle()) return;
			// Keep the standard row so the reset arrow and edit conditions still work.
			Builder.EditDefaultProperty(Handle)->CustomWidget()
				.NameContent()[Handle->CreatePropertyNameWidget()]
				.ValueContent().MinDesiredWidth(220)[SNew(SHousePresetPicker, Handle)];
		}
	};

	// Resolved at registration time, never during static initialization.
	TArray<FName> CustomizedClasses() { return {AHouseEncounter::StaticClass()->GetFName(), AHouseGenerator::StaticClass()->GetFName()}; }
}

void RegisterHousePresetDetails()
{
	if (IsRunningCommandlet()) return;
	FPropertyEditorModule& Module = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	for (const FName Class : CustomizedClasses())
		Module.RegisterCustomClassLayout(Class, FOnGetDetailCustomizationInstance::CreateStatic(&FHousePresetDetails::Make));
	Module.NotifyCustomizationModuleChanged();
}

void UnregisterHousePresetDetails()
{
	if (FPropertyEditorModule* Module = FModuleManager::GetModulePtr<FPropertyEditorModule>(TEXT("PropertyEditor")))
		for (const FName Class : CustomizedClasses()) Module->UnregisterCustomClassLayout(Class);
}
#endif
