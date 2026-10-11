#include "Gameplay/Procedural/Editor/HouseDetails.h"

#if WITH_EDITOR && PLATFORM_LINUX
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "IPropertyTypeCustomization.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "Modules/ModuleManager.h"
#include "UObject/UnrealType.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SCompoundWidget.h"

namespace
{
	// Linux/NVIDIA can crash when a menu or tooltip creates another Vulkan swapchain.
	// Keep these menus in the owning editor window and suppress their native tooltips.
	class SHousePopupScope : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SHousePopupScope) {} SLATE_DEFAULT_SLOT(FArguments, Content) SLATE_END_ARGS()
		void Construct(const FArguments& Args) { ChildSlot[Args._Content.Widget]; }
		virtual FPopupMethodReply OnQueryPopupMethod() const override { return FPopupMethodReply::UseMethod(EPopupMethod::UseCurrentWindow); }
		virtual bool OnVisualizeTooltip(const TSharedPtr<SWidget>& Content) override { return true; }
	};

	class SHouseEnumPicker : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SHouseEnumPicker) {} SLATE_END_ARGS()
		void Construct(const FArguments&, TSharedRef<IPropertyHandle> InHandle, UEnum* InEnum)
		{
			Handle = InHandle;
			Enum = InEnum;
			for (int32 I = 0; I < Enum->NumEnums() - 1; ++I)
				if (!Enum->HasMetaData(TEXT("Hidden"), I)) Values.Add(MakeShared<int64>(Enum->GetValueByIndex(I)));
			ChildSlot
			[
				SNew(SHousePopupScope)
				[
					SAssignNew(Combo, SComboBox<TSharedPtr<int64>>)
					.Method(EPopupMethod::UseCurrentWindow)
					.OptionsSource(&Values)
					.OnComboBoxOpening_Lambda([this] { SyncSelection(); })
					.IsEnabled_Lambda([this] { return Handle->IsEditable(); })
					.OnGenerateWidget_Lambda([this](TSharedPtr<int64> Value) -> TSharedRef<SWidget>
					{
						return SNew(SHousePopupScope)[SNew(STextBlock).Text(Enum->GetDisplayNameTextByValue(*Value)).Font(IPropertyTypeCustomizationUtils::GetRegularFont())];
					})
					.OnSelectionChanged_Lambda([this](TSharedPtr<int64> Value, ESelectInfo::Type Reason)
					{
						if (Value.IsValid() && Reason != ESelectInfo::Direct) Handle->SetValue(static_cast<uint8>(*Value));
					})
					[
						SNew(STextBlock).Font(IPropertyTypeCustomizationUtils::GetRegularFont()).Text_Lambda([this]
						{
							uint8 Value = 0;
							return Handle->GetValue(Value) == FPropertyAccess::Success ? Enum->GetDisplayNameTextByValue(Value) : NSLOCTEXT("HouseDetails", "Multiple", "Multiple Values");
						})
					]
				]
			];
		}
#if WITH_DEV_AUTOMATION_TESTS
		TSharedPtr<SComboBox<TSharedPtr<int64>>> GetComboForTest() { SyncSelection(); return Combo; }
#endif
	private:
		void SyncSelection()
		{
			uint8 Current = 0;
			if (Handle->GetValue(Current) == FPropertyAccess::Success)
				for (const auto& Value : Values) if (*Value == Current) { Combo->SetSelectedItem(Value); return; }
			Combo->ClearSelection();
		}
		TSharedPtr<SComboBox<TSharedPtr<int64>>> Combo;
		TSharedPtr<IPropertyHandle> Handle;
		UEnum* Enum = nullptr;
		TArray<TSharedPtr<int64>> Values;
	};

	class FHouseDetails : public IPropertyTypeCustomization
	{
	public:
		static TSharedRef<IPropertyTypeCustomization> Make() { return MakeShared<FHouseDetails>(); }
		virtual void CustomizeHeader(TSharedRef<IPropertyHandle> Handle, FDetailWidgetRow& Row, IPropertyTypeCustomizationUtils&) override
		{
			Row.NameContent()[SNew(SHousePopupScope)[Handle->CreatePropertyNameWidget()]];
		}
		virtual void CustomizeChildren(TSharedRef<IPropertyHandle> Handle, IDetailChildrenBuilder& Builder, IPropertyTypeCustomizationUtils&) override
		{
			uint32 Count = 0; Handle->GetNumChildren(Count);
			for (uint32 I = 0; I < Count; ++I)
			{
				TSharedPtr<IPropertyHandle> Child = Handle->GetChildHandle(I);
				if (!Child.IsValid()) continue;
				IDetailPropertyRow& Row = Builder.AddProperty(Child.ToSharedRef());
				const FEnumProperty* Property = CastField<FEnumProperty>(Child->GetProperty());
				if (!Property) continue;
				// Retain the property row so edit conditions, reset, notifications and undo still use UE's property system.
				Row.CustomWidget()
				.NameContent()[SNew(SHousePopupScope)[Child->CreatePropertyNameWidget()]]
				.ValueContent().MinDesiredWidth(180).MaxDesiredWidth(400)
				[SNew(SHouseEnumPicker, Child.ToSharedRef(), Property->GetEnum())];
			}
		}
	};
	const FName Types[] = {TEXT("HouseNavigationSettings"), TEXT("HouseholdSettings"), TEXT("HouseRoomOverride"), TEXT("HouseFurnitureRule")};
}

