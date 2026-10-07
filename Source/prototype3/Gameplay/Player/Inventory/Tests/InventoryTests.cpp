#include "Gameplay/Player/Inventory/InventoryComponent.h"
#include "Gameplay/Items/ItemDefinition.h"
#include "UI/Inventory/InventoryDemoData.h"
#include "UI/Inventory/SInventoryPanel.h"
#include "UI/Inventory/InventoryInputSettings.h"
#include "Gameplay/Player/Inventory/ItemUseSettings.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/FileManager.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
FInventoryShapePart MakeTestRectangle(double L, double T, double R, double B)
{
	FInventoryShapePart Part;
	Part.Vertices = { FVector2D(L,T), FVector2D(R,T), FVector2D(R,B), FVector2D(L,B) };
	return Part;
}
struct FInventoryFixture
{
	TStrongObjectPtr<UInventoryComponent> Inventory { NewObject<UInventoryComponent>() };
	UItemDefinition* Definition;
	FInventoryFixture()
	{
		Definition = NewObject<UItemDefinition>(Inventory.Get());
		Definition->ItemId = TEXT("Fixture");
		Definition->DisplayName = FText::FromString(TEXT("Fixture"));
		Definition->Category = FGameplayTag::RequestGameplayTag(TEXT("Item.Category.Consumable.Medical"));
		Definition->MassKg = 1;
		FInventoryPocket Pocket;
		Pocket.Id = TEXT("A"); Pocket.Size = FVector2D(400,300);
		Inventory->AddPocket(Pocket);
		Pocket.Id = TEXT("B"); Inventory->AddPocket(Pocket);
		FInventoryItemProfile Profile;
		Profile.Id = TEXT("Single"); Profile.Definition = Definition;
		Profile.ShapeParts = { MakeTestRectangle(-10,-10,10,10) };
		Inventory->RegisterProfile(Profile);
		Profile.Id = TEXT("L");
		Profile.ShapeParts = { MakeTestRectangle(-50,-50,-30,50), MakeTestRectangle(-30,30,50,50) };
		Inventory->RegisterProfile(Profile);
		Profile.Id = TEXT("Frame");
		Profile.ShapeParts = { MakeTestRectangle(-50,-50,50,-30), MakeTestRectangle(-50,30,50,50), MakeTestRectangle(-50,-30,-30,30), MakeTestRectangle(30,-30,50,30) };
		Inventory->RegisterProfile(Profile);
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryGeometryTest, "Prototype.Inventory.Geometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInventoryGeometryTest::RunTest(const FString&)
{
	FInventoryFixture F;
	auto& I = *F.Inventory;
	const auto Item = FItemInstance::Create(F.Definition);
	TestTrue(TEXT("L placed"), I.AddItem(Item, TEXT("L"), TEXT("A"), FVector2D(100,100), 0) == EInventoryResult::Success);
	TestTrue(TEXT("Concave opening usable"), I.CheckPlacement(TEXT("Single"), TEXT("A"), FVector2D(110,80), 0) == EInventoryResult::Success);
	TestTrue(TEXT("Filled silhouette blocks"), I.CheckPlacement(TEXT("Single"), TEXT("A"), FVector2D(60,80), 0) == EInventoryResult::Occupied);
	TestTrue(TEXT("Border touching valid"), I.CheckPlacement(TEXT("Single"), TEXT("A"), FVector2D(80,80), 0) == EInventoryResult::Success);
	TestTrue(TEXT("Tiny penetration rejected"), I.CheckPlacement(TEXT("Single"), TEXT("A"), FVector2D(79.999,80), 0) == EInventoryResult::Occupied);
	TestTrue(TEXT("Exact storage edge valid"), I.CheckPlacement(TEXT("Single"), TEXT("B"), FVector2D(10,10), 0) == EInventoryResult::Success);
	for (FVector2D P : { FVector2D(9.99,40), FVector2D(40,9.99), FVector2D(390.01,40), FVector2D(40,290.01) })
		TestTrue(TEXT("Every storage edge rejects excess"), I.CheckPlacement(TEXT("Single"), TEXT("B"), P, 0) == EInventoryResult::OutOfBounds);
	TestTrue(TEXT("Rotation can push corners out"), I.CheckPlacement(TEXT("Single"), TEXT("B"), FVector2D(10,10), 45) == EInventoryResult::OutOfBounds);
	TestTrue(TEXT("Closed-hole frame placed"), I.AddItem(FItemInstance::Create(F.Definition), TEXT("Frame"), TEXT("B"), FVector2D(100,100), 0) == EInventoryResult::Success);
	TestTrue(TEXT("Interior hole usable"), I.CheckPlacement(TEXT("Single"), TEXT("B"), FVector2D(100,100), 32.57) == EInventoryResult::Success);
	FInventoryItemProfile Profile;
	I.GetProfile(TEXT("Frame"), Profile);
	TestFalse(TEXT("Hit testing ignores hole"), Profile.Contains(FVector2D(100,100), FVector2D(100,100), 32.57));
	TestTrue(TEXT("Hit testing includes material"), Profile.Contains(FVector2D(55,100), FVector2D(100,100), 0));
	TestTrue(TEXT("Whole rotation accepted"), I.CheckMove(Item.InstanceId, TEXT("A"), FVector2D(100,100), 360) == EInventoryResult::Success);
	Profile.Id = TEXT("Fixed"); Profile.bAllowRotation = false;
	I.RegisterProfile(Profile);
	TestTrue(TEXT("Fixed silhouette rejects rotation"), I.CheckPlacement(Profile.Id, TEXT("A"), FVector2D(300,150), 0.01) == EInventoryResult::InvalidRotation);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryContactTest, "Prototype.Inventory.RotatedContact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInventoryContactTest::RunTest(const FString&)
{
	// Analytic contact at many orientations catches world-AABB collision and rounding.
	for (double Angle : { 0., 0.13, 17.375, 45., 90., 179.97, 271.5, 359.91 })
	{
		FInventoryFixture F;
		auto& I = *F.Inventory;
		const auto Item = FItemInstance::Create(F.Definition);
		const FVector2D Center(200,150);
		const double R = FMath::DegreesToRadians(Angle);
		const FVector2D Axis(FMath::Cos(R), FMath::Sin(R));
		TestTrue(TEXT("Arbitrary angle placed"), I.AddItem(Item, TEXT("Single"), TEXT("A"), Center, Angle) == EInventoryResult::Success);
		TestTrue(TEXT("Rotated edges may touch"), I.CheckPlacement(TEXT("Single"), TEXT("A"), Center + Axis * 20, Angle) == EInventoryResult::Success);
		TestTrue(TEXT("Rotated small overlap rejected"), I.CheckPlacement(TEXT("Single"), TEXT("A"), Center + Axis * 19.999, Angle) == EInventoryResult::Occupied);
		TestTrue(TEXT("Rotated small gap accepted"), I.CheckPlacement(TEXT("Single"), TEXT("A"), Center + Axis * 20.001, Angle) == EInventoryResult::Success);
		const FVector2D Tangent(-Axis.Y, Axis.X);
		TestTrue(TEXT("Corner touch accepted"), I.CheckPlacement(TEXT("Single"), TEXT("A"), Center + (Axis + Tangent) * 20, Angle) == EInventoryResult::Success);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryAtomicMoveTest, "Prototype.Inventory.AtomicMoves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInventoryAtomicMoveTest::RunTest(const FString&)
{
	FInventoryFixture F;
	auto& I = *F.Inventory;
	const auto Moving = FItemInstance::Create(F.Definition), Obstacle = FItemInstance::Create(F.Definition);
	I.AddItem(Moving, TEXT("L"), TEXT("A"), FVector2D(100.125,100.75), 13.37);
	I.AddItem(Obstacle, TEXT("Single"), TEXT("B"), FVector2D(60,80), 0);
	TestTrue(TEXT("Collision rejects whole move"), I.MoveItem(Moving.InstanceId, TEXT("B"), FVector2D(100,100), 0) == EInventoryResult::Occupied);
	FInventoryEntry Found;
	I.GetItem(Moving.InstanceId, Found);
	TestEqual(TEXT("Original pocket"), Found.PocketId, FName(TEXT("A")));
	TestEqual(TEXT("Original precise position"), Found.Position, FVector2D(100.125,100.75));
	TestEqual(TEXT("Original angle"), Found.AngleDegrees, 13.37);
	TestEqual(TEXT("No duplicates"), I.GetEntries().Num(), 2);
	TestTrue(TEXT("Self overlap is ignored"), I.MoveItem(Moving.InstanceId, TEXT("A"), FVector2D(110.5,110.75), 25.23) == EInventoryResult::Success);
	TestTrue(TEXT("Atomic transfer"), I.MoveItem(Moving.InstanceId, TEXT("B"), FVector2D(300,200), -12.34) == EInventoryResult::Success);
	I.GetItem(Moving.InstanceId, Found);
	TestEqual(TEXT("ID preserved"), Found.Item.InstanceId, Moving.InstanceId);
	TestEqual(TEXT("Quantity preserved"), Found.Item.Quantity, Moving.Quantity);
	TestTrue(TEXT("Definition preserved"), Found.Item.Definition == Moving.Definition);
	TestTrue(TEXT("Angle normalized"), FMath::IsNearlyEqual(Found.AngleDegrees, 347.66, 1.e-8));
	TestTrue(TEXT("Duplicate identity rejected"), I.AddItem(Moving, TEXT("Single"), TEXT("A"), FVector2D(350,250), 0) == EInventoryResult::DuplicateId);
	FItemInstance Removed;
	TestTrue(TEXT("Removal returns instance"), I.RemoveItem(Moving.InstanceId, Removed) == EInventoryResult::Success);
	TestEqual(TEXT("Returned ID"), Removed.InstanceId, Moving.InstanceId);
	TestTrue(TEXT("Missing removal fails"), I.RemoveItem(Moving.InstanceId, Removed) == EInventoryResult::NotFound);
	TestFalse(TEXT("Failed removal clears stale output"), Removed.IsValid());
	TestTrue(TEXT("Missing move fails"), I.MoveItem(Moving.InstanceId, TEXT("A"), FVector2D(100), 0) == EInventoryResult::NotFound);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryValidationTest, "Prototype.Inventory.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInventoryValidationTest::RunTest(const FString&)
{
	FInventoryFixture F;
	auto& I = *F.Inventory;
	const double NaN = std::numeric_limits<double>::quiet_NaN();
	const double Infinity = std::numeric_limits<double>::infinity();
	FInventoryItemProfile Profile;
	I.GetProfile(TEXT("Single"), Profile);
	TestTrue(TEXT("Duplicate profile rejected"), I.RegisterProfile(Profile) == EInventoryResult::DuplicateId);
	Profile.Id = TEXT("Bad");
	Profile.ShapeParts.Empty();
	TestTrue(TEXT("Empty silhouette rejected"), I.RegisterProfile(Profile) == EInventoryResult::InvalidProfile);
	Profile.ShapeParts = { MakeTestRectangle(-10,-10,10,10) };
	Profile.ShapeParts[0].Vertices[0].X = NaN;
	TestFalse(TEXT("NaN geometry rejected"), Profile.IsValid());
	Profile.ShapeParts = { MakeTestRectangle(0,0,20,20) };
	TestFalse(TEXT("Off-center geometry rejected"), Profile.IsValid());
	Profile.ShapeParts = { MakeTestRectangle(-5000,-10,5000,10) };
	TestFalse(TEXT("Excessive extent rejected"), Profile.IsValid());
	Profile.ShapeParts = { MakeTestRectangle(-10,-10,10,10) };
	Profile.ShapeParts[0].Vertices[2] = FVector2D(0,-5);
	TestFalse(TEXT("Concave individual part rejected"), Profile.IsValid());
	Profile.ShapeParts = { MakeTestRectangle(-10,-10,10,10) };
	Swap(Profile.ShapeParts[0].Vertices[1], Profile.ShapeParts[0].Vertices[2]);
	TestFalse(TEXT("Crossed polygon rejected"), Profile.IsValid());
	Profile.ShapeParts = { MakeTestRectangle(-10,-10,10,10) };
	const FVector2D DuplicateVertex = Profile.ShapeParts[0].Vertices[0];
	Profile.ShapeParts[0].Vertices.Add(DuplicateVertex);
	TestFalse(TEXT("Duplicate vertex rejected"), Profile.IsValid());
	FInventoryPocket Pocket;
	TestTrue(TEXT("Unnamed pocket rejected"), I.AddPocket(Pocket) == EInventoryResult::InvalidPocket);
	Pocket.Id = TEXT("Bad"); Pocket.Size = FVector2D(NaN,100);
	TestTrue(TEXT("NaN pocket rejected"), I.AddPocket(Pocket) == EInventoryResult::InvalidPocket);
	Pocket.Size = FVector2D(0,100);
	TestTrue(TEXT("Zero pocket rejected"), I.AddPocket(Pocket) == EInventoryResult::InvalidPocket);
	Pocket.Id = TEXT("A"); Pocket.Size = FVector2D(100);
	TestTrue(TEXT("Duplicate pocket rejected"), I.AddPocket(Pocket) == EInventoryResult::DuplicateId);
	const auto Item = FItemInstance::Create(F.Definition);
	TestTrue(TEXT("Invalid item rejected"), I.AddItem(FItemInstance(), TEXT("Single"), TEXT("A"), FVector2D(100), 0) == EInventoryResult::InvalidItem);
	TestTrue(TEXT("Missing pocket rejected"), I.AddItem(Item, TEXT("Single"), TEXT("Missing"), FVector2D(100), 0) == EInventoryResult::InvalidPocket);
	TestTrue(TEXT("Missing profile rejected"), I.AddItem(Item, TEXT("Missing"), TEXT("A"), FVector2D(100), 0) == EInventoryResult::InvalidProfile);
	for (double Value : { NaN, Infinity, -Infinity })
	{
		TestTrue(TEXT("Nonfinite angle rejected"), I.CheckPlacement(TEXT("Single"), TEXT("A"), FVector2D(100), Value) == EInventoryResult::InvalidRotation);
		TestTrue(TEXT("Nonfinite position rejected"), I.CheckPlacement(TEXT("Single"), TEXT("A"), FVector2D(Value,100), 0) == EInventoryResult::OutOfBounds);
	}
	UItemDefinition* Other = DuplicateObject<UItemDefinition>(F.Definition, F.Inventory.Get());
	TestTrue(TEXT("Wrong definition rejected"), I.AddItem(FItemInstance::Create(Other), TEXT("Single"), TEXT("A"), FVector2D(100), 0) == EInventoryResult::InvalidProfile);
	TestEqual(TEXT("Failures added nothing"), I.GetEntries().Num(), 0);
	auto Pockets = I.GetPockets(); Pockets[0].Size = FVector2D(999);
	TestEqual(TEXT("Snapshots cannot mutate storage"), I.GetPockets()[0].Size, FVector2D(400,300));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryBandageTest, "Prototype.Inventory.BandageIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInventoryBandageTest::RunTest(const FString&)
{
	TStrongObjectPtr<UInventoryComponent> I(NewObject<UInventoryComponent>());
	if (!TestTrue(TEXT("Real asset and demo populate"), InventoryDemo::Populate(I.Get()))) return false;
	TestEqual(TEXT("Single larger space"), I->GetPockets().Num(), 1);
	TestEqual(TEXT("Square space"), I->GetPockets()[0].Size, FVector2D(560));
	const auto Entries = I->GetEntries();
	TestEqual(TEXT("Two bandages, eight samples and three geometry fixtures"), Entries.Num(), 13);
	FInventoryItemProfile Profile;
	I->GetProfile(TEXT("Bandage_TestOnly"), Profile);
	TestTrue(TEXT("Geometry explicitly provisional"), Profile.bProvisional);
	TestEqual(TEXT("Detailed bandage outline"), Profile.ShapeParts[0].Vertices.Num(), 8);
	TestEqual(TEXT("Stack limit unchanged"), Profile.Definition->MaxStackSize, 1);
	TestEqual(TEXT("Mass unchanged"), Profile.Definition->MassKg, 0.05);
	TestTrue(TEXT("Distinct identities"), Entries[0].Item.InstanceId != Entries[1].Item.InstanceId);
	TestTrue(TEXT("Bandage fits inside frame hole"), I->MoveItem(Entries[0].Item.InstanceId, TEXT("Main"), FVector2D(420,155), 34.79) == EInventoryResult::Success);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventorySampleIntegrationTest, "Prototype.Inventory.SampleIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInventorySampleIntegrationTest::RunTest(const FString&)
{
	TStrongObjectPtr<UInventoryComponent> Inventory(NewObject<UInventoryComponent>());
	if (!TestTrue(TEXT("All saved sample assets populate"), InventoryDemo::Populate(Inventory.Get()))) return false;
	const auto Entries = Inventory->GetEntries();
	const TCHAR* SampleIds[] = { TEXT("CannedBeans"), TEXT("WaterBottle"), TEXT("Knife"), TEXT("Pistol"),
		TEXT("Flashlight"), TEXT("Jacket"), TEXT("SmallBackpack"), TEXT("ScrapMetal") };
	for (const TCHAR* Id : SampleIds)
	{
		const FInventoryEntry* Entry = Entries.FindByPredicate([&](const FInventoryEntry& E) { return E.Item.Definition->ItemId == FName(Id); });
		if (!TestNotNull(FString(Id) + TEXT(" is present"), Entry)) continue;
		FInventoryItemProfile Profile;
		TestTrue(TEXT("Profile is registered"), Inventory->GetProfile(Entry->ProfileId, Profile));
		TestTrue(TEXT("Profile explicitly provisional"), Profile.bProvisional && Profile.IsValid());
		TestTrue(TEXT("Own placement remains valid"), Inventory->CheckMove(Entry->Item.InstanceId, Entry->PocketId, Entry->Position, Entry->AngleDegrees) == EInventoryResult::Success);
		TestTrue(TEXT("Cannot overlap bandage"), Inventory->CheckMove(Entry->Item.InstanceId, Entry->PocketId, FVector2D(70,70), 0) == EInventoryResult::Occupied);
		TestTrue(TEXT("Cannot protrude from pocket"), Inventory->CheckMove(Entry->Item.InstanceId, Entry->PocketId, FVector2D(0,0), 17) == EInventoryResult::OutOfBounds);
		TestTrue(FString(Id) + TEXT(" moves and rotates"), Inventory->MoveItem(Entry->Item.InstanceId, Entry->PocketId, FVector2D(300,490), 37) == EInventoryResult::Success);
		FInventoryEntry Moved;
		TestTrue(TEXT("Identity survives move"), Inventory->GetItem(Entry->Item.InstanceId, Moved));
		TestTrue(TEXT("Shared definition survives move"), Moved.Item.Definition == Entry->Item.Definition);
		TestEqual(TEXT("Quantity remains one"), Moved.Item.Quantity, 1);
		TestTrue(TEXT("Original placement restored"), Inventory->MoveItem(Entry->Item.InstanceId, Entry->PocketId, Entry->Position, Entry->AngleDegrees) == EInventoryResult::Success);
	}
	TestEqual(TEXT("Moves neither duplicate nor lose items"), Inventory->GetEntries().Num(), Entries.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryPanelInteractionTest, "Prototype.Inventory.PanelInteraction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInventoryPanelInteractionTest::RunTest(const FString&)
{
	TStrongObjectPtr<UInventoryComponent> I(NewObject<UInventoryComponent>());
	TStrongObjectPtr<UInventoryInputSettings> Settings(NewObject<UInventoryInputSettings>());
	Settings->ResetDefaults();
	if (!TestTrue(TEXT("Demo fixture"), InventoryDemo::Populate(I.Get()))) return false;
	const int32 InitialCount = I->GetEntries().Num();
	bool bClosed = false;
	FGuid RequestedDrop;
	const auto Panel = SNew(SInventoryPanel).Inventory(I.Get()).Controls(Settings.Get()).SaveControls(false)
		.OnClose(FSimpleDelegate::CreateLambda([&]() { bClosed = true; }))
		.OnDropItem(SInventoryPanel::FOnDropItem::CreateLambda([&](FGuid Selected, FString& Error)
		{
			RequestedDrop = Selected;
			Error = TEXT("World drop unavailable in this isolated panel test.");
			return false;
		}));
	const FGeometry G = FGeometry::MakeRoot(FVector2D(1000,800), FSlateLayoutTransform());
	auto Mouse = [](FVector2D P, FKey B) { return FPointerEvent(0, P, P, TSet<FKey>(), B, 0, FModifierKeysState()); };
	auto Down = [&](FVector2D P, FKey B = EKeys::LeftMouseButton) { Panel->OnMouseButtonDown(G, Mouse(P,B)); };
	auto Up = [&](FVector2D P, FKey B = EKeys::LeftMouseButton) { Panel->OnMouseButtonUp(G, Mouse(P,B)); };
	auto Move = [&](FVector2D P) { Panel->OnMouseMove(G, Mouse(P,EKeys::Invalid)); };
	auto Key = [&](FKey K) { Panel->OnKeyDown(G, FKeyEvent(K,FModifierKeysState(),0,false,0,0)); };
	auto KeyUp = [&](FKey K) { Panel->OnKeyUp(G, FKeyEvent(K,FModifierKeysState(),0,false,0,0)); };
	const FGuid Id = I->GetEntries()[0].Item.InstanceId;
	FInventoryEntry Entry;
	Down(FVector2D(87,203)); Up(FVector2D(300,600));
	I->GetItem(Id,Entry);
	TestEqual(TEXT("Transparent corner does not grab"), Entry.Position,FVector2D(70,70));
	Down(FVector2D(115,225)); Move(FVector2D(355.25,655.75));
	Key(EKeys::E); Panel->Tick(G,0,0.125f); KeyUp(EKeys::E);
	I->GetItem(Id,Entry);
	TestEqual(TEXT("Preview does not mutate original angle"), Entry.AngleDegrees,0.);
	Up(FVector2D(355.25,655.75),EKeys::MiddleMouseButton);
	Up(FVector2D(355.25,655.75));
	I->GetItem(Id,Entry);
	TestEqual(TEXT("Subpixel placement and grab offset preserved"), Entry.Position,FVector2D(310.25,500.75));
	TestEqual(TEXT("Continuous center rotation"), Entry.AngleDegrees,15.);
	const FVector2D Placed = Entry.Position;
	const FVector2D Pointer = Placed + FVector2D(40,150);
	Down(Pointer); Key(EKeys::E); Panel->Tick(G,0,0.25f); KeyUp(EKeys::E); Up(FVector2D(300,245));
	I->GetItem(Id,Entry);
	TestEqual(TEXT("Invalid collision restores position"), Entry.Position,Placed);
	TestEqual(TEXT("Invalid collision restores angle"), Entry.AngleDegrees,15.);
	Down(Pointer); Up(FVector2D(45,155));
	I->GetItem(Id,Entry); TestEqual(TEXT("Partial outside rejected"),Entry.Position,Placed);
	Down(Pointer); Move(FVector2D(300,600)); Key(EKeys::C); Up(FVector2D(300,600));
	I->GetItem(Id,Entry); TestEqual(TEXT("Cancel restores original"),Entry.Position,Placed);
	Down(Pointer); Key(EKeys::Q); Panel->OnFocusLost(FFocusEvent()); Panel->Tick(G,0,1); Up(FVector2D(300,600));
	I->GetItem(Id,Entry); TestEqual(TEXT("Focus loss cancels gesture"),Entry.Position,Placed);
	Down(Pointer); Panel->OnMouseCaptureLost(FCaptureLostEvent()); Up(FVector2D(300,600));
	I->GetItem(Id,Entry); TestEqual(TEXT("Capture loss cancels gesture"),Entry.Position,Placed);
	// The old settings column is inert; Options owns rebinding now.
	Down(FVector2D(800,240)); Up(FVector2D(800,240)); Key(EKeys::G); KeyUp(EKeys::G);
	TestTrue(TEXT("Former controls column cannot rebind"),Settings->GetKey(EInventoryControl::Grab)==EKeys::LeftMouseButton);
	FString Error;
	Settings->TrySetKey(EInventoryControl::Grab,EKeys::G,Error);
	Key(EKeys::V);
	TestFalse(TEXT("Retired mode hotkey cannot change the preference"),Settings->bToggleGrab);
	Settings->bToggleGrab = true; // The Options menu now owns this preference.
	Move(Pointer); Key(EKeys::G); KeyUp(EKeys::G); Move(FVector2D(340,630));
	I->GetItem(Id,Entry); TestEqual(TEXT("Toggle release keeps item held"),Entry.Position,Placed);
	Key(EKeys::G); KeyUp(EKeys::G);
	I->GetItem(Id,Entry); TestEqual(TEXT("Second press commits"),Entry.Position,FVector2D(300,480));
	Move(FVector2D(340,630)); Key(EKeys::G); KeyUp(EKeys::G);
	Panel->OnMouseWheel(G,FPointerEvent(0,FVector2D(340,630),FVector2D(340,630),TSet<FKey>(),EKeys::Invalid,1,FModifierKeysState()));
	TestEqual(TEXT("Wheel up increases shared rotation speed"),Settings->TurnSpeed,135.f);
	Key(EKeys::E); Panel->Tick(G,0,0.1f); KeyUp(EKeys::E);
	Panel->OnMouseWheel(G,FPointerEvent(0,FVector2D(340,630),FVector2D(340,630),TSet<FKey>(),EKeys::Invalid,-1,FModifierKeysState()));
	TestEqual(TEXT("Wheel down decreases shared rotation speed"),Settings->TurnSpeed,120.f);
	Key(EKeys::G); KeyUp(EKeys::G);
	I->GetItem(Id,Entry); TestTrue(TEXT("Faster Q/E turning uses wheel speed"),FMath::IsNearlyEqual(Entry.AngleDegrees,28.5,0.001));
	Key(EKeys::G); KeyUp(EKeys::G);
	Panel->OnMouseWheel(G,FPointerEvent(0,FVector2D(340,630),FVector2D(340,630),TSet<FKey>(),EKeys::Invalid,100,FModifierKeysState()));
	TestEqual(TEXT("Wheel speed maximum"),Settings->TurnSpeed,360.f);
	Panel->OnMouseWheel(G,FPointerEvent(0,FVector2D(340,630),FVector2D(340,630),TSet<FKey>(),EKeys::Invalid,-100,FModifierKeysState()));
	TestEqual(TEXT("Wheel speed minimum"),Settings->TurnSpeed,15.f);
	Key(EKeys::F);
	TestEqual(TEXT("Drop requests the selected item without holding"),RequestedDrop,Id);
	TestEqual(TEXT("Failed selected drop preserves inventory"),I->GetEntries().Num(),InitialCount);
	Move(FVector2D(340,630)); Key(EKeys::G); KeyUp(EKeys::G); Key(EKeys::F);
	TestEqual(TEXT("An unavailable world drop does not delete the held item"),I->GetEntries().Num(),InitialCount);
	I->GetItem(Id,Entry);
	TestEqual(TEXT("Failed drop restores the stored placement"),Entry.Position,FVector2D(300,480));
	Down(FVector2D(850,665)); Up(FVector2D(850,665));
	TestEqual(TEXT("Former Remove button has no deletion action"),I->GetEntries().Num(),InitialCount);
	Key(EKeys::B); Move(FVector2D(110,220)); Key(EKeys::G); KeyUp(EKeys::G);
	TestEqual(TEXT("Add through configured controls"),I->GetEntries().Num(),InitialCount + 1);
	Key(EKeys::I); TestTrue(TEXT("Close shortcut"),bClosed);
	bClosed = false;
	Move(FVector2D(340,630)); Key(EKeys::G); Move(FVector2D(800,400)); Key(EKeys::Escape);
	TestTrue(TEXT("Escape closes inventory during a drag"),bClosed);
	I->GetItem(Id,Entry);
	TestEqual(TEXT("Escape preserves the original item position"),Entry.Position,FVector2D(300,480));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryInputTest, "Prototype.Inventory.InputPreferences",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInventoryInputTest::RunTest(const FString&)
{
	TStrongObjectPtr<UInventoryInputSettings> Settings(NewObject<UInventoryInputSettings>());
	Settings->ResetDefaults();
	FString Error;
	TestFalse(TEXT("Conflicts rejected"),Settings->TrySetKey(EInventoryControl::Grab,EKeys::Q,Error));
	TestFalse(TEXT("Escape reserved for interfaces"),Settings->TrySetKey(EInventoryControl::Toggle,EKeys::Escape,Error));
	TestTrue(TEXT("Original survives conflict"),Settings->GetKey(EInventoryControl::Grab)==EKeys::LeftMouseButton);
	TestFalse(TEXT("Analog axis rejected"),Settings->TrySetKey(EInventoryControl::Grab,EKeys::MouseX,Error));
	TestFalse(TEXT("Wheel cannot be held for grab"),Settings->TrySetKey(EInventoryControl::Grab,EKeys::MouseScrollDown,Error));
	TestTrue(TEXT("Right mouse rotates by default"),Settings->GetKey(EInventoryControl::RotateWithMouse)==EKeys::RightMouseButton);
	TestTrue(TEXT("C cancels by default"),Settings->GetKey(EInventoryControl::Cancel)==EKeys::C);
	TestTrue(TEXT("F is the default drop key"),Settings->GetKey(EInventoryControl::Drop)==EKeys::F);
	TestEqual(TEXT("Drop replaces the old removal control"),UInventoryInputSettings::Label(EInventoryControl::Drop),FString(TEXT("Drop selected item")));
	TestFalse(TEXT("Wheel cannot hold mouse rotation"),Settings->TrySetKey(EInventoryControl::RotateWithMouse,EKeys::MouseScrollDown,Error));
	TestFalse(TEXT("Wheel reserved from turn bindings"),Settings->TrySetKey(EInventoryControl::TurnRight,EKeys::MouseScrollUp,Error));
	TestTrue(TEXT("Mouse rotation supported"),Settings->TrySetKey(EInventoryControl::TurnLeft,EKeys::ThumbMouseButton,Error));
	TestTrue(TEXT("Mouse rotation modifier configurable"),Settings->TrySetKey(EInventoryControl::RotateWithMouse,EKeys::Z,Error));
	TestTrue(TEXT("Open control configurable"),Settings->TrySetKey(EInventoryControl::Toggle,EKeys::K,Error));
	TestTrue(TEXT("Hidden laboratory binding does not reserve B"),Settings->TrySetKey(EInventoryControl::Grab,EKeys::B,Error));
	Settings->bToggleGrab = true;
	Settings->TurnSpeed = 75;
	TestTrue(TEXT("Second binding defaults empty"),!Settings->GetKey(EInventoryControl::Sprint,1).IsValid());
	Settings->AssignKey(EInventoryControl::Sprint,1,EKeys::J,false,Error);
	Settings->TrySetKey(EInventoryControl::Bandage,EKeys::Invalid,Error);
	TestEqual(TEXT("Unbound action remains in hint text"),Settings->KeyLabel(EInventoryControl::Bandage),FString(TEXT("Unbound")));
	// Round-trip to an isolated file, never overwrite the player's own preferences.
	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("InventoryVerification/input-test.ini"));
	Settings->SaveConfig(CPF_Config,*Path);
	TStrongObjectPtr<UInventoryInputSettings> Reloaded(NewObject<UInventoryInputSettings>());
	Reloaded->ResetDefaults();
	Reloaded->LoadConfig(nullptr,*Path);
	TestTrue(TEXT("Binding survives reload"),Reloaded->GetKey(EInventoryControl::Toggle)==EKeys::K);
	TestTrue(TEXT("Mouse binding survives reload"),Reloaded->GetKey(EInventoryControl::TurnLeft)==EKeys::ThumbMouseButton);
	TestTrue(TEXT("Rotation modifier survives reload"),Reloaded->GetKey(EInventoryControl::RotateWithMouse)==EKeys::Z);
	TestTrue(TEXT("Grab mode survives reload"),Reloaded->bToggleGrab);
	TestTrue(TEXT("Reclaimed B assignment survives reload"),Reloaded->GetKey(EInventoryControl::Grab)==EKeys::B);
	TestEqual(TEXT("Speed survives reload"),Reloaded->TurnSpeed,75.f);
	TestTrue(TEXT("Second binding survives reload"),Reloaded->GetKey(EInventoryControl::Sprint,1)==EKeys::J);
	TestFalse(TEXT("Explicit unbound survives reload"),Reloaded->GetKey(EInventoryControl::Bandage).IsValid());
	const TCHAR* Section = TEXT("/Script/prototype3.InventoryInputSettings");
	GConfig->SetInt(Section,TEXT("ControlsVersion"),0,Path);
	TArray<FString> LegacyKeys { TEXT("K"), TEXT("LeftMouseButton"), TEXT("Q"), TEXT("E"), TEXT("RightMouseButton"), TEXT("Delete"), TEXT("B") };
	GConfig->SetArray(Section, TEXT("Keys"), LegacyKeys, Path);
	Reloaded->ReloadConfig(nullptr, *Path);
	TestTrue(TEXT("Legacy toggle preserved"),Reloaded->GetKey(EInventoryControl::Toggle)==EKeys::K);
	TestTrue(TEXT("Legacy cancel moves to C"),Reloaded->GetKey(EInventoryControl::Cancel)==EKeys::C);
	TestTrue(TEXT("Legacy settings gain right-mouse rotation"),Reloaded->GetKey(EInventoryControl::RotateWithMouse)==EKeys::RightMouseButton);
	TestTrue(TEXT("Legacy grab mode preserved"),Reloaded->bToggleGrab);
	TestEqual(TEXT("Legacy speed preserved"),Reloaded->TurnSpeed,75.f);
	LegacyKeys[5] = TEXT("X");
	GConfig->SetArray(Section, TEXT("Keys"), LegacyKeys, Path);
	Reloaded->ReloadConfig(nullptr, *Path);
	TestTrue(TEXT("Legacy custom removal binding becomes Drop"),Reloaded->GetKey(EInventoryControl::Drop)==EKeys::X);
	// An existing custom middle-button binding must survive migration without duplicates.
	LegacyKeys[2] = TEXT("MiddleMouseButton");
	GConfig->SetArray(Section, TEXT("Keys"), LegacyKeys, Path);
	Reloaded->ReloadConfig(nullptr, *Path);
	TestTrue(TEXT("Custom middle binding preserved"),Reloaded->GetKey(EInventoryControl::TurnLeft)==EKeys::MiddleMouseButton);
	TestTrue(TEXT("Cancel uses C"),Reloaded->GetKey(EInventoryControl::Cancel)==EKeys::C);
	TestTrue(TEXT("Right mouse still available for rotation"),Reloaded->GetKey(EInventoryControl::RotateWithMouse)==EKeys::RightMouseButton);
	// Existing wheel turn bindings migrate individually without resetting other preferences.
	TArray<FString> WheelKeys { TEXT("K"), TEXT("LeftMouseButton"), TEXT("MouseScrollDown"), TEXT("MouseScrollUp"), TEXT("Escape"), TEXT("Delete"), TEXT("B"), TEXT("R") };
	GConfig->SetArray(Section, TEXT("Keys"), WheelKeys, Path);
	Reloaded->ReloadConfig(nullptr, *Path);
	TestTrue(TEXT("Old wheel-left binding moves to Q"),Reloaded->GetKey(EInventoryControl::TurnLeft)==EKeys::Q);
	TestTrue(TEXT("Old wheel-right binding moves to E"),Reloaded->GetKey(EInventoryControl::TurnRight)==EKeys::E);
	TestTrue(TEXT("Unrelated custom bindings survive wheel migration"),Reloaded->GetKey(EInventoryControl::Toggle)==EKeys::K && Reloaded->GetKey(EInventoryControl::RotateWithMouse)==EKeys::R);
	TestTrue(TEXT("Legacy Escape migrates to cancel without resetting preferences"),Reloaded->GetKey(EInventoryControl::Cancel)==EKeys::C && Reloaded->bToggleGrab && Reloaded->TurnSpeed==75.f);
	TestTrue(TEXT("Existing R binding is preserved when backpack control is added"),Reloaded->GetKey(EInventoryControl::OpenBackpack)!=EKeys::R);

	for (int32 Index = 0; Index < static_cast<int32>(EInventoryControl::Count); ++Index)
	{
		const auto Action = static_cast<EInventoryControl>(Index);
		if (!UInventoryInputSettings::IsBindable(Action)) continue;
		const FKey Key = Reloaded->GetKey(Action);
		TestTrue(TEXT("Migrated control has no overlapping conflicts"),Reloaded->Conflicts(Action,Key).IsEmpty());

	}
	Settings->ResetDefaults();
	TArray<FString> PreviousKeys;
	for (int32 Index = 0; Index < static_cast<int32>(EInventoryControl::Count); ++Index)
		PreviousKeys.Add(Settings->GetKey(static_cast<EInventoryControl>(Index)).GetFName().ToString());
	PreviousKeys[static_cast<int32>(EInventoryControl::Bandage)] = TEXT("One");
	GConfig->SetInt(Section,TEXT("ControlsVersion"),2,Path);
	GConfig->SetArray(Section,TEXT("Keys"),PreviousKeys,Path);
	Reloaded->ReloadConfig(nullptr,*Path);
	TestTrue(TEXT("Previous bandage default migrates to G"),Reloaded->GetKey(EInventoryControl::Bandage)==EKeys::G);
	PreviousKeys[static_cast<int32>(EInventoryControl::Run)] = TEXT("G");
	GConfig->SetArray(Section,TEXT("Keys"),PreviousKeys,Path);
	Reloaded->ReloadConfig(nullptr,*Path);
	TestTrue(TEXT("Migration preserves custom G and chooses free letter"),Reloaded->GetKey(EInventoryControl::Run)==EKeys::G && Reloaded->GetKey(EInventoryControl::Bandage)==EKeys::H);
	const TCHAR* LegacySection = TEXT("/Script/prototype3.ItemUseSettings");
	GConfig->SetArray(LegacySection,TEXT("Keys"),{TEXT("One"),TEXT("J"),TEXT("G")},Path);
	GConfig->SetArray(LegacySection,TEXT("ItemTypes"),{TEXT("CannedBeans"),TEXT("Bandage"),TEXT("WaterBottle")},Path);
	TStrongObjectPtr<UItemUseSettings> Legacy(NewObject<UItemUseSettings>());
	Legacy->ReloadConfig(nullptr,*Path);
	TestTrue(TEXT("Legacy custom bandage key follows item identity"),Legacy->MigrationKey(0)==EKeys::J);
	TestTrue(TEXT("Legacy custom food key follows item identity"),Legacy->MigrationKey(1)==EKeys::One);
	TestTrue(TEXT("Legacy water default becomes 3"),Legacy->MigrationKey(2)==EKeys::Three);
	GConfig->UnloadFile(Path);
	IFileManager::Get().Delete(*Path);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryMouseRotationTest, "Prototype.Inventory.MouseRotation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInventoryMouseRotationTest::RunTest(const FString&)
{
	TStrongObjectPtr<UInventoryComponent> Inventory(NewObject<UInventoryComponent>());
	TStrongObjectPtr<UInventoryInputSettings> Settings(NewObject<UInventoryInputSettings>());
	Settings->ResetDefaults();
	if (!TestTrue(TEXT("Demo fixture"), InventoryDemo::Populate(Inventory.Get()))) return false;
	const auto Panel = SNew(SInventoryPanel).Inventory(Inventory.Get()).Controls(Settings.Get()).SaveControls(false);
	const FGeometry G = FGeometry::MakeRoot(FVector2D(1000,800), FSlateLayoutTransform(1.5f));
	auto Mouse = [&](FVector2D P, FKey B) { const FVector2D A = G.LocalToAbsolute(P); return FPointerEvent(0,A,A,TSet<FKey>(),B,0,FModifierKeysState()); };
	auto Down = [&](FVector2D P, FKey B) { Panel->OnMouseButtonDown(G,Mouse(P,B)); };
	auto Up = [&](FVector2D P, FKey B) { Panel->OnMouseButtonUp(G,Mouse(P,B)); };
	auto Move = [&](FVector2D P) { Panel->OnMouseMove(G,Mouse(P,EKeys::Invalid)); };
	const FGuid Id = Inventory->GetEntries()[0].Item.InstanceId;
	const FVector2D Origin(40,150), Center(330,480), Pivot = Origin + Center;
	FInventoryEntry Entry;
	auto Begin = [&](FVector2D Offset, double Angle = 37.0)
	{
		TestTrue(TEXT("Reset placement"),Inventory->MoveItem(Id,TEXT("Main"),Center,Angle)==EInventoryResult::Success);
		Down(Pivot + Offset,EKeys::LeftMouseButton);
		Down(Pivot + Offset,EKeys::RightMouseButton);
	};
	auto Check = [&](const TCHAR* Label, FVector2D ExpectedCenter, double ExpectedAngle)
	{
		Inventory->GetItem(Id,Entry);
		TestTrue(FString(Label)+TEXT(" position"),Entry.Position.Equals(ExpectedCenter,0.001));
		TestTrue(FString(Label)+TEXT(" angle"),FMath::IsNearlyEqual(Entry.AngleDegrees,ExpectedAngle,0.001));
	};
	Begin(FVector2D(10,0));
	Panel->Tick(G,0,1);
	Up(Pivot+FVector2D(10,0),EKeys::LeftMouseButton);
	Up(Pivot+FVector2D(10,0),EKeys::RightMouseButton);
	Check(TEXT("No snap or timed rotation"),Center,37);
	Begin(FVector2D(10,0)); Move(Pivot+FVector2D(0,70));
	Check(TEXT("Preview is atomic"),Center,37);
	Up(Pivot+FVector2D(0,70),EKeys::RightMouseButton);
	Move(Pivot+FVector2D(-20,50)); Up(Pivot+FVector2D(-20,50),EKeys::LeftMouseButton);
	Check(TEXT("Resume moving without a jump"),Center+FVector2D(-20,-20),127);
	Begin(FVector2D(10,0)); Move(Pivot+FVector2D(0,-70));
	Up(Pivot+FVector2D(0,-70),EKeys::LeftMouseButton);
	Up(Pivot+FVector2D(0,-70),EKeys::RightMouseButton);
	Check(TEXT("Left released first commits fixed center"),Center,307);
	Begin(FVector2D::ZeroVector); Move(Pivot+FVector2D(70,0)); Move(Pivot+FVector2D(0,70));
	Move(Pivot); Move(Pivot+FVector2D(0,-70));
	Up(Pivot+FVector2D(0,-70),EKeys::LeftMouseButton); Up(Pivot,EKeys::RightMouseButton);
	Check(TEXT("Starting and crossing at the pivot does not snap"),Center,127);
	Begin(FVector2D(10,0)); Move(Pivot+FVector2D(-70,0));
	Up(Pivot+FVector2D(-70,0),EKeys::LeftMouseButton); Up(Pivot,EKeys::RightMouseButton);
	Check(TEXT("Crossing pivot between events does not flip"),Center,37);
	Begin(FVector2D(10,0),350);
	for (FVector2D D : { FVector2D(0,70), FVector2D(-70,1), FVector2D(-70,-1), FVector2D(0,-70), FVector2D(70,0) }) Move(Pivot+D);
	Up(Pivot+FVector2D(70,0),EKeys::LeftMouseButton); Up(Pivot,EKeys::RightMouseButton);
	Check(TEXT("Full turn across angle boundary"),Center,350);
	Begin(FVector2D(10,0)); Move(Pivot+FVector2D(0,70));
	Up(Pivot+FVector2D(0,70),EKeys::RightMouseButton);
	Down(Pivot+FVector2D(0,70),EKeys::RightMouseButton); Move(Pivot+FVector2D(-70,0));
	Panel->OnKeyDown(G,FKeyEvent(EKeys::C,FModifierKeysState(),0,false,0,0));
	Up(Pivot,EKeys::LeftMouseButton); Up(Pivot,EKeys::RightMouseButton);
	Check(TEXT("Middle click cancels repeated rotation"),Center,37);
	Begin(FVector2D(10,0)); Move(Pivot+FVector2D(0,70));
	Up(Pivot+FVector2D(0,70),EKeys::RightMouseButton);
	Move(FVector2D(0,0)); Up(FVector2D(0,0),EKeys::LeftMouseButton);
	Check(TEXT("Invalid drop restores position and angle"),Center,37);
	Begin(FVector2D(10,0));
	const FVector2D WheelPoint = Pivot + FVector2D(10,0);
	const FVector2D AbsoluteWheelPoint = G.LocalToAbsolute(WheelPoint);
	Panel->OnMouseWheel(G,FPointerEvent(0,AbsoluteWheelPoint,AbsoluteWheelPoint,TSet<FKey>(),EKeys::Invalid,1,FModifierKeysState()));
	TestEqual(TEXT("Wheel raises mouse rotation speed while held"),Settings->TurnSpeed,135.f);
	Move(Pivot+FVector2D(0,70));
	Up(Pivot+FVector2D(0,70),EKeys::LeftMouseButton); Up(Pivot,EKeys::RightMouseButton);
	Check(TEXT("Faster mouse rotation"),Center,138.25);
	Settings->TurnSpeed = 60.f;
	Begin(FVector2D(10,0)); Move(Pivot+FVector2D(0,70));
	Up(Pivot+FVector2D(0,70),EKeys::LeftMouseButton); Up(Pivot,EKeys::RightMouseButton);
	Check(TEXT("Slower mouse rotation"),Center,82);
	return true;
}
#endif
