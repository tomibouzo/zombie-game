#include "UI/Inventory/InventoryDemoData.h"
#include "Gameplay/Player/Inventory/InventoryComponent.h"
#include "Gameplay/Items/ItemDefinition.h"

namespace
{
FInventoryShapePart Rectangle(double Left, double Top, double Right, double Bottom)
{
	FInventoryShapePart Part;
	Part.Vertices = { FVector2D(Left, Top), FVector2D(Right, Top), FVector2D(Right, Bottom), FVector2D(Left, Bottom) };
	return Part;
}
}

bool InventoryDemo::Populate(UInventoryComponent* Inventory)
{
	UItemDefinition* Bandage = LoadObject<UItemDefinition>(nullptr, TEXT("/Game/Items/DA_Item_Bandage.DA_Item_Bandage"));
	if (!Inventory || !Bandage || !Bandage->IsValidDefinition()) return false;
	FInventoryPocket Main;
	Main.Id = TEXT("Main"); Main.Size = FVector2D(560, 560);
	if (Inventory->AddPocket(Main) != EInventoryResult::Success) return false;
	FInventoryItemProfile BandageProfile;
	BandageProfile.Id = TEXT("Bandage_TestOnly");
	BandageProfile.Definition = Bandage;
	FInventoryShapePart BandageShape;
	BandageShape.Vertices = { FVector2D(-18,-17), FVector2D(18,-17), FVector2D(24,-11), FVector2D(24,11),
		FVector2D(18,17), FVector2D(-18,17), FVector2D(-24,11), FVector2D(-24,-11) };
	BandageProfile.ShapeParts = { BandageShape };
	if (Inventory->RegisterProfile(BandageProfile) != EInventoryResult::Success) return false;
	if (Inventory->AddItem(FItemInstance::Create(Bandage), BandageProfile.Id, Main.Id, FVector2D(70,70), 0) != EInventoryResult::Success) return false;
	if (Inventory->AddItem(FItemInstance::Create(Bandage), BandageProfile.Id, Main.Id, FVector2D(260,95), 25) != EInventoryResult::Success) return false;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		UItemDefinition* Fixture = NewObject<UItemDefinition>(Inventory, NAME_None, RF_Transient);
		Fixture->ItemId = Index == 0 ? TEXT("Demo_L") : Index == 1 ? TEXT("Demo_Bar") : TEXT("Demo_Frame");
		Fixture->DisplayName = FText::FromString(Index == 0 ? TEXT("L piece") : Index == 1 ? TEXT("Bar") : TEXT("Frame"));
		Fixture->Category = Bandage->Category;
		Fixture->MassKg = 1;
		FInventoryItemProfile Profile;
		Profile.Id = Fixture->ItemId;
		Profile.Definition = Fixture;
		FVector2D Center;
		double Angle = 0;
		if (Index == 0)
		{
			Profile.ShapeParts = { Rectangle(-70,-70,-30,70), Rectangle(-30,30,70,70) };
			Center = FVector2D(170,250);
		}
		else if (Index == 1)
		{
			Profile.ShapeParts = { Rectangle(-100,-18,100,18) };
			Center = FVector2D(380,370);
			Angle = 337;
		}
		else
		{
			Profile.ShapeParts = { Rectangle(-60,-60,60,-42), Rectangle(-60,42,60,60),
				Rectangle(-60,-42,-42,42), Rectangle(42,-42,60,42) };
			Center = FVector2D(420,155);
		}
		if (Inventory->RegisterProfile(Profile) != EInventoryResult::Success) return false;
		if (Inventory->AddItem(FItemInstance::Create(Fixture), Profile.Id, Main.Id, Center, Angle) != EInventoryResult::Success) return false;
	}
	return true;
}
