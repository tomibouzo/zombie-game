#include "Gameplay/Player/Inventory/ItemUseSettings.h"
UItemUseSettings::UItemUseSettings() { Keys = { EKeys::One, EKeys::Two, EKeys::Three }; }
FName UItemUseSettings::ItemType(int32 Slot)
{
	static const FName Types[] = { TEXT("Bandage"), TEXT("CannedBeans"), TEXT("WaterBottle") };
	return Slot >= 0 && Slot < ItemCount ? Types[Slot] : NAME_None;
}

FString UItemUseSettings::Label(int32 Slot)
{
	switch (Slot)
	{
	case 0: return TEXT("Bandage");
	case 1: return TEXT("Canned food");
	case 2: return TEXT("Water");
	default: return FString();
	}
}

FKey UItemUseSettings::MigrationKey(int32 Slot) const
{
	if (Slot < 0 || Slot >= ItemCount) return EKeys::Invalid;
	const FKey Defaults[] = { EKeys::One, EKeys::Two, EKeys::Three };
	const FKey OldDefaults[] = { EKeys::E, EKeys::F, EKeys::G };
	const int32 Index = ItemTypes.IsEmpty() ? Slot : ItemTypes.IndexOfByKey(ItemType(Slot));
	if (!Keys.IsValidIndex(Index)) return Defaults[Slot];
	return Index < 3 && Keys[Index] == OldDefaults[Index] ? Defaults[Slot] : Keys[Index];
}
