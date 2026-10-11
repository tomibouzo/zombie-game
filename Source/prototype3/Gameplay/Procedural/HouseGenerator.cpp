#include "Gameplay/Procedural/HouseGenerator.h"
#include "Gameplay/Procedural/HouseHousehold.h"
#include "Gameplay/Procedural/HouseSizing.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Gameplay/Procedural/HouseStyle.h"
#include "Gameplay/Procedural/HouseNavigation.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/BodySetup.h"
#include "UObject/ConstructorHelpers.h"
#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogHouseGenerator, Log, All);

namespace
{
const TCHAR* FurnitureComponentNames[] = {TEXT("FurnitureSofa"), TEXT("FurnitureCoffeeTable"), TEXT("FurnitureKitchenCounter"),
		TEXT("FurnitureFridge"), TEXT("FurnitureBed"), TEXT("FurnitureWardrobe"), TEXT("FurnitureSink"), TEXT("FurnitureToilet"),
		TEXT("FurnitureSingleBed"), TEXT("FurnitureDesk"), TEXT("FurnitureChair"), TEXT("FurnitureBedsideTable"), TEXT("FurnitureToyBox"), TEXT("FurnitureStorageShelf")};
static_assert(UE_ARRAY_COUNT(FurnitureComponentNames) == static_cast<int32>(EHouseFurnitureType::Count));
}

AHouseGenerator::AHouseGenerator()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Floors = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Floors"));
	Walls = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Walls"));
	Roof = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Roof"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	DefaultBlockMesh = Cube.Object;

	for (const TCHAR* Name : FurnitureComponentNames)
	{
		UInstancedStaticMeshComponent* Component = CreateDefaultSubobject<UInstancedStaticMeshComponent>(FName(Name));
		Component->SetupAttachment(GetRootComponent());
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetStaticMesh(DefaultBlockMesh);
		Component->SetCollisionProfileName(TEXT("BlockAll"));
		Component->SetCanEverAffectNavigation(true);
		FurnitureMeshes.Add(Component);
	}
	for (UInstancedStaticMeshComponent* Component : {Floors.Get(), Walls.Get(), Roof.Get()})
	{
		Component->SetupAttachment(GetRootComponent());
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetStaticMesh(DefaultBlockMesh);
		Component->SetCollisionProfileName(TEXT("BlockAll"));
		Component->SetCanEverAffectNavigation(true);
	}
}

void AHouseGenerator::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SetRoofHidden(bHideRoof);
	for (UTextRenderComponent* Label : GeneratedLabels) if (Label) Label->SetVisibility(bShowLabels);
}

void AHouseGenerator::BeginPlay()
{
	Super::BeginPlay();
	if (bGenerateOnBeginPlay) GenerateHouse();
	SetRoofHidden(bHideRoof);
	for (UTextRenderComponent* Label : GeneratedLabels) if (Label) Label->SetVisibility(bShowLabels);
}

void AHouseGenerator::SetRoofHidden(bool bNewHidden)
{
	bHideRoof = bNewHidden;
	Roof->SetVisibility(!bNewHidden);
	Roof->SetHiddenInGame(bNewHidden);
}

void AHouseGenerator::ResetFurnitureRulesToDefaults()
{
#if WITH_EDITOR
	const FScopedTransaction Transaction(NSLOCTEXT("HouseGenerator", "ResetFurniture", "Reset furniture rules"));
#endif
	Modify();
	Settings.FurnitureRules = FHouseGenerationSettings().FurnitureRules;
}

void AHouseGenerator::GenerateNewHouse()
{
#if WITH_EDITOR
	const FScopedTransaction Transaction(NSLOCTEXT("HouseGenerator", "NewHouse", "Generate new house"), !GetWorld()->IsGameWorld());
#endif
	Modify();
	Settings.Seed = static_cast<int32>(static_cast<uint32>(Settings.Seed) + 1u);
	GenerateHouse();
}

