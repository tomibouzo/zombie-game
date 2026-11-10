#include "Gameplay/Shelter/Build/ShelterBuildArea.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

AShelterBuildArea::AShelterBuildArea()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	// Editor outline of the grid only; it never blocks or overlaps anything.
	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("GridOutline"));
	Bounds->SetupAttachment(GetRootComponent());
	Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bounds->SetCanEverAffectNavigation(false);
	Bounds->SetLineThickness(4.f);
}

void AShelterBuildArea::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	const FVector Half(SizeCells.X * Module.Width * 0.5, SizeCells.Y * Module.Width * 0.5, Module.Height * 0.5);
	Bounds->SetBoxExtent(Half.ComponentMax(FVector(1)));
	Bounds->SetRelativeLocation(Half);
}

void AShelterBuildArea::BeginPlay()
{
	Super::BeginPlay();
	Model = FShelterBuildModel();
	Model.SizeCells = SizeCells;
	PieceActors.Reset();
}

void AShelterBuildArea::EndPlay(const EEndPlayReason::Type Reason)
{
	for (AShelterPiece* Piece : PieceActors) if (IsValid(Piece)) Piece->Destroy();
	PieceActors.Reset();
	Super::EndPlay(Reason);
}

bool AShelterBuildArea::ContainsLocation(const FVector& WorldLocation) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation);
	const double Margin = FMath::Max(0.f, EntryMargin);
	return Local.X >= -Margin && Local.Y >= -Margin
		&& Local.X <= SizeCells.X * Module.Width + Margin && Local.Y <= SizeCells.Y * Module.Width + Margin
		&& Local.Z >= -100 && Local.Z <= Module.Height + 200;
}

FTransform AShelterBuildArea::SlotTransform(const FShelterSlot& Slot, int32 Turns) const
{
	const FVector2D Center = FShelterBuildModel::SlotCenter(Slot, Module.Width);
	const FTransform Local(FRotator(0, FShelterBuildModel::SlotYaw(Slot, Turns), 0), FVector(Center.X, Center.Y, 0));
	return Local * GetActorTransform();
}

bool AShelterBuildArea::FindSlotFromView(const FVector& Start, const FVector& Direction, float MaxDistance, EShelterSlotKind Kind, FShelterSlot& OutSlot) const
{
	if (Start.ContainsNaN() || Direction.ContainsNaN() || Direction.IsNearlyZero() || !FMath::IsFinite(MaxDistance) || MaxDistance <= 0) return false;
	const FTransform& Transform = GetActorTransform();
	const FVector LocalStart = Transform.InverseTransformPosition(Start);
	const FVector LocalDirection = Transform.InverseTransformVectorNoScale(Direction.GetSafeNormal());
	double Distance = MaxDistance;
	if (LocalDirection.Z < -UE_KINDA_SMALL_NUMBER) Distance = FMath::Min<double>(MaxDistance, -LocalStart.Z / LocalDirection.Z);
	const FVector Aim = LocalStart + LocalDirection * FMath::Max(0.0, Distance);
	return Model.NearestSlot(FVector2D(Aim.X, Aim.Y), Kind, Module.Width, OutSlot);
}

EShelterBuildResult AShelterBuildArea::CanPlace(const FShelterSlot& Slot, const UShelterPieceDefinition* Piece) const
{
	if (!IsValid(Piece) || !Piece->IsValidDefinition()) return EShelterBuildResult::InvalidPiece;
	const EShelterBuildResult Result = Model.CanPlace(Slot, Piece->PieceId, Piece->SlotKind);
	if (Result != EShelterBuildResult::Success || !GetWorld()) return Result;
	const FTransform Transform = SlotTransform(Slot, 0);
	const bool bEdge = Slot.Kind == EShelterSlotKind::Edge;
	const FVector Half = bEdge
		? FVector(Module.Width * 0.5 - 2, Module.WallThickness * 0.5 + 2, Module.Height * 0.5 - 2)
		: FVector(Module.Width * 0.5 - 2, Module.Width * 0.5 - 2, Module.Height * 0.5 - 2);
	const FVector Center = Transform.TransformPosition(FVector(0, 0, Module.Height * 0.5));
	if (GetWorld()->OverlapAnyTestByObjectType(Center, Transform.GetRotation(), FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeBox(Half), FCollisionQueryParams(SCENE_QUERY_STAT(ShelterBuildSlot), false)))
		return EShelterBuildResult::Blocked;
	return EShelterBuildResult::Success;
}

