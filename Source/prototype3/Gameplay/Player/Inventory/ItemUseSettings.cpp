#include "Gameplay/Player/Inventory/ItemUseSettings.h"
UItemUseSettings::UItemUseSettings()
{
	Keys = { EKeys::E, EKeys::F, EKeys::G };
	ItemTypes = { FName(TEXT("Bandage")), FName(TEXT("CannedBeans")), NAME_None };
}
void UItemUseSettings::Normalize()
{
	if (Keys.Num() != 3) Keys = { EKeys::E, EKeys::F, EKeys::G };
	if (ItemTypes.Num() != 3) ItemTypes = { FName(TEXT("Bandage")), FName(TEXT("CannedBeans")), NAME_None };
}
bool UItemUseSettings::TryBind(int32 Slot, FKey Key, FKey InventoryToggle)
{
	Normalize();
	if (!Keys.IsValidIndex(Slot) || !Key.IsValid() || Key.IsGamepadKey() || Key.IsMouseButton()
		|| Key == InventoryToggle || Key == EKeys::MouseScrollUp || Key == EKeys::MouseScrollDown
		|| Key == EKeys::W || Key == EKeys::A || Key == EKeys::S || Key == EKeys::D
		|| Key == EKeys::LeftShift || Key == EKeys::RightShift || Key == EKeys::LeftAlt || Key == EKeys::RightAlt
		|| Key == EKeys::LeftControl || Key == EKeys::RightControl || Key == EKeys::SpaceBar || Key == EKeys::Escape) return false;
	for (int32 I = 0; I < Keys.Num(); ++I) if (I != Slot && Keys[I] == Key) return false;
	Keys[Slot] = Key;
	return true;
}
