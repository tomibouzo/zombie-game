#pragma once

#include "CoreMinimal.h"
#include "HouseEncounterTypes.generated.h"

UENUM(BlueprintType)
enum class EHouseEncounterState : uint8
{
	Idle, Preparing, Active, Choosing, Complete, Failed
};

/** A decision belongs to this encounter only. Complete is terminal, even for the selected option. */
USTRUCT(BlueprintType)
struct PROTOTYPE3_API FHouseExitDecision
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bOffered = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName Selected;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FName> Options;
	bool Offer(const TArray<FName>& InOptions);
	bool Choose(FName Choice);
	bool IsAvailable(FName Choice) const { return bOffered && Selected.IsNone() && Options.Contains(Choice); }
};

USTRUCT(BlueprintType)
struct PROTOTYPE3_API FHouseEncounterPlacementReport
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Attempts = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 BestZombies = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 BestItems = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 GeometryRejections = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 AccessRejections = 0;
	/** Search results describe attempts, not the theoretical capacity of a building. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString Summary;
};

USTRUCT(BlueprintType)
struct PROTOTYPE3_API FHouseEncounterPlacement
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector PlayerFeet = FVector::ZeroVector;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector ExitFeet = FVector::ZeroVector;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FVector> ZombieFeet;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FVector> ItemFeet;
	/** Clear, reachable standing positions within pickup range, one per item. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FVector> ItemAccessFeet;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FName> ItemTypes;
};