void AHouseGenerator::GenerateHouse()
{
	// Old maps serialize the former eight-element array. Rebuild this derived lookup from
	// the named native subobjects so new furniture types remain available after loading.
	FurnitureMeshes.Reset();
	for (const TCHAR* Name : FurnitureComponentNames)
		FurnitureMeshes.Add(CastChecked<UInstancedStaticMeshComponent>(GetDefaultSubobjectByName(FName(Name))));
	FHouseLayout NewLayout;
	FString Error;
	UStaticMesh* Mesh = Style && Style->BlockMesh ? Style->BlockMesh.Get() : DefaultBlockMesh.Get();
	if (!Mesh || Mesh->GetBoundingBox().GetSize().GetMin() <= UE_SMALL_NUMBER || !Mesh->GetBodySetup() ||
		Mesh->GetBodySetup()->AggGeom.GetElementCount() == 0)
	{
		LastError = TEXT("Block mesh needs nonzero bounds and collision. Existing house was preserved.");
		UE_LOG(LogHouseGenerator, Warning, TEXT("%s: %s"), *GetName(), *LastError);
		return;
	}
	const FHouseFurniturePalette* Palette = Style && Settings.Household.Profile != EHouseholdProfile::None ?
		Style->HouseholdPalettes.Find(Settings.Household.FurnitureStyle) : nullptr;
	if (Style)
	{
		TMap<EHouseFurnitureType, TObjectPtr<UStaticMesh>> SelectedMeshes = Style->FurnitureMeshes;
		if (Palette) for (const auto& Entry : Palette->Meshes) if (Entry.Value) SelectedMeshes.Add(Entry.Key, Entry.Value);
		for (const auto& Entry : SelectedMeshes)
		{
			UStaticMesh* FurnitureMesh = Entry.Value;
			if (FurnitureMesh && (FurnitureMesh->GetBoundingBox().GetSize().GetMin() <= UE_SMALL_NUMBER ||
				!FurnitureMesh->GetBodySetup() || FurnitureMesh->GetBodySetup()->AggGeom.GetElementCount() == 0))
			{
				LastError = TEXT("Furniture meshes need nonzero bounds and simple collision. Existing house was preserved.");
				UE_LOG(LogHouseGenerator, Warning, TEXT("%s: %s"), *GetName(), *LastError);
				return;
			}
		}
	}
	FHouseGenerationSettings EffectiveSettings;
	int32 Attempts = 0;
	const int32 RequestedRoomCount = Settings.RoomCount;
	if (!FHouseLayoutGenerator::GenerateWithRetries(Settings, EffectiveSettings, NewLayout, Attempts, Error))
	{
		LastError = Error;
		UE_LOG(LogHouseGenerator, Warning, TEXT("%s: %s Existing house was preserved."), *GetName(), *Error);
		return;
	}
#if WITH_EDITOR
	const FScopedTransaction Transaction(NSLOCTEXT("HouseGenerator", "Generate", "Generate house"), !GetWorld()->IsGameWorld());
#endif
	Modify();
	for (UInstancedStaticMeshComponent* Component : {Floors.Get(), Walls.Get(), Roof.Get()})
	{
		Component->Modify();
		Component->ClearInstances();
		Component->SetStaticMesh(Mesh);
	}
	ClearLabels();
	for (int32 Index = 0; Index < FurnitureMeshes.Num(); ++Index)
	{
		UInstancedStaticMeshComponent* Component = FurnitureMeshes[Index];
		Component->Modify();
		Component->ClearInstances();
		UStaticMesh* FurnitureMesh = DefaultBlockMesh;
		if (Style)
			if (const TObjectPtr<UStaticMesh>* Override = Style->FurnitureMeshes.Find(static_cast<EHouseFurnitureType>(Index)))
				if (*Override) FurnitureMesh = Override->Get();
		if (Palette)
			if (const TObjectPtr<UStaticMesh>* Override = Palette->Meshes.Find(static_cast<EHouseFurnitureType>(Index)))
				if (*Override) FurnitureMesh = Override->Get();
		Component->SetStaticMesh(FurnitureMesh);
		Component->SetMaterial(0, Palette && Palette->Material ? Palette->Material.Get() : Style && Style->FurnitureMaterial ? Style->FurnitureMaterial.Get() : FurnitureMesh->GetMaterial(0));
	}
	Floors->SetMaterial(0, Style && Style->FloorMaterial ? Style->FloorMaterial.Get() : Mesh->GetMaterial(0));
	Walls->SetMaterial(0, Style && Style->WallMaterial ? Style->WallMaterial.Get() : Mesh->GetMaterial(0));
	Roof->SetMaterial(0, Style && Style->RoofMaterial ? Style->RoofMaterial.Get() : Mesh->GetMaterial(0));
	Layout = MoveTemp(NewLayout);
	GeneratedSettings = EffectiveSettings;
	GeneratedSeed = EffectiveSettings.Seed;
	NavigationSummary = Layout.Navigation.Summary;
	GenerationSummary = FString::Printf(TEXT("Input seed %d -> generated seed %d; %d attempt(s)."), Settings.Seed, GeneratedSeed, Attempts);
	if (RequestedRoomCount != GeneratedSettings.RoomCount)
		GenerationSummary += FString::Printf(TEXT(" Used %d rooms (requested %d) to satisfy difficulty/household."), GeneratedSettings.RoomCount, RequestedRoomCount);
	if (Settings.WidthCells != GeneratedSettings.WidthCells || Settings.DepthCells != GeneratedSettings.DepthCells)
		GenerationSummary += FString::Printf(TEXT(" Expanded envelope from %d x %d to %d x %d cells; disable Allow Larger Footprint for strict limits."),
			Settings.WidthCells, Settings.DepthCells, GeneratedSettings.WidthCells, GeneratedSettings.DepthCells);
	GenerationSummary += TEXT(" Your input controls are unchanged.");
	SizeSummary = Layout.SizeSummary;
	LastError.Reset();
	BuildGeometry();
	SetRoofHidden(bHideRoof);
	UE_LOG(LogHouseGenerator, Display, TEXT("%s: generated seed %d, %d rooms, %d furniture items (%d could not fit)."),
		*GetName(), GeneratedSeed, Layout.Rooms.Num(), Layout.Furniture.Num(), Layout.SkippedFurnitureCount);
	UE_LOG(LogHouseGenerator, Display, TEXT("%s"), *NavigationSummary);
	UE_LOG(LogHouseGenerator, Display, TEXT("%s"), *GenerationSummary);
}

