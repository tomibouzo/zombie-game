#include "Gameplay/Items/ItemActionData.h"

bool UHealingItemActionData::IsValidActionData() const
{
	return FMath::IsFinite(HealAmount) && HealAmount > 0.0;
}
