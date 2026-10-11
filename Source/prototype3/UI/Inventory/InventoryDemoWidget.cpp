#include "UI/Inventory/InventoryDemoWidget.h"
#include "UI/Inventory/SInventoryPanel.h"
#include "UI/Inventory/InventoryDemoData.h"
#include "Gameplay/Player/Inventory/InventoryComponent.h"
#include "Gameplay/Items/World/DroppedItem.h"
#include "Gameplay/Player/Inventory/PlayerItemUseComponent.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SBox.h"

void UInventoryDemoWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
}

void UInventoryDemoWidget::ConfigurePlayerInventory(UPlayerItemUseComponent* Component)
{
	ItemUse = Component;
	Inventory = Component ? Component->Inventory.Get() : nullptr;
}

TSharedRef<SWidget> UInventoryDemoWidget::RebuildWidget()
{
	if (!Inventory)
	{
		Inventory = NewObject<UInventoryComponent>(this, NAME_None, RF_Transient);
		if (!InventoryDemo::Populate(Inventory)) UE_LOG(LogTemp, Error, TEXT("Inventory demo could not load its sample items."));
	}
	return SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
	[
		SNew(SBox).WidthOverride(1780).HeightOverride(1040).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(1540).HeightOverride(940)
			[
			SAssignNew(Panel, SInventoryPanel).Inventory(Inventory).ItemUse(ItemUse).Player(GetOwningPlayerPawn()).PocketsOnly(bPocketsOnly)
			.OnClose(FSimpleDelegate::CreateWeakLambda(this, [this]() { OnClose.ExecuteIfBound(); }))
			.OnDropItem(SInventoryPanel::FOnDropItem::CreateWeakLambda(this, [this](FGuid Id, FString& Error)
			{
				return ADroppedItem::DropFromInventory(Inventory, Id, GetOwningPlayerPawn(), Error) != nullptr;
			}))
			]
		]
	];
}

TSharedPtr<SWidget> UInventoryDemoWidget::GetInventoryFocusTarget() const
{
	return Panel;
}

void UInventoryDemoWidget::CancelInteraction()
{
	if (Panel.IsValid()) Panel->CancelInteraction();
}

void UInventoryDemoWidget::SetPocketsOnly(bool bOnly)
{
	bPocketsOnly = bOnly;
	if (Panel.IsValid()) Panel->SetPocketsOnly(bOnly);
}

bool UInventoryDemoWidget::IsPocketsOnly() const { return Panel.IsValid() ? Panel->IsPocketsOnly() : bPocketsOnly; }

void UInventoryDemoWidget::CycleQuickPocket() { if (Panel.IsValid()) Panel->CycleQuickPocket(); }

void UInventoryDemoWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Panel.Reset();
}
