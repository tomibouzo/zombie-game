#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Procedural/HouseLayout.h"
#include "HouseGenerator.generated.h"

class UHouseStyle;
class UInstancedStaticMeshComponent;
class UStaticMesh;
class UTextRenderComponent;

/** Single-storey blockout generator. Local origin is the southwest corner at finished floor height.
 * Keep actor scale at one for the documented centimeter clearances. */
UCLASS(Blueprintable)
class PROTOTYPE3_API AHouseGenerator : public AActor
{
	GENERATED_BODY()

public:
	AHouseGenerator();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House Controls", meta=(ShowOnlyInnerProperties))
	FHouseGenerationSettings Settings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House")
	TObjectPtr<UHouseStyle> Style;

	/** Defaults off: generated editor geometry is serialized with the map. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House")
	bool bGenerateOnBeginPlay = false;

	/** Visualization only; the roof still blocks collision. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House")
	bool bHideRoof = false;

	/** Top-facing labels for room types and furniture blockouts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="House")
	bool bShowLabels = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House|Result")
	FHouseLayout Layout;

	/** Serialized snapshot for geometry and Blueprint queries, hidden from Details to avoid duplicate read-only controls. */
	UPROPERTY(BlueprintReadOnly, Category="House|Result")
	FHouseGenerationSettings GeneratedSettings;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House|Result")
	int32 GeneratedSeed = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House|Result", meta=(MultiLine="true"))
	FString GenerationSummary;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House|Result")
	FString LastError;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House|Result", meta=(MultiLine="true"))
	FString NavigationSummary;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House|Result")
	FString SizeSummary;

	/** Failed requests preserve the last valid house. Success replaces only our generated components. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category="House", meta=(DisplayName="Generate House"))
	void GenerateHouse();

	/** Pick a different input seed and generate with the current controls. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category="House", meta=(DisplayName="Generate New House"))
	void GenerateNewHouse();

	/** Replaces the editable furniture rules with the current blockout catalogue. Does not regenerate. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category="House", meta=(DisplayName="Reset Furniture Rules To Defaults"))
	void ResetFurnitureRulesToDefaults();

	UFUNCTION(CallInEditor, BlueprintCallable, Category="House", meta=(DisplayName="Clear House"))
	void ClearHouse();

	/** Colored room outlines and room IDs, visible for 30 seconds. Uses the last generated settings. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category="House", meta=(DisplayName="Draw Layout Debug"))
	void DrawLayoutDebug();

	UFUNCTION(BlueprintCallable, Category="House")
	void SetRoofHidden(bool bNewHidden);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House|Components")
	TObjectPtr<UInstancedStaticMeshComponent> Floors;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House|Components")
	TObjectPtr<UInstancedStaticMeshComponent> Walls;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House|Components")
	TObjectPtr<UInstancedStaticMeshComponent> Roof;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="House|Components")
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> FurnitureMeshes;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	UPROPERTY()
	TObjectPtr<UStaticMesh> DefaultBlockMesh;

	UPROPERTY()
	TArray<TObjectPtr<UTextRenderComponent>> GeneratedLabels;

	void ClearLabels();
	void AddLabel(const FString& Text, const FVector& Position, const FColor& Color, float TextSize = 25.0f);
	void BuildGeometry();
	void AddBox(UInstancedStaticMeshComponent* Component, const FVector& Center, const FVector& Size);
};
