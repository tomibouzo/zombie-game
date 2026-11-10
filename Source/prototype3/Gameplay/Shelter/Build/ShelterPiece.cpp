#include "Gameplay/Shelter/Build/ShelterPiece.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

const FPrimaryAssetType UShelterPieceDefinition::PieceAssetType(TEXT("ShelterPiece"));

bool UShelterPieceDefinition::IsValidDefinition() const
{
	const bool bNumbers = FMath::IsFinite(OpeningWidth) && FMath::IsFinite(OpeningHeight) && FMath::IsFinite(OpeningSillHeight)
		&& OpeningWidth >= 0 && OpeningHeight >= 0 && OpeningSillHeight >= 0
		&& !CellObjectSize.ContainsNaN() && CellObjectSize.GetMin() > 0 && CellObjectSize.GetMax() <= 1000;
	return !PieceId.IsNone() && !DisplayName.IsEmpty() && !Category.IsEmpty() && bNumbers
		&& (SlotKind == EShelterSlotKind::Edge || SlotKind == EShelterSlotKind::Cell);
}

TSubclassOf<AShelterPiece> UShelterPieceDefinition::ResolveActorClass() const
{
	return ActorClass ? ActorClass : TSubclassOf<AShelterPiece>(AShelterPlaceholderPiece::StaticClass());
}

FPrimaryAssetId UShelterPieceDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(PieceAssetType, PieceId.IsNone() ? GetFName() : PieceId);
}

AShelterPiece::AShelterPiece()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AShelterPiece::InitializePiece(const UShelterPieceDefinition* InDefinition, const FShelterModule& InModule)
{
	Definition = const_cast<UShelterPieceDefinition*>(InDefinition);
	Module = InModule;
}

void AShelterPiece::SetPreview(bool bValid)
{
	bPreview = true;
	SetActorEnableCollision(false);
}

AShelterPlaceholderPiece::AShelterPlaceholderPiece()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Basic(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	CubeMesh = Cube.Object;
	BaseMaterial = Basic.Object;
}

void AShelterPlaceholderPiece::AddBox(const FVector& Center, const FVector& Size)
{
	if (!CubeMesh || Size.GetMin() <= UE_KINDA_SMALL_NUMBER) return;
	auto* Mesh = NewObject<UStaticMeshComponent>(this);
	AddInstanceComponent(Mesh);
	Mesh->SetupAttachment(GetRootComponent());
	Mesh->SetStaticMesh(CubeMesh);
	if (Material) Mesh->SetMaterial(0, Material);
	Mesh->SetRelativeLocation(Center);
	Mesh->SetRelativeScale3D(Size / 100.0);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->RegisterComponent();
}

void AShelterPlaceholderPiece::InitializePiece(const UShelterPieceDefinition* InDefinition, const FShelterModule& InModule)
{
	Super::InitializePiece(InDefinition, InModule);
	if (!InDefinition) return;
	if (BaseMaterial)
	{
		Material = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		Material->SetVectorParameterValue(TEXT("Color"), InDefinition->PlaceholderColor);
	}
	if (InDefinition->SlotKind == EShelterSlotKind::Cell)
	{
		const FVector Size = InDefinition->CellObjectSize.ComponentMin(FVector(InModule.Width - 10, InModule.Width - 10, InModule.Height));
		AddBox(FVector(0, 0, Size.Z * 0.5), Size);
		return;
	}
	const float W = InModule.Width, H = InModule.Height, T = InModule.WallThickness;
	// The opening is clamped inside the panel so a mis-authored asset still yields a closed frame.
	const float OpenW = FMath::Clamp(InDefinition->OpeningWidth, 0.f, W - 2 * T);
	const float Sill = FMath::Clamp(InDefinition->OpeningSillHeight, 0.f, H - T);
	const float OpenH = FMath::Clamp(InDefinition->OpeningHeight, 0.f, H - T - Sill);
	if (OpenW <= 1 || OpenH <= 1)
	{
		AddBox(FVector(0, 0, H * 0.5), FVector(W, T, H));
		return;
	}
	const float Side = (W - OpenW) * 0.5f;
	AddBox(FVector(-(OpenW + Side) * 0.5f, 0, H * 0.5f), FVector(Side, T, H));
	AddBox(FVector((OpenW + Side) * 0.5f, 0, H * 0.5f), FVector(Side, T, H));
	const float Top = Sill + OpenH;
	AddBox(FVector(0, 0, (Top + H) * 0.5f), FVector(OpenW, T, H - Top));
	if (Sill > 1) AddBox(FVector(0, 0, Sill * 0.5f), FVector(OpenW, T, Sill));
}

void AShelterPlaceholderPiece::SetPreview(bool bValid)
{
	Super::SetPreview(bValid);
	if (Material) Material->SetVectorParameterValue(TEXT("Color"), bValid ? FLinearColor(0.1f, 0.75f, 0.3f) : FLinearColor(0.85f, 0.12f, 0.1f));
}
