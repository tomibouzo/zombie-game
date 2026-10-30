#include "Gameplay/Procedural/Encounter/HouseEncounterPlacement.h"
#include "Gameplay/Procedural/HouseGenerator.h"
#include "Gameplay/Procedural/HouseSizing.h"
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

bool HouseEncounterPlacement::Plan(AHouseGenerator* House, APawn* Player, float ZombieRadius, float ZombieHalfHeight,
	int32 ZombieCount, int32 ItemCount, int32 Seed, FHouseEncounterPlacement& Out, FString& Error)
{
	Out = FHouseEncounterPlacement();
	Error.Empty();
	if (!IsValid(House) || !IsValid(Player) || ZombieCount < 0 || ZombieCount > 32 || ItemCount < 0 || ItemCount > 64)
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
	TArray<FVector> Points;
	// Fixed lattice plus seeded shuffle. No navmesh random-point calls or actor iteration order in selection.
	// Original Random Layout stores its footprint in GeneratedSettings; planned layouts store an explicit rectangle.
	const FIntPoint Footprint = FHouseSizing::Footprint(House->GeneratedSettings, House->Layout);
	const FVector2D Size(Footprint.X * Cell, Footprint.Y * Cell);
	if (Size.GetMax() > 10000)
	{ Error = TEXT("Encounter footprint exceeds the bounded placement search (100 metres)."); return false; }
	for (double Y = 75; Y < Size.Y; Y += 75)
		for (double X = 75; X < Size.X; X += 75)
			Points.Add(House->GetActorTransform().TransformPosition(FVector(X,Y,0)));
	FRandomStream Random(Seed);
	for (int32 I = Points.Num()-1; I > 0; --I) Points.Swap(I, Random.RandRange(0,I));
	TArray<FVector> Reserved = {Candidate.PlayerFeet, Candidate.ExitFeet};
	const float Separation = FMath::Max(110.f, Radius * 2 + 30);
	for (const FVector& Point : Points)
	{
		if (Candidate.ZombieFeet.Num() == ZombieCount && Candidate.ItemFeet.Num() == ItemCount) break;
		if (FVector::Dist2D(Point, Candidate.PlayerFeet) < 250 || Reserved.ContainsByPredicate([&](const FVector& P)
			{ return FVector::DistSquared2D(Point,P) < FMath::Square(Separation); })) continue;
		if (!ClearCapsule(World, Point, Radius, Height, Player) || !Reachable(World, Point, Candidate.PlayerFeet, Radius, Height, Player)) continue;
		if (Candidate.ZombieFeet.Num() < ZombieCount) Candidate.ZombieFeet.Add(Point);
		else Candidate.ItemFeet.Add(Point);
		Reserved.Add(Point);
	}
	if (Candidate.ZombieFeet.Num() != ZombieCount || Candidate.ItemFeet.Num() != ItemCount)
	{
		Error = FString::Printf(TEXT("No complete placement: found %d/%d zombies and %d/%d items. Reduce counts or change the seed."),
			Candidate.ZombieFeet.Num(), ZombieCount, Candidate.ItemFeet.Num(), ItemCount);
		return false;
	}
	Out = MoveTemp(Candidate);
	return true;
}
