#pragma once
#include "CoreMinimal.h"
#include "Gameplay/Items/ItemInstance.h"
#include "InventoryTypes.generated.h"

class UItemDefinition;

UENUM(BlueprintType)
enum class EInventoryResult : uint8
{
	Success, InvalidItem, InvalidProfile, InvalidPocket, InvalidRotation,
	DuplicateId, NotFound, OutOfBounds, Occupied, TooHeavy, Incompatible, InUse
};

/** Filled convex polygon. A union of parts represents concave silhouettes and holes. */
USTRUCT(BlueprintType)
struct PROTOTYPE3_API FInventoryShapePart
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
	TArray<FVector2D> Vertices;
	bool IsValid() const;
	bool Contains(FVector2D Point) const;
	bool Overlaps(const FInventoryShapePart& Other) const;
};

/** Explicit silhouette, independent of item art and the shared item contract. */
USTRUCT(BlueprintType)
struct PROTOTYPE3_API FInventoryItemProfile
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
	FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
	TObjectPtr<UItemDefinition> Definition = nullptr;
	/** Local coordinates centered on the silhouette's bounding box. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
	TArray<FInventoryShapePart> ShapeParts;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
	bool bAllowRotation = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
	bool bProvisional = true;
	bool IsValid() const;
	TArray<FInventoryShapePart> GetTransformedParts(FVector2D Center, double AngleDegrees) const;
	bool Contains(FVector2D Point, FVector2D Center, double AngleDegrees) const;
};

USTRUCT(BlueprintType)
struct PROTOTYPE3_API FInventoryPocket
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
	FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
	FVector2D Size = FVector2D(560, 560);
	/** Zero means unlimited. This limits one object, not the combined pocket mass. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
	double MaxItemMassKg = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
	bool bAllowFirearms = true;
};

/** Position is the center; positive angles turn clockwise in the UI's downward Y axis. */
USTRUCT(BlueprintType)
struct PROTOTYPE3_API FInventoryEntry
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	FItemInstance Item;
	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	FName ProfileId;
	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	FName PocketId;
	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	FVector2D Position = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	double AngleDegrees = 0;
};

namespace InventoryGeometry
{
	// Numerical tolerance in logical units, not a gameplay gap or packing margin.
	constexpr double Tolerance = 1.e-7;
	PROTOTYPE3_API bool IsFinite(FVector2D Point);
	PROTOTYPE3_API double NormalizeAngle(double Degrees);
}
