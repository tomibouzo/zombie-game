#include "Gameplay/Procedural/Encounter/HouseEncounterPlacement.h"
#include "Gameplay/Procedural/HouseGenerator.h"
#include "Gameplay/Procedural/HouseSizing.h"
#include "Gameplay/Procedural/HouseSpatial.h"
#include "Gameplay/Items/World/DroppedItem.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"

namespace
{
FCollisionObjectQueryParams EncounterObjects()
{
	FCollisionObjectQueryParams Objects;
	for (auto Channel : {ECC_WorldStatic, ECC_WorldDynamic, ECC_PhysicsBody, ECC_Pawn}) Objects.AddObjectTypesToQuery(Channel);
	return Objects;
}

bool Supported(UWorld* World, FVector Feet, float Radius, const AActor* Ignored)
{
	FCollisionQueryParams Query(SCENE_QUERY_STAT(EncounterFloor), false, Ignored);
	// Check the footprint as well as its centre; a centre trace alone accepts overhanging ledges.
	for (FVector Offset : {FVector::ZeroVector, FVector(Radius,0,0), FVector(-Radius,0,0), FVector(0,Radius,0), FVector(0,-Radius,0)})
	{
		FHitResult Hit;
		if (!World->LineTraceSingleByObjectType(Hit, Feet + Offset + FVector(0,0,8), Feet + Offset - FVector(0,0,8),
			FCollisionObjectQueryParams(ECC_WorldStatic), Query) || Hit.bStartPenetrating || Hit.ImpactNormal.Z < .9f
			|| FMath::Abs(Hit.ImpactPoint.Z - Feet.Z) > 3) return false;
	}
	return true;
}
}

bool HouseEncounterPlacement::ClearCapsule(UWorld* World, FVector Feet, float Radius, float HalfHeight, const AActor* Ignored)
{
	if (!World || Radius <= 0 || HalfHeight < Radius || Feet.ContainsNaN()) return false;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(EncounterClearance), false, Ignored);
	return Supported(World, Feet, Radius, Ignored) && !World->OverlapAnyTestByObjectType(
		Feet + FVector(0,0,HalfHeight + 2), FQuat::Identity, EncounterObjects(),
		FCollisionShape::MakeCapsule(Radius + 2, HalfHeight), Query);
}

bool HouseEncounterPlacement::Reachable(UWorld* World, FVector From, FVector To, float Radius, float HalfHeight, const AActor* Ignored, FString* Error)
{
	auto Fail = [Error](const FString& Reason) { if (Error) *Error = Reason; return false; };
	auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!Nav) return Fail(TEXT("Navigation system is unavailable."));
	FNavLocation Start, End;
	const FVector Extent(12,12,25);
	if (!Nav->ProjectPointToNavigation(From, Start, Extent) || !Nav->ProjectPointToNavigation(To, End, Extent)
		|| FVector::Dist2D(Start.Location, From) > 12 || FVector::Dist2D(End.Location, To) > 12)
		return Fail(TEXT("Route endpoint is outside the navigable floor."));
	UNavigationPath* Path = Nav->FindPathToLocationSynchronously(World, Start.Location, End.Location);
	if (!Path || !Path->IsValid() || Path->IsPartial() || Path->PathPoints.Num() < 2)
		return Fail(TEXT("Navigation returned an incomplete path."));
	FCollisionQueryParams Query(SCENE_QUERY_STAT(EncounterRoute), false, Ignored);
	// Recast has voxel height offsets. This encounter only supports a level floor, so sweep at its exact height.
	FVector Previous = From + FVector(0,0,HalfHeight + 2);
	for (int32 I = 0; I <= Path->PathPoints.Num(); ++I)
	{
		FVector Next = I == Path->PathPoints.Num() ? To : Path->PathPoints[I];
		if (FMath::Abs(Next.Z - From.Z) > 25) return Fail(TEXT("Path leaves the supported floor height."));
		Next.Z = Previous.Z;
		FHitResult Hit;
		if (World->SweepSingleByObjectType(Hit, Previous, Next, FQuat::Identity, EncounterObjects(),
			FCollisionShape::MakeCapsule(Radius, HalfHeight), Query))
			return Fail(FString::Printf(TEXT("Path capsule hits %s (%s)."), *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent())));
		Previous = Next;
	}
	return true;
}

