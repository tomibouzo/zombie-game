#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Gameplay/Procedural/HouseLayout.h"
#include "HouseStyle.generated.h"

class UMaterialInterface;
class UStaticMesh;

USTRUCT(BlueprintType)
struct PROTOTYPE3_API FHouseFurniturePalette
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Furniture")
	TMap<EHouseFurnitureType, TObjectPtr<UStaticMesh>> Meshes;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Furniture")
	TObjectPtr<UMaterialInterface> Material;
};

/** Blockout style. A replacement mesh must be a solid box with simple collision.
 * Its bounds are fitted to each piece; authored modular wall kits are future work. */
UCLASS(BlueprintType)
class PROTOTYPE3_API UHouseStyle : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Optional; defaults to the Engine basic cube. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="House")
	TObjectPtr<UStaticMesh> BlockMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="House")
	TObjectPtr<UMaterialInterface> FloorMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="House")
	TObjectPtr<UMaterialInterface> WallMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="House")
	TObjectPtr<UMaterialInterface> RoofMaterial;

	/** Optional blockout replacements, with simple collision. Bounds are fitted to each furniture rule's size. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Furniture")
	TMap<EHouseFurnitureType, TObjectPtr<UStaticMesh>> FurnitureMeshes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Furniture")
	TObjectPtr<UMaterialInterface> FurnitureMaterial;

	/** Style-specific catalogue; missing entries fall back to Furniture Meshes then labelled cubes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Furniture")
	TMap<EHouseFurnitureStyle, FHouseFurniturePalette> HouseholdPalettes;
};
