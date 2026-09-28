#pragma once
#include "CoreMinimal.h"
class UInventoryComponent;

/** Test fixtures only. Never mutates the saved bandage asset. */
namespace InventoryDemo
{
	bool Populate(UInventoryComponent* Inventory);
}
