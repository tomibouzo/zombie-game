#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PrototypeDebugSettings.generated.h"

/** Local debug preferences, saved independently of control bindings and maps. */
UCLASS(Config=GameUserSettings)
class PROTOTYPE3_API UPrototypeDebugSettings : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(Config)
	bool bInvisibleToZombies = false;
};
