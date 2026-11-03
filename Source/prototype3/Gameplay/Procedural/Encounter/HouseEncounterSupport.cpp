#include "Gameplay/Procedural/Encounter/HouseEncounterSupport.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

AHouseZombiePlaceholder::AHouseZombiePlaceholder()
{
	GetCapsuleComponent()->InitCapsuleSize(34, 96);
	AutoPossessAI = EAutoPossessAI::Disabled;
	GetCapsuleComponent()->SetCanEverAffectNavigation(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	auto* Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderBody"));
	Body->SetupAttachment(GetRootComponent());
	Body->SetStaticMesh(Cube.Object);
	Body->SetRelativeScale3D(FVector(.45, .4, 1.5));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCanEverAffectNavigation(false);
	auto* Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PlaceholderLabel"));
	Label->SetupAttachment(GetRootComponent());
	Label->SetRelativeLocation(FVector(0, 0, 105));
	Label->SetText(FText::FromString(TEXT("ZOMBIE\nPlaceholder")));
	Label->SetTextRenderColor(FColor(220, 70, 70));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(18);
}

void AHouseEncounterNavMesh::PostInitProperties()
{
	Super::PostInitProperties();
	RuntimeGeneration = ERuntimeGenerationType::Dynamic;
	bForceRebuildOnLoad = true;
	if (!HasAnyFlags(RF_ClassDefaultObject)) SetConfig(UHouseEncounterNavigationSystem::AgentConfig());
}

FNavDataConfig UHouseEncounterNavigationSystem::AgentConfig()
{
	FNavDataConfig Agent(35, 192);
	Agent.Name = TEXT("HouseEncounter");
	Agent.SetNavDataClass(AHouseEncounterNavMesh::StaticClass());
	Agent.SetPreferredNavData(AHouseEncounterNavMesh::StaticClass());
	return Agent;
}

void UHouseEncounterNavigationSystem::PostInitProperties()
{
	SupportedAgents = {AgentConfig()};
	Super::PostInitProperties();
}

AHouseEncounterNavBounds::AHouseEncounterNavBounds()
{
	auto* Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("NavigationExtent"));
	Bounds->SetupAttachment(GetRootComponent());
	Bounds->SetBoxExtent(FVector(2200, 2200, 400));
	Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bounds->SetCanEverAffectNavigation(false);
}