bool HouseEncounterPlacement::ClearItem(UWorld* World, FVector Feet, FVector HalfExtent, FQuat Rotation, const AActor* Ignored)
{
	if (!World || Feet.ContainsNaN() || HalfExtent.ContainsNaN() || HalfExtent.GetMin() <= 0) return false;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(EncounterItemSupport), false, Ignored);
	// The item box, including its corners, must rest on the floor; no shelf or ledge overhangs.
	for (FVector Offset : {FVector::ZeroVector, FVector(HalfExtent.X,HalfExtent.Y,0), FVector(-HalfExtent.X,HalfExtent.Y,0),
		FVector(HalfExtent.X,-HalfExtent.Y,0), FVector(-HalfExtent.X,-HalfExtent.Y,0)})
	{
		const FVector Point = Feet + Rotation.RotateVector(Offset);
		FHitResult Hit;
		if (!World->LineTraceSingleByObjectType(Hit,Point+FVector(0,0,8),Point-FVector(0,0,8),
			FCollisionObjectQueryParams(ECC_WorldStatic),Query) || Hit.bStartPenetrating || Hit.ImpactNormal.Z < .9f
			|| FMath::Abs(Hit.ImpactPoint.Z-Feet.Z) > 3) return false;
	}
	return !World->OverlapAnyTestByObjectType(Feet+FVector(0,0,HalfExtent.Z+2),Rotation,
		EncounterObjects(),FCollisionShape::MakeBox(HalfExtent+FVector(1)),Query);
}

bool HouseEncounterPlacement::Plan(AHouseGenerator* House, APawn* Player, float ZombieRadius, float ZombieHalfHeight,
	int32 ZombieCount, int32 ItemCount, int32 Seed, FHouseEncounterPlacement& Out, FString& Error)
{
	TArray<FVector> Extents;
	if (ItemCount >= 0 && ItemCount <= 64) Extents.Init(FVector(25),ItemCount);
	else { Out = FHouseEncounterPlacement(); Error = TEXT("Item count must be 0..64."); return false; }
	return Plan(House,Player,ZombieRadius,ZombieHalfHeight,ZombieCount,Extents,Seed,Out,Error);
}

