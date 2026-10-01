#pragma once
#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "UObject/Object.h"
#include "InventoryInputSettings.generated.h"

// Drop keeps the old Remove slot so saved custom bindings remain valid.
enum class EInventoryControl : uint8 { Toggle, Grab, TurnLeft, TurnRight, Cancel, Drop, Add, RotateWithMouse, Count };

/** Local player preferences for the laboratory, stored separately from its temporary items. */
UCLASS(Config=GameUserSettings)
class PROTOTYPE3_API UInventoryInputSettings : public UObject
{
	GENERATED_BODY()
public:
	UInventoryInputSettings();
	virtual void PostInitProperties() override;
	virtual void PostReloadConfig(FProperty* PropertyThatWasLoaded) override;
	UPROPERTY(Config)
	bool bToggleGrab = false;
	UPROPERTY(Config)
	float TurnSpeed = 120;
	FKey GetKey(EInventoryControl Action) const;
	bool TrySetKey(EInventoryControl Action, FKey Key, FString& Error);
	void ResetDefaults();
	static FString Label(EInventoryControl Action);
	static bool Supports(EInventoryControl Action, FKey Key);
private:
	void ValidateSettings();
	UPROPERTY(Config)
	TArray<FKey> Keys;
};
