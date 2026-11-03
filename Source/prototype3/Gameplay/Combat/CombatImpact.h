#pragma once
#include "Engine/DamageEvents.h"

/** Damage and interruption strength are independent, usable by any combat actor. */
struct FCombatImpactDamageEvent : public FPointDamageEvent
{
	static const int32 ClassID = 10701;
	float ImpactStrength = 0;
	virtual int32 GetTypeID() const override { return ClassID; }
	virtual bool IsOfType(int32 InID) const override { return InID == ClassID || FPointDamageEvent::IsOfType(InID); }
};
