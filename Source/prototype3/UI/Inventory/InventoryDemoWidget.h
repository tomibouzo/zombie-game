#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryDemoWidget.generated.h"

class UInventoryComponent;
class SInventoryPanel;

UCLASS()
class PROTOTYPE3_API UInventoryDemoWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	FSimpleDelegate OnClose;
	virtual void NativeOnInitialized() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	TSharedPtr<SWidget> GetInventoryFocusTarget() const;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
private:
	UPROPERTY(Transient)
	TObjectPtr<UInventoryComponent> Inventory;
	TSharedPtr<SInventoryPanel> Panel;
};
