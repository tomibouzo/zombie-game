// Copyright Epic Games, Inc. All Rights Reserved.

#include "prototype3.h"
#include "Modules/ModuleManager.h"
#include "Gameplay/Procedural/Editor/HouseDetails.h"
#include "Gameplay/Procedural/Editor/HousePresetDetails.h"

class FPrototype3Module : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();
#if WITH_EDITOR
		RegisterHousePresetDetails();
#endif
#if WITH_EDITOR && PLATFORM_LINUX
		RegisterHouseDetails();
#endif
	}
	virtual void ShutdownModule() override
	{
#if WITH_EDITOR && PLATFORM_LINUX
		UnregisterHouseDetails();
#endif
#if WITH_EDITOR
		UnregisterHousePresetDetails();
#endif
		FDefaultGameModuleImpl::ShutdownModule();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FPrototype3Module, prototype3, "prototype3");

DEFINE_LOG_CATEGORY(Logprototype3)