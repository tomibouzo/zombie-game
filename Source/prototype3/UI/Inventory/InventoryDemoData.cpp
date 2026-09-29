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

FInventoryShapePart Polygon(std::initializer_list<FVector2D> Vertices)
{
	FInventoryShapePart Part;
	for (FVector2D Vertex : Vertices) Part.Vertices.Add(Vertex);
	return Part;
}

// Provisional logical units only. A backpack item does not own this test pocket.
bool AddSample(UInventoryComponent* Inventory, const TCHAR* ItemId, FVector2D Center,
	TArray<FInventoryShapePart> ShapeParts)
{
	const FString AssetPath = FString::Printf(TEXT("/Game/Items/DA_Item_%s.DA_Item_%s"), ItemId, ItemId);
	UItemDefinition* Definition = LoadObject<UItemDefinition>(nullptr, *AssetPath);
	if (!Definition || !Definition->IsValidDefinition())
	{
		UE_LOG(LogTemp, Error, TEXT("Inventory sample definition is missing or invalid: %s"), *AssetPath);
		return false;
	}
	FInventoryItemProfile Profile;
	Profile.Id = FName(FString(ItemId) + TEXT("_TestOnly"));
	Profile.Definition = Definition;
	Profile.ShapeParts = MoveTemp(ShapeParts);
	Profile.bProvisional = true;
	if (Inventory->RegisterProfile(Profile) != EInventoryResult::Success) return false;
	return Inventory->AddItem(FItemInstance::Create(Definition), Profile.Id, TEXT("Main"), Center, 0)
		== EInventoryResult::Success;
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
	return AddSample(Inventory, TEXT("CannedBeans"), FVector2D(150,60),
		{ Polygon({ {-16,-26}, {16,-26}, {21,-20}, {21,20}, {16,26}, {-16,26}, {-21,20}, {-21,-20} }) })
		&& AddSample(Inventory, TEXT("WaterBottle"), FVector2D(340,60),
			{ Rectangle(-7,-36,7,-25), Polygon({ {-7,-25}, {7,-25}, {14,-16}, {14,36}, {-14,36}, {-14,-16} }) })
		&& AddSample(Inventory, TEXT("Knife"), FVector2D(480,55),
			{ Rectangle(-46,-6,-10,6), Polygon({ {-10,-8}, {30,-8}, {46,0}, {-10,8} }) })
		&& AddSample(Inventory, TEXT("Pistol"), FVector2D(280,200),
			{ Rectangle(-39,-27,39,-9), Rectangle(-39,-9,-19,27) })
		&& AddSample(Inventory, TEXT("Flashlight"), FVector2D(285,310),
			{ Rectangle(-31,-7,13,7), Polygon({ {13,-7}, {23,-11}, {31,-11}, {31,11}, {23,11}, {13,7} }) })
		&& AddSample(Inventory, TEXT("Jacket"), FVector2D(65,405),
			{ Rectangle(-42,-40,42,-16), Rectangle(-24,-16,24,40) })
		&& AddSample(Inventory, TEXT("SmallBackpack"), FVector2D(155,465),
			{ Polygon({ {-26,-50}, {26,-50}, {40,-34}, {40,40}, {30,50}, {-30,50}, {-40,40}, {-40,-34} }) })
		&& AddSample(Inventory, TEXT("ScrapMetal"), FVector2D(465,480),
			{ Polygon({ {-25,-12}, {5,-20}, {25,0}, {12,20}, {-20,15} }) });
}
