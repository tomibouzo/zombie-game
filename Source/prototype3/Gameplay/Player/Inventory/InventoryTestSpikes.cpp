#include "Gameplay/Player/Inventory/InventoryTestSpikes.h"
#include "Gameplay/Player/Vitals/PlayerVitalsComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/DamageEvents.h"
#include "UObject/ConstructorHelpers.h"

AInventoryTestSpikes::AInventoryTestSpikes()
{
	PrimaryActorTick.bCanEverTick = true;
	Area = CreateDefaultSubobject<UBoxComponent>(TEXT("DamageArea"));
	SetRootComponent(Area); Area->SetBoxExtent(FVector(65,65,25));
	Area->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	for (int32 I = 0; I < 9; ++I)
	{
		auto* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Spike%d"), I));
		Mesh->SetupAttachment(Area); Mesh->SetStaticMesh(Cone.Object);
		Mesh->SetRelativeLocation(FVector((I%3-1)*40,(I/3-1)*40,7));
		Mesh->SetRelativeScale3D(FVector(.25,.25,.20));
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	auto* Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Area); Label->SetRelativeLocation(FVector(0,0,75));
	Label->SetHorizontalAlignment(EHTA_Center); Label->SetWorldSize(14);
	Label->SetText(FText::FromString(TEXT("TEST SPIKES\n10 damage / second\nMinimum: 1 HP")));
	Label->SetTextRenderColor(FColor::Red);
}
float AInventoryTestSpikes::Hurt(AActor* Target)
{
	auto* Vitals = Target ? Target->FindComponentByClass<UPlayerVitalsComponent>() : nullptr;
	if (!Vitals || !Vitals->IsAlive()) return 0;
	const float Amount = FMath::Min(FMath::Max(DamagePerPulse,0.f), FMath::Max(Vitals->GetCurrentHealth()-1,0.f));
	return Amount > 0 ? Target->TakeDamage(Amount, FDamageEvent(), nullptr, this) : 0;
}
void AInventoryTestSpikes::Tick(float Delta)
{
	Super::Tick(Delta);
	Elapsed += Delta;
	if (Elapsed < FMath::Max(PulseSeconds,.1f)) return;
	Elapsed = 0;
	TArray<AActor*> Targets; Area->GetOverlappingActors(Targets);
	for (auto* Target : Targets) Hurt(Target);
}