bool HouseEncounterPlacement::Plan(AHouseGenerator* House, APawn* Player, float ZombieRadius, float ZombieHalfHeight,
	int32 ZombieCount, const TArray<FVector>& ItemHalfExtents, int32 Seed, FHouseEncounterPlacement& Out,
	FString& Error, FHouseEncounterPlacementReport* Report)
{
	Out = FHouseEncounterPlacement();
	Error.Empty();
	FHouseEncounterPlacementReport LocalReport;
	FHouseEncounterPlacementReport& Stats = Report ? *Report : LocalReport;
	Stats = FHouseEncounterPlacementReport();
	const int32 ItemCount = ItemHalfExtents.Num();
	if (!IsValid(House) || !IsValid(Player) || House->GetWorld() != Player->GetWorld()
		|| ZombieCount < 0 || ZombieCount > 32 || ItemCount > 64
		|| !FMath::IsFinite(ZombieRadius) || !FMath::IsFinite(ZombieHalfHeight) || ZombieRadius <= 0 || ZombieHalfHeight < ZombieRadius
		|| ItemHalfExtents.ContainsByPredicate([](FVector E) { return E.ContainsNaN() || E.GetMin() <= 0; }))
	{ Error = TEXT("Encounter requires a player, a house, 0..32 zombies and 0..64 items."); return false; }
	UWorld* World = House->GetWorld();
	const float PlayerRadius = Player->GetSimpleCollisionRadius(), PlayerHeight = Player->GetSimpleCollisionHalfHeight();
	const float Radius = FMath::Max(PlayerRadius, ZombieRadius), Height = FMath::Max(PlayerHeight, ZombieHalfHeight);
	const FHouseWall* Door = House->Layout.Walls.FindByPredicate([](const FHouseWall& W)
		{ return W.RoomB == INDEX_NONE && W.Opening == EHouseOpening::Door; });
	if (!Door || !House->GetActorScale3D().Equals(FVector::OneVector) || !FMath::IsNearlyZero(House->GetActorRotation().Pitch)
		|| !FMath::IsNearlyZero(House->GetActorRotation().Roll))
	{ Error = TEXT("Encounter needs a south entrance and an upright house at unit scale."); return false; }
	const float Cell = House->GeneratedSettings.CellSize;
	const FVector Entrance((Door->Start.X + Door->End.X) * Cell * .5, 0, 0);
	FHouseEncounterPlacement Candidate;
	Candidate.ExitFeet = House->GetActorTransform().TransformPosition(Entrance - FVector(0,150,0));
	if (!ClearCapsule(World, Candidate.ExitFeet, Radius, Height, Player))
	{ Error = TEXT("Exit landing is blocked or unsupported."); return false; }
	bool bEntranceFound = false;
	FString EntranceError = TEXT("All entrance candidates are obstructed or unsupported.");
	// Furniture reserves doorway access, but a fixed point farther into the room may hit a sofa or table.
	for (float Distance : {75.f, 100.f, 150.f, 200.f, 250.f})
	{
		const FVector Start = House->GetActorTransform().TransformPosition(Entrance + FVector(0,Distance,0));
		if (ClearCapsule(World, Start, Radius, Height, Player) && Reachable(World, Start, Candidate.ExitFeet, Radius, Height, Player, &EntranceError))
		{ Candidate.PlayerFeet = Start; bEntranceFound = true; break; }
	}
	if (!bEntranceFound)
	{ Error = TEXT("No usable entrance: ") + EntranceError; return false; }
	const FTransform HouseTransform = House->GetActorTransform();
	const FVector LocalPlayer = HouseTransform.InverseTransformPosition(Candidate.PlayerFeet);
	const int32 PlayerRoom = FHouseSpatial::RoomAt(House->GeneratedSettings,House->Layout,FVector2D(LocalPlayer));
	if (PlayerRoom == INDEX_NONE)
	{ Error = TEXT("Could not identify the player's starting room."); return false; }
	// Reserve the whole starting room, including a capsule margin at open doorways.
	// This affects initial zombie placement only; navigation into the room stays enabled.
	const FBox2D ZombieExclusion = FHouseSpatial::RoomBounds(House->GeneratedSettings,House->Layout.Rooms[PlayerRoom]).ExpandBy(ZombieRadius+2);
	TArray<FVector> ZombiePoints, ItemPoints;
	// Fixed lattice plus seeded shuffle. No navmesh random-point calls or actor iteration order in selection.
	// Original Random Layout stores its footprint in GeneratedSettings; planned layouts store an explicit rectangle.
	const FIntPoint Footprint = FHouseSizing::Footprint(House->GeneratedSettings, House->Layout);
	const FVector2D Size(Footprint.X * Cell, Footprint.Y * Cell);
	if (Size.GetMax() > 10000)
	{ Error = TEXT("Encounter footprint exceeds the bounded placement search (100 metres)."); return false; }
	for (double Y = 75; Y < Size.Y; Y += 75)
		for (double X = 75; X < Size.X; X += 75)
			ZombiePoints.Add(HouseTransform.TransformPosition(FVector(X,Y,0)));
	for (double Y = 25; Y < Size.Y; Y += 50)
		for (double X = 25; X < Size.X; X += 50)
			ItemPoints.Add(HouseTransform.TransformPosition(FVector(X,Y,0)));
	const float PickupRadius = GetDefault<ADroppedItem>()->GetPickupRadius();
	if (ItemCount && (!FMath::IsFinite(PickupRadius) || PickupRadius <= 0))
	{ Error = TEXT("Collectible pickup radius must be positive and finite."); return false; }
	TArray<FBox2D> AccessZones;
	for (const FHouseFurniturePlacement& Item : House->Layout.Furniture)
	{
		AccessZones.Add(FHouseSpatial::FurnitureBody(House->GeneratedSettings,Item));
		AccessZones.Add(FHouseSpatial::FurnitureAccess(House->GeneratedSettings,Item));
	}
	for (const FHouseWall& Wall : House->Layout.Walls)
		if (Wall.Opening == EHouseOpening::Door)
			for (int32 Room : {Wall.RoomA,Wall.RoomB})
				if (House->Layout.Rooms.IsValidIndex(Room))
					AccessZones.Add(FHouseSpatial::DoorAccess(House->GeneratedSettings,House->Layout.Rooms[Room],Wall));
	TArray<int8> ZombieValidity;
	ZombieValidity.Init(0,ZombiePoints.Num());
	auto Shuffle = [](TArray<int32>& Indices,int32 Count,FRandomStream& Random)
	{
		for (int32 I=0; I<Count; ++I) Indices.Add(I);
		for (int32 I=Count-1; I>0; --I) Indices.Swap(I,Random.RandRange(0,I));
	};
	const float Separation = FMath::Max(110.f,Radius*2+30);
	const FHouseEncounterPlacement EntrancePlan = Candidate;
	// Alternate placement priority, then try a different seeded ordering. Never change the layout or counts.
	constexpr int32 MaxAttempts = 4;
	for (int32 Attempt=0; Attempt<MaxAttempts; ++Attempt)
	{
		Stats.Attempts = Attempt+1;
		Candidate = EntrancePlan;
		FRandomStream Random(static_cast<int32>(static_cast<uint32>(Seed)+0x9e3779b9u*Attempt));
		TArray<int32> ZombieOrder, ItemOrder;
		Shuffle(ZombieOrder,ZombiePoints.Num(),Random);
		Shuffle(ItemOrder,ItemPoints.Num(),Random);
		TArray<FBox2D> ItemBoxes;
		auto Local2 = [&](FVector P) { return FVector2D(HouseTransform.InverseTransformPosition(P)); };
		auto BodyNear = [](const FBox2D& Box,FVector2D P,double BodyRadius)
		{ return Box.ComputeSquaredDistanceToPoint(P) < FMath::Square(BodyRadius+3); };
		auto PlaceZombies = [&]()
		{
			for (int32 Index : ZombieOrder)
			{
				if (Candidate.ZombieFeet.Num() == ZombieCount) break;
				const FVector Point = ZombiePoints[Index];
				if (ZombieExclusion.IsInsideOrOn(Local2(Point)) || FVector::Dist2D(Point,Candidate.PlayerFeet) < 250) continue;
				if (Candidate.ZombieFeet.ContainsByPredicate([&](FVector P) { return FVector::Dist2D(P,Point)<Separation; })
					|| ItemBoxes.ContainsByPredicate([&](const FBox2D& Box) { return BodyNear(Box,Local2(Point),Radius); })
					|| Candidate.ItemAccessFeet.ContainsByPredicate([&](FVector P) { return FVector::Dist2D(P,Point)<PlayerRadius+ZombieRadius+5; })) continue;
				int8& Validity = ZombieValidity[Index];
				if (!Validity)
				{
					if (!ClearCapsule(World,Point,Radius,Height,Player)) { Validity=-1; ++Stats.GeometryRejections; }
					else if (!Reachable(World,Point,Candidate.PlayerFeet,Radius,Height,Player)) { Validity=-1; ++Stats.AccessRejections; }
					else Validity=1;
				}
				if (Validity>0) Candidate.ZombieFeet.Add(Point);
			}
		};
		auto PlaceItems = [&]()
		{
			// Place larger bodies first; restore selected-type order before returning a complete plan.
			TArray<int32> Sequence;
			for (int32 I=0; I<ItemCount; ++I) Sequence.Add(I);
			Sequence.StableSort([&](int32 A,int32 B) { return ItemHalfExtents[A].X*ItemHalfExtents[A].Y > ItemHalfExtents[B].X*ItemHalfExtents[B].Y; });
			TArray<FVector> FeetByType, AccessByType;
			FeetByType.SetNum(ItemCount); AccessByType.SetNum(ItemCount);
			int32 Placed=0;
			for (int32 ItemIndex : Sequence)
			{
				const FVector Extent = ItemHalfExtents[ItemIndex];
				bool bFound=false;
				for (int32 PointIndex : ItemOrder)
				{
					const FVector Feet=ItemPoints[PointIndex];
					if (FVector::Dist2D(Feet,Candidate.PlayerFeet)<250) continue;
					const FVector2D Local=Local2(Feet), Margin(Extent.X+2,Extent.Y+2);
					const FBox2D Box(Local-Margin,Local+Margin);
					const int32 Room=FHouseSpatial::RoomAt(House->GeneratedSettings,House->Layout,Local);
					if (Room==INDEX_NONE || !FHouseSpatial::Contains(FHouseSpatial::RoomBounds(House->GeneratedSettings,
						House->Layout.Rooms[Room],House->GeneratedSettings.WallThickness*.5),Box)) continue;
					if (AccessZones.ContainsByPredicate([&](const FBox2D& Zone) { return FHouseSpatial::Overlaps(Zone,Box); })
						|| ItemBoxes.ContainsByPredicate([&](const FBox2D& Other) { return FHouseSpatial::Overlaps(Other.ExpandBy(8),Box); })
						|| Candidate.ZombieFeet.ContainsByPredicate([&](FVector P) { return BodyNear(Box,Local2(P),Radius); })
						|| Candidate.ItemAccessFeet.ContainsByPredicate([&](FVector P) { return BodyNear(Box,Local2(P),PlayerRadius); })) continue;
					if (!ClearItem(World,Feet,Extent,HouseTransform.GetRotation(),Player)) { ++Stats.GeometryRejections; continue; }
					FVector Access;
					bool bAccessible=false;
					const double StartDistance=FMath::Max(60.,PlayerRadius+FVector2D(Extent).Size()+10);
					for (double Distance : {StartDistance,StartDistance+50})
					{
						for (int32 Direction=0; Direction<8; ++Direction)
						{
							const double Angle=Direction*UE_DOUBLE_PI/4;
							const FVector Stand=Feet+HouseTransform.TransformVectorNoScale(FVector(FMath::Cos(Angle)*Distance,FMath::Sin(Angle)*Distance,0));
							const FVector Center=Feet+FVector(0,0,Extent.Z+2), Eye=Stand+FVector(0,0,PlayerHeight*1.5f);
							if (FVector::Dist(Stand,Center)>PickupRadius || FVector::Dist(Eye,Center)>290
								|| FHouseSpatial::RoomAt(House->GeneratedSettings,House->Layout,Local2(Stand))!=Room
								|| Candidate.ZombieFeet.ContainsByPredicate([&](FVector P) { return FVector::Dist2D(P,Stand)<PlayerRadius+ZombieRadius+5; })
								|| ItemBoxes.ContainsByPredicate([&](const FBox2D& Other) { return BodyNear(Other,Local2(Stand),PlayerRadius); })) continue;
							FCollisionQueryParams Query(SCENE_QUERY_STAT(EncounterPickupAccess),false,Player);
							FHitResult Hit;
							if (!ClearCapsule(World,Stand,PlayerRadius,PlayerHeight,Player)
								|| World->LineTraceSingleByObjectType(Hit,Eye,Center,EncounterObjects(),Query)
								|| !Reachable(World,Stand,Candidate.PlayerFeet,PlayerRadius,PlayerHeight,Player)) continue;
							Access=Stand; bAccessible=true; break;
						}
						if (bAccessible) break;
					}
					if (!bAccessible) { ++Stats.AccessRejections; continue; }
					Candidate.ItemFeet.Add(Feet); Candidate.ItemAccessFeet.Add(Access); ItemBoxes.Add(Box);
					FeetByType[ItemIndex]=Feet; AccessByType[ItemIndex]=Access;
					++Placed; bFound=true; break;
				}
				if (!bFound) break;
			}
			if (Placed==ItemCount) { Candidate.ItemFeet=MoveTemp(FeetByType); Candidate.ItemAccessFeet=MoveTemp(AccessByType); }
		};
		if (Attempt%2==0) { PlaceZombies(); PlaceItems(); }
		else { PlaceItems(); PlaceZombies(); }
		if (Candidate.ZombieFeet.Num()+Candidate.ItemFeet.Num() > Stats.BestZombies+Stats.BestItems)
		{ Stats.BestZombies=Candidate.ZombieFeet.Num(); Stats.BestItems=Candidate.ItemFeet.Num(); }
		if (Candidate.ZombieFeet.Num()==ZombieCount && Candidate.ItemFeet.Num()==ItemCount)
		{
			Stats.Summary=FString::Printf(TEXT("Placement succeeded on attempt %d/%d: %d zombies, %d items with reachable pickup positions."),Stats.Attempts,MaxAttempts,ZombieCount,ItemCount);
			Out=MoveTemp(Candidate);
			return true;
		}
	}
	Stats.Summary=FString::Printf(TEXT("No complete placement after %d attempts: best attempt found %d/%d zombies and %d/%d items. Zombies exclude the player's starting room. This is a bounded search, not a capacity estimate; change the seed, size or counts."),
		Stats.Attempts,Stats.BestZombies,ZombieCount,Stats.BestItems,ItemCount);
	Error=Stats.Summary;
	return false;
}