void RegisterHouseDetails()
{
	if (IsRunningCommandlet()) return;
	FPropertyEditorModule& Module = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	for (FName Type : Types) Module.RegisterCustomPropertyTypeLayout(Type, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FHouseDetails::Make));
	Module.NotifyCustomizationModuleChanged();
}

void UnregisterHouseDetails()
{
	if (FPropertyEditorModule* Module = FModuleManager::GetModulePtr<FPropertyEditorModule>(TEXT("PropertyEditor")))
		for (FName Type : Types) Module->UnregisterCustomPropertyTypeLayout(Type);
}
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Gameplay/Procedural/HouseLayout.h"
#include "IPropertyRowGenerator.h"
#include "IDetailTreeNode.h"
#include "UObject/StructOnScope.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SVirtualWindow.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseInlineMenusTest, "Prototype.House.EditorInlineMenus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseInlineMenusTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("Slate initialized"), FSlateApplication::IsInitialized())) return false;
	FPropertyEditorModule& Module = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	auto Data = MakeShared<FStructOnScope>(FHouseGenerationSettings::StaticStruct());
	auto Rows = Module.CreatePropertyRowGenerator(FPropertyRowGeneratorArgs());
	Rows->SetStructure(Data);
	TSharedPtr<SWidget> ValueWidget;
	TSharedPtr<IPropertyHandle> Pattern;
	TFunction<void(TSharedRef<IDetailTreeNode>)> FindPattern = [&](TSharedRef<IDetailTreeNode> Node)
	{
		auto Handle = Node->CreatePropertyHandle();
		if (Handle.IsValid() && Handle->GetProperty() && Handle->GetProperty()->GetFName() == TEXT("Pattern"))
		{ Pattern = Handle; ValueWidget = Node->CreateNodeWidgets().ValueWidget; return; }
		TArray<TSharedRef<IDetailTreeNode>> Children; Node->GetChildren(Children, true);
		for (auto Child : Children) FindPattern(Child);
	};
	for (auto Root : Rows->GetRootTreeNodes()) FindPattern(Root);
	if (!TestTrue(TEXT("Pattern property widget exists"), ValueWidget.IsValid())) return false;
	TSharedPtr<SHouseEnumPicker> Picker;
	TFunction<void(TSharedRef<SWidget>)> FindPicker = [&](TSharedRef<SWidget> Widget)
	{
		if (Widget->GetTypeAsString() == TEXT("SHouseEnumPicker")) { Picker = StaticCastSharedRef<SHouseEnumPicker>(Widget); return; }
		FChildren* Children = Widget->GetChildren();
		for (int32 I = 0; I < Children->Num(); ++I) FindPicker(Children->GetChildAt(I));
	};
	FindPicker(ValueWidget.ToSharedRef());
	if (!TestTrue(TEXT("House customization supplies the inline enum picker"), Picker.IsValid())) return false;
	auto& Slate = FSlateApplication::Get();
	const int32 WindowCount = Slate.GetTopLevelWindows().Num();
	auto Window = SNew(SVirtualWindow).Size(FVector2D(640, 480));
	Window->SetContent(ValueWidget.ToSharedRef());
	Slate.RegisterVirtualWindow(Window);
	Window->SlatePrepass(1.0f);
	for (uint8 Value = 0; Value < 3; ++Value)
	{
		Pattern->SetValue(Value);
		auto Combo = Picker->GetComboForTest();
		Combo->SetIsOpen(true);
		TestTrue(TEXT("Dropdown reuses owning window"), Combo->IsOpen() && Combo->GetMenuWindow() == Window);
		TestEqual(TEXT("No additional native window"), Slate.GetTopLevelWindows().Num(), WindowCount);
		TestTrue(TEXT("Selection reflects property updates"), Combo->GetSelectedItem().IsValid() && *Combo->GetSelectedItem() == Value);
		Combo->SetIsOpen(false);
	}
	Slate.UnregisterVirtualWindow(Window);
	return true;
}
#endif

#endif