void AHouseGenerator::ClearHouse()
{
#if WITH_EDITOR
	const FScopedTransaction Transaction(NSLOCTEXT("HouseGenerator", "Clear", "Clear house"), !GetWorld()->IsGameWorld());
#endif
	Modify();
	for (UInstancedStaticMeshComponent* Component : {Floors.Get(), Walls.Get(), Roof.Get()})
	{
		Component->Modify();
		Component->ClearInstances();
	}
	for (UInstancedStaticMeshComponent* Component : FurnitureMeshes)
	{
		Component->Modify();
		Component->ClearInstances();
	}
	ClearLabels();
	Layout = FHouseLayout();
	GeneratedSeed = 0;
	GenerationSummary.Reset();
	NavigationSummary.Reset();
	SizeSummary.Reset();
	LastError.Reset();
}

void AHouseGenerator::ClearLabels()
{
	for (UTextRenderComponent* Label : GeneratedLabels)
	{
		if (!Label) continue;
		Label->Modify();
		RemoveInstanceComponent(Label);
		Label->DestroyComponent();
	}
	GeneratedLabels.Reset();
}

void AHouseGenerator::AddLabel(const FString& Text, const FVector& Position, const FColor& Color, float TextSize)
{
	UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this, NAME_None, RF_Transactional);
	Label->SetupAttachment(GetRootComponent());
	Label->SetRelativeLocationAndRotation(Position, FRotator(90, 0, 0));
	Label->SetText(FText::FromString(Text));
	Label->SetWorldSize(TextSize);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetTextRenderColor(Color);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Label->SetVisibility(bShowLabels);
	AddInstanceComponent(Label);
	Label->RegisterComponent();
	GeneratedLabels.Add(Label);
}

void AHouseGenerator::AddBox(UInstancedStaticMeshComponent* Component, const FVector& Center, const FVector& Size)
{
	if (Size.GetMin() <= UE_SMALL_NUMBER) return;
	const FBox Bounds = Component->GetStaticMesh()->GetBoundingBox();
	const FVector Scale = Size / Bounds.GetSize();
	Component->AddInstance(FTransform(FQuat::Identity, Center - Bounds.GetCenter() * Scale, Scale));
}

