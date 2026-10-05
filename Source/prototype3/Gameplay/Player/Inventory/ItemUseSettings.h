#pragma once
#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "ItemUseSettings.generated.h"

/** Personal quick-access choices; independent of inventory manipulation controls. */
UCLASS(Config=GameUserSettings)
class PROTOTYPE3_API UItemUseSettings : public UObject
{
	GENERATED_BODY()
public:
	UItemUseSettings();
	UPROPERTY(Config) TArray<FKey> Keys;
	UPROPERTY(Config) TArray<FName> ItemTypes;
	bool TryBind(int32 Slot, FKey Key, FKey InventoryToggle);
	void Normalize();
};
