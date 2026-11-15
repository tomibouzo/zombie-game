#pragma once

#include "Gameplay/Player/Interaction/Interactable.h"
#include "Gameplay/Shelter/Build/ShelterPiece.h"
#include "ShelterInteractivePieces.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

UCLASS()
class PROTOTYPE3_API AShelterDoorPiece : public AShelterPiece, public IInteractable
{
	GENERATED_BODY()
public:
	AShelterDoorPiece();
	virtual void InitializePiece(const UShelterPieceDefinition* InDefinition, const FShelterModule& InModule) override;
	virtual void SetPreview(bool bValid) override;
	virtual bool CanInteract(const AActor* Instigator) const override;
	virtual FText GetInteractionPrompt(const AActor* Instigator) const override;
	virtual void Interact(AActor* Instigator) override;
	bool IsOpen() const { return bOpen; }

private:
	bool IsDoorwayOccupied() const;

	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> LeftFrame;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> RightFrame;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Header;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Hinge;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Leaf;
	bool bOpen = false;
};

UCLASS()
class PROTOTYPE3_API AShelterLampPiece : public AShelterPiece, public IInteractable
{
	GENERATED_BODY()
public:
	AShelterLampPiece();
	virtual void SetPreview(bool bValid) override;
	virtual bool CanInteract(const AActor* Instigator) const override;
	virtual FText GetInteractionPrompt(const AActor* Instigator) const override;
	virtual void Interact(AActor* Instigator) override;
	bool IsLit() const;

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Stand;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Shade;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Light;
};