void AHouseGenerator::BuildGeometry()
{
	const FHouseGenerationSettings& S = GeneratedSettings;
	for (const FHouseRoom& Room : Layout.Rooms)
	{
		const FVector Center((Room.Min.X + Room.Max.X) * S.CellSize * 0.5f,
			(Room.Min.Y + Room.Max.Y) * S.CellSize * 0.5f, -S.SlabThickness * 0.5f);
		const FVector2D Clear = FHouseSizing::ClearSize(S, Room.Max - Room.Min);
		AddLabel(FString::Printf(TEXT("%d: %s\n%.1f x %.1f m\n%.1f m2"), static_cast<int32>(&Room - Layout.Rooms.GetData()),
			*FHouseHousehold::RoomLabel(Room), Clear.X, Clear.Y, Clear.X * Clear.Y),
			FVector(Center.X, Center.Y, 2), FColor(15, 35, 65), FMath::Clamp(FMath::Min(Clear.X, Clear.Y) * 100.0 / 12.0, 12.0, 25.0));
		AddBox(Floors, Center, FVector((Room.Max.X - Room.Min.X) * S.CellSize,
			(Room.Max.Y - Room.Min.Y) * S.CellSize, S.SlabThickness));
	}
	for (const FHouseWall& Wall : Layout.Walls)
	{
		const bool AlongX = Wall.Start.Y == Wall.End.Y;
		const float Length = (Wall.End.X - Wall.Start.X + Wall.End.Y - Wall.Start.Y) * S.CellSize;
		const FVector Start(Wall.Start.X * S.CellSize, Wall.Start.Y * S.CellSize, 0.0f);
		const FVector Direction = AlongX ? FVector::ForwardVector : FVector::RightVector;
		auto Segment = [&](float From, float To, float Bottom, float Top)
		{
			const FVector Center = Start + Direction * ((From + To) * 0.5f) + FVector(0, 0, (Bottom + Top) * 0.5f);
			AddBox(Walls, Center, AlongX ? FVector(To - From, S.WallThickness, Top - Bottom) :
				FVector(S.WallThickness, To - From, Top - Bottom));
		};
		// Half-thickness end caps close corners/T junctions without duplicating shared walls.
		const float From = -S.WallThickness * 0.5f;
		const float To = Length + S.WallThickness * 0.5f;
		struct FSpan { float Left, Right, Bottom, Top; };
		TArray<FSpan> Openings;
		if (Wall.Opening == EHouseOpening::Door)
			Openings.Add({(Length - S.DoorWidth) * 0.5f, (Length + S.DoorWidth) * 0.5f, 0, S.DoorHeight});
		for (const FHouseWindow& Window : Wall.Windows)
			Openings.Add({Window.Offset - Window.Width * 0.5f, Window.Offset + Window.Width * 0.5f,
				Window.SillHeight, Window.SillHeight + Window.Height});
		// Compatibility with the initial saved example, until the user regenerates it.
		if (Wall.Opening == EHouseOpening::Window && Wall.Windows.IsEmpty())
			Openings.Add({(Length - S.WindowWidth) * 0.5f, (Length + S.WindowWidth) * 0.5f,
				S.WindowSillHeight, S.WindowSillHeight + S.WindowHeight});
		Openings.Sort([](const FSpan& A, const FSpan& B) { return A.Left < B.Left; });
		float Cursor = From;
		for (const FSpan& Opening : Openings)
		{
			Segment(Cursor, Opening.Left, 0, S.WallHeight);
			Segment(Opening.Left, Opening.Right, Opening.Top, S.WallHeight);
			if (Opening.Bottom > 0) Segment(Opening.Left, Opening.Right, 0, Opening.Bottom);
			Cursor = Opening.Right;
		}
		Segment(Cursor, To, 0, S.WallHeight);
	}
	for (const FHouseFurniturePlacement& Item : Layout.Furniture)
	{
		const FHouseFurnitureRule& Rule = S.FurnitureRules[Item.RuleIndex];
		UInstancedStaticMeshComponent* Component = FurnitureMeshes[static_cast<int32>(Item.Type)];
		const FBox Bounds = Component->GetStaticMesh()->GetBoundingBox();
		const FVector Scale = Rule.Size / Bounds.GetSize();
		const FQuat Rotation(FVector::UpVector, Item.QuarterTurns * UE_HALF_PI);
		const FVector Center(Item.Center.X, Item.Center.Y, Rule.Size.Z * 0.5);
		Component->AddInstance(FTransform(Rotation, Center - Rotation.RotateVector(Bounds.GetCenter() * Scale), Scale));
		AddLabel(StaticEnum<EHouseFurnitureType>()->GetDisplayNameTextByValue(static_cast<int64>(Item.Type)).ToString() +
			(S.Household.Profile == EHouseholdProfile::None ? FString() : TEXT(" / ") + StaticEnum<EHouseFurnitureStyle>()->GetDisplayNameTextByValue(static_cast<int64>(S.Household.FurnitureStyle)).ToString()),
			FVector(Item.Center.X, Item.Center.Y, Rule.Size.Z + 2), FColor(10, 20, 35));
	}
	const FIntPoint Actual = FHouseSizing::Footprint(S, Layout);
	AddBox(Roof, FVector(Actual.X * S.CellSize * 0.5f, Actual.Y * S.CellSize * 0.5f,
		S.WallHeight + S.SlabThickness * 0.5f),
		FVector(Actual.X * S.CellSize + S.WallThickness, Actual.Y * S.CellSize + S.WallThickness, S.SlabThickness));
}

