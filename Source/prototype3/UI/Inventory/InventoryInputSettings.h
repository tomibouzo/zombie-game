#pragma once
#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "UObject/Object.h"
#include "InventoryInputSettings.generated.h"

// Keep retired slots in place so older saved arrays retain their action identities.
enum class EInventoryControl : uint8
{
	Toggle, Grab, TurnLeft, TurnRight, Cancel, Drop, Add, RotateWithMouse,
	ShowQuick, OpenBackpack, ToggleBackpack, ToHands, ToQuick, ToBackpack, Stow,
	AssignShortcut1, AssignShortcut2, AssignShortcut3, ToggleGrabMode,
	MoveForward, MoveBackward, MoveLeft, MoveRight, Run, Sprint, Crouch,
	Primary, Secondary, Bandage, Food, Water, CyclePocket, FasterRotation,
	SlowerRotation, ScrollFloorUp, ScrollFloorDown, Back, PickUpWorld, DropHeld, Count
};

enum class EControlSection : uint8 { Movement, ItemActions, Inventory, Gameplay };

/** Saved keyboard/mouse controls. The legacy Add slot is used only by the isolated laboratory. */
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
	bool bAllowItemReplace = false;
	UPROPERTY(Config)
	float TurnSpeed = 120;
	UPROPERTY(Config) float LookSensitivity = 1.f;
	FKey GetKey(EInventoryControl Action, int32 Slot = 0) const;
	bool Matches(EInventoryControl Action, FKey Key) const;
	FString KeyLabel(EInventoryControl Action) const;
	static FString ShortKeyLabel(FKey Key);
	void CopySettingsFrom(const UInventoryInputSettings& Other);
	bool HasSameSettings(const UInventoryInputSettings& Other) const;
	bool TrySetKey(EInventoryControl Action, FKey Key, FString& Error);
	bool AssignKey(EInventoryControl Action, int32 Slot, FKey Key, bool bReplace, FString& Error);
	TArray<EInventoryControl> Conflicts(EInventoryControl Action, FKey Key) const;
	void ResetDefaults();
	bool ResetSection(EControlSection Section);
	void SetTurnSpeed(float Value);
	static FString Label(EInventoryControl Action);
	static FString HoverDescription(EInventoryControl Action);
	static EControlSection Section(EInventoryControl Action);
	static uint32 Contexts(EInventoryControl Action);
	static bool IsBindable(EInventoryControl Action);
	static bool IsGameplayControl(EInventoryControl Action);
	static bool IsGameplayReserved(FKey Key);
	static bool Supports(EInventoryControl Action, FKey Key);
private:
	void ValidateSettings();
	UPROPERTY(Config)
	TArray<FKey> Keys;
	UPROPERTY(Config) TArray<FKey> AlternateKeys;
	UPROPERTY(Config) int32 ControlsVersion = 0;
};
