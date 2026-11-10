#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Gameplay/Procedural/HouseLayout.h"
#include "HouseGenerationPreset.generated.h"

/** Inclusive decimal bounds. Equal bounds select a fixed value. */
USTRUCT(BlueprintType)
struct PROTOTYPE3_API FHouseFloatRange
{
	GENERATED_BODY()
	FHouseFloatRange() = default;
	FHouseFloatRange(float InMin, float InMax) : Min(InMin), Max(InMax) {}
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Range", meta=(ClampMin="0"))
	float Min = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Range", meta=(ClampMin="0"))
	float Max = 0;
};

/** Seed-resolved values of a preset. Request carries the chosen size as exact (equal-bound) ranges. */
struct PROTOTYPE3_API FHousePresetResult
{
	FHouseGenerationSettings Request;
	/** Width/depth/room combinations tested before one built (1 = the first seeded choice worked). */
	int32 SizeCandidatesTried = 0;
	int32 WidthCells = 0;
	int32 DepthCells = 0;
	int32 RoomCount = 0;
	int32 ZombieCount = 0;
	int32 ItemCount = 0;
	FString Describe() const;
};

/**
 * A selectable building model: fixed construction rules plus min/max ranges that the seed resolves.
 * The same seed and preset always produce the same values. Select one on a House Builder or Encounter;
 * leave it empty to use the actor's own (Custom) settings.
 */
UCLASS(BlueprintType)
class PROTOTYPE3_API UHouseGenerationPreset : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Preset", meta=(MultiLine="true"))
	FText Description;

	/** Fixed values: layout pattern, household, dimensions, room sizing, furniture catalogue and room rules.
	 * Seed, size, room count, extra connection chance and furnishing density here are replaced by the ranges below. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Preset|Fixed")
	FHouseGenerationSettings FixedSettings;

	/** Actual footprint width/depth in cells and total room count (including hallways). The seed tries the
	 * combinations inside these ranges in a seeded order and uses the first one the layout pattern can build. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Preset|Ranges")
	FHouseGenerationRanges Size;

	/** Chance of optional extra doors (alternate routes/loops). Used by office and original layouts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Preset|Ranges")
	FHouseFloatRange ExtraConnectionChance = FHouseFloatRange(0.2f, 0.2f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Preset|Ranges")
	EHouseFurnishingDensity MinFurnishingDensity = EHouseFurnishingDensity::Standard;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Preset|Ranges")
	EHouseFurnishingDensity MaxFurnishingDensity = EHouseFurnishingDensity::Standard;

	/** Zombies per room, excluding the entrance room. The result is rounded, then clamped to Min/Max Zombies. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Preset|Population")
	FHouseFloatRange ZombiesPerRoom = FHouseFloatRange(0.5f, 0.5f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Preset|Population", meta=(ClampMin="0", ClampMax="32"))
	int32 MinZombies = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Preset|Population", meta=(ClampMin="0", ClampMax="32"))
	int32 MaxZombies = 32;

	/** Collectibles per room, including the entrance room. Rounded, then clamped to Min/Max Items. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Preset|Population")
	FHouseFloatRange ItemsPerRoom = FHouseFloatRange(1.0f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Preset|Population", meta=(ClampMin="0", ClampMax="64"))
	int32 MinItems = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Preset|Population", meta=(ClampMin="0", ClampMax="64"))
	int32 MaxItems = 64;

	bool Validate(FString& OutError) const;
	/** Resolve every ranged value from Seed. Does not mutate the preset. */
	bool Resolve(int32 Seed, FHousePresetResult& Out, FString& OutError) const;
	/** Starting point when switching an actor from this preset to Custom: fixed values, the same size
	 * ranges, midpoint connection chance/density and the population at the midpoint size. */
	FHouseGenerationSettings MakeCustomSettings(int32 Seed, int32& OutZombies, int32& OutItems) const;

	/** Bound on size combinations tested per resolution; each failed test costs one bounded layout search. */
	static constexpr int32 MaxSizeCandidates = 64;

	/** Population for a resolved room count; shared by Resolve and MakeCustomSettings. */
	static int32 Population(float PerRoom, int32 Rooms, int32 Minimum, int32 Maximum);
};

/** Built-in starting values for the shipped preset assets (and tests when an asset is missing). */
namespace HousePresetDefaults
{
	PROTOTYPE3_API void SmallOffice(UHouseGenerationPreset& Preset);
	PROTOTYPE3_API void CompactHouse(UHouseGenerationPreset& Preset);
	PROTOTYPE3_API void FamilyHouse(UHouseGenerationPreset& Preset);
	/** Asset object paths, e.g. /Game/Procedural/Presets/DA_HousePreset_SmallOffice. */
	PROTOTYPE3_API const TArray<FString>& AssetPaths();
	using FInit = void(*)(UHouseGenerationPreset&);
	PROTOTYPE3_API const TArray<FInit>& Initializers();
	/** The saved asset for Index, or a transient copy of its built-in values when the asset does not exist. */
	PROTOTYPE3_API UHouseGenerationPreset* LoadOrDefault(int32 Index, bool* bOutFromAsset = nullptr);
}
