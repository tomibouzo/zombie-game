#include "UI/Inventory/InventoryInputSettings.h"

UInventoryInputSettings::UInventoryInputSettings() { ResetDefaults(); }

void UInventoryInputSettings::ResetDefaults()
{
	Keys = { EKeys::I, EKeys::LeftMouseButton, EKeys::Q, EKeys::E, EKeys::MiddleMouseButton, EKeys::Delete, EKeys::B, EKeys::RightMouseButton };
	bToggleGrab = false;
	TurnSpeed = 120;
}

void UInventoryInputSettings::PostInitProperties()
{
	Super::PostInitProperties();
	ValidateSettings();
}

void UInventoryInputSettings::PostReloadConfig(FProperty* PropertyThatWasLoaded)
{
	Super::PostReloadConfig(PropertyThatWasLoaded);
	ValidateSettings();
}

void UInventoryInputSettings::ValidateSettings()
{
	// Append the new action to legacy arrays, preserving unrelated preferences.
	if (Keys.Num() == static_cast<int32>(EInventoryControl::RotateWithMouse))
	{
		FKey& Cancel = Keys[static_cast<int32>(EInventoryControl::Cancel)];
		if (Cancel == EKeys::RightMouseButton)
		{
			for (FKey Candidate : { EKeys::MiddleMouseButton, EKeys::Escape, EKeys::BackSpace,
				EKeys::F8, EKeys::F9, EKeys::F10, EKeys::F11, EKeys::F12 })
				if (!Keys.Contains(Candidate)) { Cancel = Candidate; break; }
		}
		// Keep custom bindings that already use right mouse; use an available fallback.
		for (FKey Candidate : { EKeys::RightMouseButton, EKeys::MiddleMouseButton, EKeys::R,
			EKeys::F8, EKeys::F9, EKeys::F10, EKeys::F11, EKeys::F12 })
			if (!Keys.Contains(Candidate)) { Keys.Add(Candidate); break; }
	}
	// Scrolling now adjusts rotation speed. Move older scroll turn bindings to free keys.
	if (Keys.Num() == static_cast<int32>(EInventoryControl::Count))
		for (EInventoryControl Action : { EInventoryControl::TurnLeft, EInventoryControl::TurnRight })
		{
			FKey& Key = Keys[static_cast<int32>(Action)];
			if (Key != EKeys::MouseScrollUp && Key != EKeys::MouseScrollDown) continue;
			for (FKey Candidate : { EKeys::Q, EKeys::E, EKeys::Left, EKeys::Right,
				EKeys::R, EKeys::F8, EKeys::F9, EKeys::F10, EKeys::F11, EKeys::F12 })
				if (!Keys.Contains(Candidate)) { Key = Candidate; break; }
		}
	bool bValid = Keys.Num() == static_cast<int32>(EInventoryControl::Count);
	TSet<FKey> Unique;
	for (int32 I = 0; bValid && I < Keys.Num(); ++I)
	{
		bValid = Supports(static_cast<EInventoryControl>(I), Keys[I]) && !Unique.Contains(Keys[I]);
		Unique.Add(Keys[I]);
	}
	if (!bValid) ResetDefaults();
	if (!FMath::IsFinite(TurnSpeed)) TurnSpeed = 120;
	TurnSpeed = FMath::Clamp(TurnSpeed, 15.f, 360.f);
}

FKey UInventoryInputSettings::GetKey(EInventoryControl Action) const
{
	return Keys.IsValidIndex(static_cast<int32>(Action)) ? Keys[static_cast<int32>(Action)] : EKeys::Invalid;
}

bool UInventoryInputSettings::Supports(EInventoryControl Action, FKey Key)
{
	if (Action >= EInventoryControl::Count || !Key.IsValid() || Key == EKeys::AnyKey || !Key.IsDigital() || Key.IsGamepadKey() || Key.IsTouch()) return false;
	if (Key == EKeys::MouseScrollUp || Key == EKeys::MouseScrollDown) return false;
	return true;
}

bool UInventoryInputSettings::TrySetKey(EInventoryControl Action, FKey Key, FString& Error)
{
	if (!Supports(Action, Key))
	{
		Error = TEXT("Use a key or mouse button. Wheel scrolling adjusts rotation speed.");
		return false;
	}
	for (int32 I = 0; I < Keys.Num(); ++I)
		if (I != static_cast<int32>(Action) && Keys[I] == Key)
		{
			Error = TEXT("Already assigned to: ") + Label(static_cast<EInventoryControl>(I));
			return false;
		}
	Keys[static_cast<int32>(Action)] = Key;
	Error.Empty();
	return true;
}

FString UInventoryInputSettings::Label(EInventoryControl Action)
{
	switch (Action)
	{
	case EInventoryControl::Toggle: return TEXT("Open / close");
	case EInventoryControl::Grab: return TEXT("Grab / place");
	case EInventoryControl::TurnLeft: return TEXT("Turn left");
	case EInventoryControl::TurnRight: return TEXT("Turn right");
	case EInventoryControl::Cancel: return TEXT("Cancel placement");
	case EInventoryControl::Remove: return TEXT("Remove item");
	case EInventoryControl::Add: return TEXT("Add bandage");
	case EInventoryControl::RotateWithMouse: return TEXT("Hold to rotate");
	default: return FString();
	}
}
