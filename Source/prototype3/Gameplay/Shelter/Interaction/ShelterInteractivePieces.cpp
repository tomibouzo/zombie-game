#include "Gameplay/Shelter/Interaction/ShelterInteractivePieces.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
constexpr float DoorOpeningWidth = 140.f;
constexpr float DoorOpeningHeight = 220.f;

void ConfigureBox(UStaticMeshComponent& Mesh, USceneComponent& Parent)
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Mesh.SetupAttachment(&Parent);
	Mesh.SetMobility(EComponentMobility::Movable);
	Mesh.SetStaticMesh(Cube.Object);
	Mesh.SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Mesh.SetCanEverAffectNavigation(false);
}

void SizeBox(UStaticMeshComponent& Mesh, const FVector& Center, const FVector& Size)
{
	Mesh.SetRelativeLocation(Center);
	Mesh.SetRelativeScale3D(Size / 100.0);
}

void SetPieceColor(UStaticMeshComponent& Mesh, const FLinearColor& Color)
{
	auto* Material = Cast<UMaterialInstanceDynamic>(Mesh.GetMaterial(0));
	if (!Material) Material = Mesh.CreateDynamicMaterialInstance(0);
	if (Material) Material->SetVectorParameterValue(TEXT("Color"), Color);
}

FLinearColor PreviewColor(bool bValid)
{
	return bValid ? FLinearColor(0.1f, 0.75f, 0.3f) : FLinearColor(0.85f, 0.12f, 0.1f);
}
}

AShelterDoorPiece::AShelterDoorPiece()
{
	LeftFrame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftFrame"));
	RightFrame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightFrame"));
	Header = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Header"));
	Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));
	Leaf = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Leaf"));

	Hinge->SetupAttachment(GetRootComponent());
	ConfigureBox(*LeftFrame, *GetRootComponent());
	ConfigureBox(*RightFrame, *GetRootComponent());
	ConfigureBox(*Header, *GetRootComponent());
	ConfigureBox(*Leaf, *Hinge);
}

void AShelterDoorPiece::InitializePiece(const UShelterPieceDefinition* InDefinition, const FShelterModule& InModule)
{
	Super::InitializePiece(InDefinition, InModule);

	const double Width = InModule.Width;
	const double Height = InModule.Height;
	const double Thickness = FMath::Clamp<double>(InModule.WallThickness, 5.0, FMath::Min(Width, Height) / 4.0);
	const double OpeningWidth = FMath::Min<double>(DoorOpeningWidth, Width - 2.0 * Thickness);
	const double OpeningHeight = FMath::Min<double>(DoorOpeningHeight, Height - Thickness);
	const double SideWidth = (Width - OpeningWidth) / 2.0;
	const double SideCenter = (OpeningWidth + SideWidth) / 2.0;
	SizeBox(*LeftFrame, FVector(-SideCenter, 0, Height / 2.0), FVector(SideWidth, Thickness, Height));
	SizeBox(*RightFrame, FVector(SideCenter, 0, Height / 2.0), FVector(SideWidth, Thickness, Height));
	SizeBox(*Header, FVector(0, 0, (Height + OpeningHeight) / 2.0), FVector(OpeningWidth, Thickness, Height - OpeningHeight));
	Hinge->SetRelativeLocation(FVector(-OpeningWidth / 2.0, 0, 0));
	SizeBox(*Leaf, FVector(OpeningWidth / 2.0, 0, OpeningHeight / 2.0),
		FVector(OpeningWidth, FMath::Min(Thickness, 8.0), OpeningHeight - 2.0));
}

void AShelterDoorPiece::SetPreview(bool bValid)
{
	Super::SetPreview(bValid);
	for (auto* Mesh : { LeftFrame.Get(), RightFrame.Get(), Header.Get(), Leaf.Get() })
		SetPieceColor(*Mesh, PreviewColor(bValid));
}

bool AShelterDoorPiece::CanInteract(const AActor* Instigator) const
{
	return IsValid(Instigator) && !IsPreview();
}

FText AShelterDoorPiece::GetInteractionPrompt(const AActor* Instigator) const
{
	if (!bOpen) return NSLOCTEXT("Interaction", "OpenDoor", "Open door");
	return IsDoorwayOccupied()
		? NSLOCTEXT("Interaction", "DoorBlocked", "Doorway is blocked")
		: NSLOCTEXT("Interaction", "CloseDoor", "Close door");
}

bool AShelterDoorPiece::IsDoorwayOccupied() const
{
	// Use the closed heading even while the leaf is open.
	const FVector Center = GetActorTransform().TransformPosition(Hinge->GetRelativeLocation() + Leaf->GetRelativeLocation());
	const FVector Extent = Leaf->GetComponentScale().GetAbs() * 50.0;
	const FCollisionObjectQueryParams Pawns(ECC_Pawn);
	const FCollisionQueryParams Query(SCENE_QUERY_STAT(ShelterDoorway), false, this);
	return GetWorld()->OverlapAnyTestByObjectType(Center, GetActorQuat(), Pawns, FCollisionShape::MakeBox(Extent), Query);
}

void AShelterDoorPiece::Interact(AActor* Instigator)
{
	if (!CanInteract(Instigator) || (bOpen && IsDoorwayOccupied())) return;
	bOpen = !bOpen;
	Hinge->SetRelativeRotation(FRotator(0, bOpen ? 90.f : 0.f, 0));
}

AShelterLampPiece::AShelterLampPiece()
{
	Stand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Stand"));
	Shade = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shade"));
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	ConfigureBox(*Stand, *GetRootComponent());
	ConfigureBox(*Shade, *GetRootComponent());
	SizeBox(*Stand, FVector(0, 0, 45), FVector(18, 18, 90));
	SizeBox(*Shade, FVector(0, 0, 100), FVector(55, 55, 20));

	Light->SetupAttachment(GetRootComponent());
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetRelativeLocation(FVector(0, 0, 120));
	Light->SetIntensity(1400.f);
	Light->SetAttenuationRadius(650.f);
	Light->SetLightColor(FLinearColor(1.f, .82f, .59f));
	Light->SetVisibility(false);
}

void AShelterLampPiece::SetPreview(bool bValid)
{
	Super::SetPreview(bValid);
	Light->SetVisibility(false);
	SetPieceColor(*Stand, PreviewColor(bValid));
	SetPieceColor(*Shade, PreviewColor(bValid));
}

bool AShelterLampPiece::CanInteract(const AActor* Instigator) const
{
	return IsValid(Instigator) && !IsPreview();
}

bool AShelterLampPiece::IsLit() const
{
	return Light->IsVisible();
}

FText AShelterLampPiece::GetInteractionPrompt(const AActor* Instigator) const
{
	return IsLit() ? NSLOCTEXT("Interaction", "LampOff", "Turn off lamp") : NSLOCTEXT("Interaction", "LampOn", "Turn on lamp");
}

void AShelterLampPiece::Interact(AActor* Instigator)
{
	if (!CanInteract(Instigator)) return;
	const bool bLightOn = !IsLit();
	Light->SetVisibility(bLightOn);
	SetPieceColor(*Shade, bLightOn ? FLinearColor(1.f, .8f, .45f) : FLinearColor(.25f, .23f, .2f));
}
