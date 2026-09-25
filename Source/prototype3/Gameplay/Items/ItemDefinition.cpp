#include "Gameplay/Items/ItemDefinition.h"

#include "Gameplay/Items/ItemActionData.h"

bool UItemDefinition::IsValidDefinition() const
{
	const FGameplayTag ItemCategoryRoot = FGameplayTag::RequestGameplayTag(TEXT("Item.Category"), false);
	return !ItemId.IsNone()
		&& !DisplayName.IsEmpty()
		&& ItemCategoryRoot.IsValid()
		&& Category.MatchesTag(ItemCategoryRoot)
		&& FMath::IsFinite(MassKg)
		&& MassKg > 0.0
		&& MaxStackSize >= 1
		&& (!PrimaryAction || PrimaryAction->IsValidActionData())
		&& (!SecondaryAction || SecondaryAction->IsValidActionData());
}
