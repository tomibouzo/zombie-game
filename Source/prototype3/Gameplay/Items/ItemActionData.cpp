#include "Gameplay/Items/ItemActionData.h"

bool UHealingItemActionData::IsValidActionData() const
{
	return FMath::IsFinite(HealAmount) && HealAmount > 0.0 && FMath::IsFinite(UseSeconds) && UseSeconds > 0;
}

bool UIntentItemActionData::IsValidActionData() const
{
	const FGameplayTag Root = FGameplayTag::RequestGameplayTag(TEXT("Item.Action"), false);
	return Root.IsValid() && ActionTag != Root && ActionTag.MatchesTag(Root);
}
