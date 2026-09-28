#include "UI/Inventory/InventoryDemoWidget.h"
#include "UI/Inventory/SInventoryPanel.h"
#include "UI/Inventory/InventoryDemoData.h"
#include "Gameplay/Player/Inventory/InventoryComponent.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SBox.h"

void UInventoryDemoWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	Inventory = NewObject<UInventoryComponent>(this, NAME_None, RF_Transient);
	if (!InventoryDemo::Populate(Inventory)) UE_LOG(LogTemp, Error, TEXT("Inventory demo could not load its bandage fixture."));
}

TSharedRef<SWidget> UInventoryDemoWidget::RebuildWidget()
{
	return SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
	[
		SNew(SBox).WidthOverride(1000).HeightOverride(650)
		[
			SAssignNew(Panel, SInventoryPanel).Inventory(Inventory)
			.OnClose(FSimpleDelegate::CreateWeakLambda(this, [this]() { OnClose.ExecuteIfBound(); }))
		]
	];
}

TSharedPtr<SWidget> UInventoryDemoWidget::GetInventoryFocusTarget() const
{
	return Panel;
}

void UInventoryDemoWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Panel.Reset();
}
