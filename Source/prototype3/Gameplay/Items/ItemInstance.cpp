#include "Gameplay/Items/ItemInstance.h"

#include "Gameplay/Items/ItemDefinition.h"

FItemInstance FItemInstance::Create(UItemDefinition* InDefinition, int32 InQuantity)
{
	FItemInstance Result;
	if (::IsValid(InDefinition) && InDefinition->IsValidDefinition()
		&& InQuantity >= 1 && InQuantity <= InDefinition->MaxStackSize)
	{
		Result.InstanceId = FGuid::NewGuid();
		Result.Definition = InDefinition;
		Result.Quantity = InQuantity;
	}
	return Result;
}

bool FItemInstance::IsValid() const
{
	return InstanceId.IsValid()
		&& ::IsValid(Definition.Get())
		&& Definition->IsValidDefinition()
		&& Quantity >= 1
		&& Quantity <= Definition->MaxStackSize;
}
