#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryDemoWidget.generated.h"

class UInventoryComponent;
class SInventoryPanel;
class UPlayerItemUseComponent;

UCLASS()
class PROTOTYPE3_API UInventoryDemoWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	FSimpleDelegate OnClose;
	void ConfigurePlayerInventory(UPlayerItemUseComponent* Component);
	UInventoryComponent* GetInventory() const { return Inventory; }
	virtual void NativeOnInitialized() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	TSharedPtr<SWidget> GetInventoryFocusTarget() const;
	void CancelInteraction();
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
private:
	UPROPERTY(Transient) TObjectPtr<UPlayerItemUseComponent> ItemUse;
	UPROPERTY(Transient)
	TObjectPtr<UInventoryComponent> Inventory;
	TSharedPtr<SInventoryPanel> Panel;
};
