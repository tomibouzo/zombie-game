#pragma once
#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "ItemUseSettings.generated.h"

/** Fixed quick-item catalog and read-only migration of old preferences.
 * Current bindings live exclusively in UInventoryInputSettings. */
UCLASS(Config=GameUserSettings)
class PROTOTYPE3_API UItemUseSettings : public UObject
{
	GENERATED_BODY()
public:
	UItemUseSettings();
	static constexpr int32 ItemCount = 3;
	static FName ItemType(int32 Slot);
	static FString Label(int32 Slot);
	FKey MigrationKey(int32 Slot) const;
private:
	UPROPERTY(Config) TArray<FKey> Keys;
	UPROPERTY(Config) TArray<FName> ItemTypes;
};
