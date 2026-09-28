#include "UI/Inventory/InventoryDemoData.h"
#include "Gameplay/Player/Inventory/InventoryComponent.h"
#include "Gameplay/Items/ItemDefinition.h"

bool InventoryDemo::Populate(UInventoryComponent* Inventory)
{
	UItemDefinition* Bandage = LoadObject<UItemDefinition>(nullptr, TEXT("/Game/Items/DA_Item_Bandage.DA_Item_Bandage"));
	if (!Inventory || !Bandage || !Bandage->IsValidDefinition()) return false;
	FInventoryPocket Main;
	Main.Id = TEXT("Main"); Main.Size = FIntPoint(8, 6);
	FInventoryPocket Side;
	Side.Id = TEXT("Side"); Side.Size = FIntPoint(4, 3);
	if (Inventory->AddPocket(Main) != EInventoryResult::Success || Inventory->AddPocket(Side) != EInventoryResult::Success) return false;
	FInventoryItemProfile BandageProfile;
	BandageProfile.Id = TEXT("Bandage_TestOnly");
	BandageProfile.Definition = Bandage;
	BandageProfile.OccupiedCells = { FIntPoint(0, 0) };
	if (Inventory->RegisterProfile(BandageProfile) != EInventoryResult::Success) return false;
	if (Inventory->AddItem(FItemInstance::Create(Bandage), BandageProfile.Id, Main.Id, FIntPoint(0, 0), 0) != EInventoryResult::Success) return false;
	if (Inventory->AddItem(FItemInstance::Create(Bandage), BandageProfile.Id, Side.Id, FIntPoint(1, 1), 0) != EInventoryResult::Success) return false;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		UItemDefinition* Fixture = NewObject<UItemDefinition>(Inventory, NAME_None, RF_Transient);
		Fixture->ItemId = Index == 0 ? TEXT("Demo_L") : TEXT("Demo_Bar");
		Fixture->DisplayName = FText::FromString(Index == 0 ? TEXT("Pieza L (prueba)") : TEXT("Barra (prueba)"));
		Fixture->Category = Bandage->Category;
		Fixture->MassKg = 1.0;
		FInventoryItemProfile Profile;
		Profile.Id = Fixture->ItemId;
		Profile.Definition = Fixture;
		Profile.OccupiedCells = Index == 0
			? TArray<FIntPoint>{ FIntPoint(0, 0), FIntPoint(0, 1), FIntPoint(0, 2), FIntPoint(1, 2) }
			: TArray<FIntPoint>{ FIntPoint(0, 0), FIntPoint(1, 0), FIntPoint(2, 0) };
		if (Inventory->RegisterProfile(Profile) != EInventoryResult::Success) return false;
		if (Inventory->AddItem(FItemInstance::Create(Fixture), Profile.Id, Main.Id, Index == 0 ? FIntPoint(2, 1) : FIntPoint(4, 0), 0) != EInventoryResult::Success) return false;
	}
	return true;
}
