#pragma once

#include "CoreMinimal.h"
#include "HouseLayout.generated.h"

UENUM(BlueprintType)
enum class EHouseRoomType : uint8
{
	LivingRoom,
	Kitchen,
	Bedroom,
	Bathroom,
	Hallway
};

/** Household categories are independent of navigation difficulty. None preserves old maps. */
UENUM(BlueprintType)
enum class EHouseholdProfile : uint8 { None, SingleResident, Couple, Family, RetiredCouple, WorkingFromHomeCouple };
UENUM(BlueprintType)
enum class EHouseFurnitureStyle : uint8 { Traditional, Modern, MixedSecondhand };
UENUM(BlueprintType)
// Retained for loading older saved settings; generation always uses Settled.
enum class EHouseOccupancy : uint8 { Settled, MovingIn UMETA(Hidden), MovingOut UMETA(Hidden), Vacant UMETA(Hidden) };
UENUM(BlueprintType)
enum class EHouseFurnishingDensity : uint8 { Sparse, Standard, Full };
/** Purpose refines a private room without changing its hallway-access rule. */
UENUM(BlueprintType)
enum class EHouseRoomPurpose : uint8 { Default, PrimaryBedroom, GuestBedroom, ChildBedroom, Office, Storage };

USTRUCT(BlueprintType)
struct PROTOTYPE3_API FHouseRoomOverride
{
	GENERATED_BODY()
	/** Stable ID shown in room labels. Applies to the current seed/layout. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Household", meta=(ClampMin="0"))
	int32 RoomIndex = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Household")
	EHouseRoomPurpose Purpose = EHouseRoomPurpose::GuestBedroom;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Household")
	bool bOverrideDensity = false;
	/** Leave this room without movable furniture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Household")
	bool bUnfurnished = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Household")
	EHouseFurnishingDensity Density = EHouseFurnishingDensity::Standard;
};

USTRUCT(BlueprintType)
struct PROTOTYPE3_API FHouseholdSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Household")
	EHouseholdProfile Profile = EHouseholdProfile::None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Household")
	EHouseFurnitureStyle FurnitureStyle = EHouseFurnitureStyle::Modern;
	UPROPERTY()
	EHouseOccupancy Occupancy = EHouseOccupancy::Settled;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Household")
	EHouseFurnishingDensity Density = EHouseFurnishingDensity::Standard;
	/** Number of separate children's bedrooms required by Family. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Household", meta=(ClampMin="1", ClampMax="4"))
	int32 ChildrenBedrooms = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Household")
	TArray<FHouseRoomOverride> RoomOverrides;
};

UENUM(BlueprintType)
enum class EHouseLayoutPattern : uint8
{
	LegacyRandom UMETA(DisplayName="Original Random Layout"),
	CentralHub,
	DeepBranches
};

UENUM(BlueprintType)
enum class EHouseNavigationDifficulty : uint8
{
	Easy,
	Normal,
	Hard,
	Custom
};

USTRUCT(BlueprintType)
struct PROTOTYPE3_API FHouseNavigationSettings
{
	GENERATED_BODY()

	/** Original mode preserves existing maps/settings. Planned modes count hallways as rooms. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Navigation")
	EHouseLayoutPattern Pattern = EHouseLayoutPattern::LegacyRandom;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Navigation", meta=(EditCondition="Pattern != EHouseLayoutPattern::LegacyRandom"))
	EHouseNavigationDifficulty Difficulty = EHouseNavigationDifficulty::Normal;

	/** Exact deepest room distance in interior doorways, excluding the exterior entrance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Navigation", meta=(ClampMin="2", ClampMax="12", EditCondition="Pattern != EHouseLayoutPattern::LegacyRandom && Difficulty == EHouseNavigationDifficulty::Custom"))
	int32 CustomRouteDepth = 3;

	/** Reject a custom plan exceeding this count; the entrance room is not counted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Navigation", meta=(ClampMin="3", ClampMax="31", EditCondition="Pattern != EHouseLayoutPattern::LegacyRandom && Difficulty == EHouseNavigationDifficulty::Custom"))
	int32 CustomMaxDeadEnds = 31;
};

USTRUCT(BlueprintType)
struct PROTOTYPE3_API FHouseNavigationReport
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Navigation")
	FString Summary;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Navigation")
	int32 EntranceRoom = INDEX_NONE;

	/** A farthest bedroom when possible. This is a navigation marker, not a spawned objective. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Navigation")
	int32 DeepestRoom = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Navigation")
	int32 MaxDoorDepth = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Navigation")
	float AverageDoorDepth = 0;

	/** Rooms with at least three interior door connections. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Navigation")
	int32 JunctionCount = 0;

	/** Number of independent cycles, not number of possible routes. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Navigation")
	int32 LoopCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Navigation")
	TArray<int32> RoomDoorDepths;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Navigation")
	TArray<int32> DeadEndRooms;

	/** Entrance-to-deepest room IDs. Debug lines show room connections, not a navmesh path. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Navigation")
	TArray<int32> MainRoute;
};

UENUM(BlueprintType)
enum class EHouseFurnitureType : uint8
{
	Sofa,
	CoffeeTable,
	KitchenCounter,
	Fridge,
	Bed,
	Wardrobe,
	Sink,
	Toilet,
	SingleBed,
	Desk,
	Chair,
	BedsideTable,
	ToyBox,
	StorageShelf,
	Count UMETA(Hidden)
};

/** One optional item per matching room. Sizes use local X=width, Y=depth, Z=height; front is +Y. */
USTRUCT(BlueprintType)
struct PROTOTYPE3_API FHouseFurnitureRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Furniture")
	EHouseFurnitureType Type = EHouseFurnitureType::Sofa;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Furniture")
	TArray<EHouseRoomType> AllowedRooms;

