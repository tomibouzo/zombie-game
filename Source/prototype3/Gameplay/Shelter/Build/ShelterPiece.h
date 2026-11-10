#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Shelter/Build/ShelterBuildModel.h"
#include "ShelterPiece.generated.h"

class AShelterPiece;
class UMaterialInstanceDynamic;
class UStaticMesh;
class UMaterialInterface;

/**
 * One buildable piece. Adding a piece means adding an asset of this type under
 * /Game/Shelter/Pieces; build code never names individual pieces. Behaviour lives in ActorClass.
 */
UCLASS(BlueprintType)
class PROTOTYPE3_API UShelterPieceDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	static const FPrimaryAssetType PieceAssetType;

	/** Stable identity stored in the build grid. Do not change it when renaming the asset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Piece|Identity") FName PieceId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Piece|Identity") FText DisplayName;
	/** Build menu group. Pieces sharing a category are cycled together. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Piece|Menu") FText Category;
	/** Lower values come first, both for categories (their lowest piece) and within a category. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Piece|Menu") int32 SortOrder = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Piece|Placement") EShelterSlotKind SlotKind = EShelterSlotKind::Edge;
	/** Actor spawned in the slot. Empty uses the placeholder actor configured below. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Piece|Placement") TSubclassOf<AShelterPiece> ActorClass;

	/** Placeholder only. Edge pieces: centred opening, zero for a solid panel. Units are centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Piece|Placeholder", meta=(ClampMin="0", Units="cm")) float OpeningWidth = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Piece|Placeholder", meta=(ClampMin="0", Units="cm")) float OpeningHeight = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Piece|Placeholder", meta=(ClampMin="0", Units="cm")) float OpeningSillHeight = 0;
	/** Placeholder only. Cell pieces: box size (X width, Y depth, Z height). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Piece|Placeholder", meta=(Units="cm")) FVector CellObjectSize = FVector(100, 60, 90);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Piece|Placeholder") FLinearColor PlaceholderColor = FLinearColor(0.45f, 0.42f, 0.38f);

	UFUNCTION(BlueprintPure, Category="Piece") bool IsValidDefinition() const;
	TSubclassOf<AShelterPiece> ResolveActorClass() const;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};

/** Module dimensions handed to a piece when it is created. */
USTRUCT(BlueprintType)
struct PROTOTYPE3_API FShelterModule
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shelter", meta=(ClampMin="100", Units="cm")) float Width = 200;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shelter", meta=(ClampMin="100", Units="cm")) float Height = 300;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shelter", meta=(ClampMin="5", Units="cm")) float WallThickness = 20;
};

/**
 * Base for every placed piece. Local frame: origin at the slot centre on the floor,
 * X along the edge, Z up. Interactive pieces derive from this and add their own behaviour.
 */
UCLASS(Abstract)
class PROTOTYPE3_API AShelterPiece : public AActor
{
	GENERATED_BODY()
public:
	AShelterPiece();
	/** Called once, before the piece finishes spawning. */
	virtual void InitializePiece(const UShelterPieceDefinition* InDefinition, const FShelterModule& InModule);
	/** Preview copies have no collision; bValid selects the can/cannot-build tint. */
	virtual void SetPreview(bool bValid);
	const UShelterPieceDefinition* GetDefinition() const { return Definition; }
	bool IsPreview() const { return bPreview; }
protected:
	UPROPERTY(VisibleAnywhere, Category="Shelter") TObjectPtr<UShelterPieceDefinition> Definition;
	UPROPERTY(VisibleAnywhere, Category="Shelter") FShelterModule Module;
	bool bPreview = false;
};

/** Untextured stand-in built from the definition's placeholder values. */
UCLASS()
class PROTOTYPE3_API AShelterPlaceholderPiece : public AShelterPiece
{
	GENERATED_BODY()
public:
	AShelterPlaceholderPiece();
	virtual void InitializePiece(const UShelterPieceDefinition* InDefinition, const FShelterModule& InModule) override;
	virtual void SetPreview(bool bValid) override;
private:
	void AddBox(const FVector& Center, const FVector& Size);
	UPROPERTY() TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY() TObjectPtr<UMaterialInterface> BaseMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Material;
};
