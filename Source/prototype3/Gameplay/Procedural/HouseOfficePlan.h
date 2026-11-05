#pragma once
#include "Gameplay/Procedural/HouseLayout.h"

/** Size-constrained guillotine packing and a connected workroom graph with private service leaves. */
class PROTOTYPE3_API FHouseOfficePlan
{
public:
	static bool Build(const FHouseGenerationSettings& S, FHouseLayout& L, TArray<FIntPoint>& Connections, FString& Error);
	static bool Validate(const FHouseLayout& L, FString& Error);
	/** Explicit authoring action: replaces generation controls, retaining the requested seed. */
	static FHouseGenerationSettings Preset(int32 Seed);
};