void AHouseGenerator::DrawLayoutDebug()
{
	if (!GetWorld()) return;
	const FTransform Transform = GetActorTransform();
	const FHouseNavigationReport Report = FHouseNavigation::Measure(Layout);
	auto CenterOf = [&](int32 ID)
	{
		const FHouseRoom& Room = Layout.Rooms[ID];
		return FVector((Room.Min.X + Room.Max.X) * GeneratedSettings.CellSize * 0.5f,
			(Room.Min.Y + Room.Max.Y) * GeneratedSettings.CellSize * 0.5f, 40);
	};
	for (const FHouseWall& Wall : Layout.Walls)
	{
		if (Wall.Opening != EHouseOpening::Door || Wall.RoomB == INDEX_NONE) continue;
		const int32 A = Report.MainRoute.IndexOfByKey(Wall.RoomA), B = Report.MainRoute.IndexOfByKey(Wall.RoomB);
		const bool bMain = A != INDEX_NONE && B != INDEX_NONE && FMath::Abs(A - B) == 1;
		const int32 From = bMain && A > B ? Wall.RoomB : Wall.RoomA;
		const int32 To = From == Wall.RoomA ? Wall.RoomB : Wall.RoomA;
		const FVector Door((Wall.Start.X + Wall.End.X) * GeneratedSettings.CellSize * 0.5f,
			(Wall.Start.Y + Wall.End.Y) * GeneratedSettings.CellSize * 0.5f, 40);
		const FColor Color = bMain ? FColor::Cyan : FColor(150, 150, 150);
		DrawDebugLine(GetWorld(), Transform.TransformPosition(CenterOf(From)), Transform.TransformPosition(Door), Color, false, 30, 0, bMain ? 8 : 3);
		if (bMain)
			DrawDebugDirectionalArrow(GetWorld(), Transform.TransformPosition(Door), Transform.TransformPosition(CenterOf(To)), 35, Color, false, 30, 0, 8);
		else
			DrawDebugLine(GetWorld(), Transform.TransformPosition(Door), Transform.TransformPosition(CenterOf(To)), Color, false, 30, 0, 3);
	}
	for (int32 Index = 0; Index < Layout.Rooms.Num(); ++Index)
	{
		const FHouseRoom& Room = Layout.Rooms[Index];
		const float Cell = GeneratedSettings.CellSize;
		const FVector Center((Room.Min.X + Room.Max.X) * Cell * 0.5f, (Room.Min.Y + Room.Max.Y) * Cell * 0.5f, 15.0f);
		const FVector Extent((Room.Max.X - Room.Min.X) * Cell * 0.5f - 15.0f,
			(Room.Max.Y - Room.Min.Y) * Cell * 0.5f - 15.0f, 5.0f);
		const FColor Color = FLinearColor::MakeFromHSV8(static_cast<uint8>(Index * 67), 220, 255).ToFColor(true);
		DrawDebugBox(GetWorld(), Transform.TransformPosition(Center), Extent * Transform.GetScale3D().GetAbs(),
			Transform.GetRotation(), Color, false, 30.0f, 0, 4.0f);
		const bool bDeadEnd = Report.DeadEndRooms.Contains(Index);
		DrawDebugString(GetWorld(), Transform.TransformPosition(Center), FString::Printf(TEXT("%d: %s | %d doors%s%s%s"), Index,
			*FHouseHousehold::RoomLabel(Room), Report.RoomDoorDepths[Index],
			bDeadEnd ? TEXT(" | dead end") : TEXT(""), Index == Report.DeepestRoom ? TEXT(" | deepest") : TEXT(""),
			Index == Report.EntranceRoom ? TEXT(" | entrance") : TEXT("")), nullptr, bDeadEnd ? FColor::Red : Color, 30.0f);
	}
	if (Layout.Rooms.Num())
		DrawDebugString(GetWorld(), Transform.TransformPosition(FVector(0, -80, 100)),
			FHouseNavigation::Describe(GeneratedSettings, Report) + TEXT(" Cyan: main door route. Red: dead ends."), nullptr, FColor::White, 30);
}
