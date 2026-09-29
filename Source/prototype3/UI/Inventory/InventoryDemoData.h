#pragma once
#include "CoreMinimal.h"
class UInventoryComponent;

/** Saved item definitions with provisional inventory profiles, plus geometry fixtures.
 * Never mutates the saved assets. Profile dimensions are logical test units, not SI sizes.
 */
namespace InventoryDemo
{
	bool Populate(UInventoryComponent* Inventory);
}