AShelterPiece* AShelterBuildArea::SpawnPiece(const UShelterPieceDefinition* Piece, const FTransform& Transform)
{
	if (!GetWorld() || !IsValid(Piece)) return nullptr;
	auto* Actor = GetWorld()->SpawnActorDeferred<AShelterPiece>(Piece->ResolveActorClass(), Transform, this, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Actor) return nullptr;
	Actor->InitializePiece(Piece, Module);
	Actor->FinishSpawning(Transform);
	return IsValid(Actor) ? Actor : nullptr;
}

AShelterPiece* AShelterBuildArea::SpawnPreview(const UShelterPieceDefinition* Piece)
{
	AShelterPiece* Actor = SpawnPiece(Piece, GetActorTransform());
	if (Actor) Actor->SetPreview(false);
	return Actor;
}

EShelterBuildResult AShelterBuildArea::PlacePiece(const FShelterSlot& Slot, const UShelterPieceDefinition* Piece, int32 Turns)
{
	const EShelterBuildResult Check = CanPlace(Slot, Piece);
	if (Check != EShelterBuildResult::Success) return Check;
	// Create the actor first: a failed spawn must leave the grid unchanged.
	const int32 Normalized = FShelterBuildModel::NormalizeTurns(Slot.Kind, Turns);
	AShelterPiece* Actor = SpawnPiece(Piece, SlotTransform(Slot, Normalized));
	if (!Actor) return EShelterBuildResult::InvalidPiece;
	const EShelterBuildResult Result = Model.Place(Slot, Piece->PieceId, Piece->SlotKind, Normalized);
	if (Result != EShelterBuildResult::Success) { Actor->Destroy(); return Result; }
	PieceActors.Add(Actor);
	return EShelterBuildResult::Success;
}

EShelterBuildResult AShelterBuildArea::RemoveAt(const FShelterSlot& Slot)
{
	const int32 Index = Model.FindIndex(Slot);
	if (Index == INDEX_NONE) return EShelterBuildResult::NotFound;
	Model.Remove(Slot);
	if (PieceActors.IsValidIndex(Index))
	{
		if (IsValid(PieceActors[Index])) PieceActors[Index]->Destroy();
		PieceActors.RemoveAt(Index);
	}
	return EShelterBuildResult::Success;
}

EShelterBuildResult AShelterBuildArea::RemovePiece(const AShelterPiece* Piece)
{
	const int32 Index = Piece ? PieceActors.IndexOfByKey(Piece) : INDEX_NONE;
	if (!Model.Placements.IsValidIndex(Index)) return EShelterBuildResult::NotFound;
	const FShelterSlot Slot = Model.Placements[Index].Slot;
	return RemoveAt(Slot);
}

AShelterPiece* AShelterBuildArea::GetPieceAt(const FShelterSlot& Slot) const
{
	const int32 Index = Model.FindIndex(Slot);
	return PieceActors.IsValidIndex(Index) ? PieceActors[Index].Get() : nullptr;
}

AShelterBuildArea* AShelterBuildArea::FindContaining(const UWorld* World, const FVector& WorldLocation)
{
	if (!World) return nullptr;
	for (TActorIterator<AShelterBuildArea> It(World); It; ++It)
		if (It->ContainsLocation(WorldLocation)) return *It;
	return nullptr;
}