	/** Empty means any purpose/style. Profile-only rules do not affect old houses. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Furniture")
	TArray<EHouseRoomPurpose> AllowedPurposes;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Furniture")
	TArray<EHouseFurnitureStyle> AllowedStyles;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Furniture")
	bool bHouseholdOnly = false;
	/** Essential pieces survive sparse density; occupancy can still remove them. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Furniture")
	bool bEssential = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Furniture")
	EHouseFurnishingDensity MinimumDensity = EHouseFurnishingDensity::Standard;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Furniture", meta=(Units="cm"))
	FVector Size = FVector(200, 90, 90);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Furniture", meta=(Units="cm", ClampMin="50"))
	float FrontClearance = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Furniture")
	bool bPreferCenter = false;
};

/** Usable internal dimensions, measured between wall faces, in metres / square metres. */
USTRUCT(BlueprintType)
struct PROTOTYPE3_API FHouseRoomSizeRange
{
	GENERATED_BODY()
	FHouseRoomSizeRange() = default;
	FHouseRoomSizeRange(float Min, float Max, float Width, float Aspect = 2.2f) : MinArea(Min), MaxArea(Max), MinWidth(Width), MaxAspectRatio(Aspect) {}
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sizing", meta=(ClampMin="1"))
	float MinArea = 7.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sizing", meta=(ClampMin="1"))
	float MaxArea = 16.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sizing", meta=(ClampMin="1", Units="m"))
	float MinWidth = 2.15f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sizing", meta=(ClampMin="1", ClampMax="4"))
	float MaxAspectRatio = 2.2f;
};

USTRUCT(BlueprintType)
struct PROTOTYPE3_API FHouseSizingSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sizing")
	FHouseRoomSizeRange LivingRoom = FHouseRoomSizeRange(18, 32, 3.2f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sizing")
	FHouseRoomSizeRange Kitchen = FHouseRoomSizeRange(8, 20, 1.8f, 3.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sizing")
	FHouseRoomSizeRange PrimaryBedroom = FHouseRoomSizeRange(11.5f, 20, 2.75f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sizing")
	FHouseRoomSizeRange OtherBedroom = FHouseRoomSizeRange(7.5f, 16, 2.15f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sizing")
	FHouseRoomSizeRange Bathroom = FHouseRoomSizeRange(4, 8.5f, 1.8f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sizing")
	FHouseRoomSizeRange Office = FHouseRoomSizeRange(7.5f, 14, 2.15f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sizing")
	FHouseRoomSizeRange Storage = FHouseRoomSizeRange(3, 12, 1.5f);
	/** Rounded up to whole cells. Default 100 cm cells produce 180 cm clear corridors. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sizing", meta=(ClampMin="1", Units="m"))
	float MinHallwayWidth = 1.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sizing", meta=(ClampMin="1", Units="m"))
	float MaxHallwayWidth = 1.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sizing", meta=(ClampMin="2", Units="m"))
	float MaxHallwayLength = 10.0f;
};

USTRUCT(BlueprintType)
struct PROTOTYPE3_API FHouseGenerationSettings
{
	GENERATED_BODY()
	FHouseGenerationSettings();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Controls|Layout")
	int32 Seed = 12345;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Controls|Layout")
	FHouseNavigationSettings Navigation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Controls|Layout")
	FHouseholdSettings Household;

	/** Planned patterns use these limits and fit a compact house inside Width/Depth Cells. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Controls|Layout")
	FHouseSizingSettings RoomSizing;


	/** Planned houses may grow beyond Width/Depth Cells when necessary, up to a 24 x 24 envelope.
	 * Disable to enforce strict footprint limits. Your input dimensions are never overwritten. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Controls|Layout", meta=(DisplayName="Allow Larger Footprint"))
	bool bAllowLargerFootprint = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Controls|Layout", meta=(ClampMin="3", ClampMax="64"))
	int32 WidthCells = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Controls|Layout", meta=(ClampMin="3", ClampMax="64"))
	int32 DepthCells = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Controls|Layout", meta=(ClampMin="1", ClampMax="32"))
	int32 RoomCount = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Controls|Layout", meta=(ClampMin="3", ClampMax="32"))
	int32 MinRoomCells = 3;

	/** Used only by Original Random Layout; planned patterns preserve their requested door graph. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Controls|Layout", meta=(ClampMin="0", ClampMax="1"))
	float ExtraConnectionChance = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Controls|Dimensions", meta=(Units="cm", ClampMin="100", ClampMax="500"))
	float CellSize = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Controls|Dimensions", meta=(Units="cm"))
	float WallHeight = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Controls|Dimensions", meta=(Units="cm"))
	float WallThickness = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Controls|Dimensions", meta=(Units="cm"))
	float SlabThickness = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Controls|Dimensions", meta=(Units="cm"))
	float DoorWidth = 140.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Controls|Dimensions", meta=(Units="cm"))
	float DoorHeight = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Controls|Dimensions", meta=(Units="cm"))
	float WindowWidth = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Controls|Dimensions", meta=(Units="cm"))
	float WindowHeight = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Controls|Dimensions", meta=(Units="cm"))
	float WindowSillHeight = 100.0f;

	/** Fractional variation sampled once per room around nominal window dimensions, with room-type adjustments. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Controls|Windows", meta=(ClampMin="0", ClampMax="0.75"))
	float WindowSizeVariation = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Controls|Windows", meta=(ClampMin="0", ClampMax="1"))
	float WindowChance = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Controls|Windows", meta=(ClampMin="0", ClampMax="6"))
	int32 MaxWindowsPerWall = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Controls|Windows", meta=(Units="cm", ClampMin="20"))
	float WindowSpacing = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Controls|Furniture")
	bool bGenerateFurniture = true;

	/** Reserved passage width used by the furniture placement solver. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Controls|Furniture", meta=(Units="cm", ClampMin="80"))
	float WalkwayWidth = 90.0f;

	/** Evaluated in order: put large/important pieces first. Items that cannot fit safely are skipped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Controls|Furniture")
	TArray<FHouseFurnitureRule> FurnitureRules;
};

USTRUCT(BlueprintType)
struct PROTOTYPE3_API FHouseRoom
{
	GENERATED_BODY()

	/** Integer grid bounds; Max is exclusive. Array index is the stable room ID. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House")
	FIntPoint Min = FIntPoint::ZeroValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House")
	FIntPoint Max = FIntPoint::ZeroValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House")
	EHouseRoomType RoomType = EHouseRoomType::Bedroom;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House")
	EHouseRoomPurpose Purpose = EHouseRoomPurpose::Default;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House")
	bool bUnfurnished = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House")
	EHouseFurnishingDensity Density = EHouseFurnishingDensity::Standard;
};

UENUM(BlueprintType)
enum class EHouseOpening : uint8
{
	None,
	Door,
	Window
};

USTRUCT(BlueprintType)
struct PROTOTYPE3_API FHouseWindow
{
	GENERATED_BODY()

	/** Centimetres along the wall, relative to Start. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House")
	float Offset = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House")
	float Width = 120;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House")
	float Height = 100;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House")
	float SillHeight = 100;
};

USTRUCT(BlueprintType)
struct PROTOTYPE3_API FHouseWall
{
	GENERATED_BODY()

	/** Canonical direction: increasing X or increasing Y. Each boundary appears once. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House")
	FIntPoint Start = FIntPoint::ZeroValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House")
	FIntPoint End = FIntPoint::ZeroValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House")
	int32 RoomA = INDEX_NONE;

	/** INDEX_NONE means exterior. Doorways are centered on the boundary. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House")
	int32 RoomB = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House")
	EHouseOpening Opening = EHouseOpening::None;

	/** Ordered by Offset. Exterior door walls can also contain windows. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House")
	TArray<FHouseWindow> Windows;
};

USTRUCT(BlueprintType)
struct PROTOTYPE3_API FHouseFurniturePlacement
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House")
	int32 RoomIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House")
	int32 RuleIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House")
	EHouseFurnitureType Type = EHouseFurnitureType::Sofa;

	/** Local house position at the footprint centre, in centimetres. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House")
	FVector2D Center = FVector2D::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House")
	int32 QuarterTurns = 0;
};

USTRUCT(BlueprintType)
struct PROTOTYPE3_API FHouseLayout
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House")
	FHouseNavigationReport Navigation;
	/** Actual generated rectangle; planned houses can be smaller than the requested envelope. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House")
	FIntPoint FootprintCells = FIntPoint::ZeroValue;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House")
	FString SizeSummary;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House")
	FHouseholdSettings Household;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House")
	TArray<FHouseRoom> Rooms;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House")
	TArray<FHouseWall> Walls;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House")
	TArray<FHouseFurniturePlacement> Furniture;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House")
	int32 SkippedFurnitureCount = 0;
};

/** Data-only generation: no world, assets, global randomness or spawned actors. */
class PROTOTYPE3_API FHouseLayoutGenerator
{
public:
	/** Editor-friendly generation: raise undersized room counts, then try up to 16 deterministic seeds.
	 * EffectiveSettings records the successful seed/count; original request is never mutated. */
	static bool GenerateWithRetries(const FHouseGenerationSettings& Requested, FHouseGenerationSettings& EffectiveSettings,
		FHouseLayout& OutLayout, int32& Attempts, FString& OutError);
	static bool ValidateSettings(const FHouseGenerationSettings& Settings, FString& OutError);
	static bool Generate(const FHouseGenerationSettings& Settings, FHouseLayout& OutLayout, FString& OutError);
	static bool Validate(const FHouseGenerationSettings& Settings, const FHouseLayout& Layout, FString& OutError);
};
