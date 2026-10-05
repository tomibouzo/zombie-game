#include "UI/Inventory/InventoryInputSettings.h"

namespace
{
TArray<FKey> DefaultKeys()
{
	return { EKeys::I, EKeys::LeftMouseButton, EKeys::Q, EKeys::E, EKeys::MiddleMouseButton,
		EKeys::Delete, EKeys::B, EKeys::RightMouseButton, EKeys::Tab, EKeys::R, EKeys::U,
		EKeys::H, EKeys::T, EKeys::Y, EKeys::X, EKeys::One, EKeys::Two, EKeys::Three, EKeys::V };
}
}

UInventoryInputSettings::UInventoryInputSettings() { ResetDefaults(); }

void UInventoryInputSettings::ResetDefaults()
{
	Keys = DefaultKeys();
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
			for (FKey Candidate : { EKeys::MiddleMouseButton, EKeys::BackSpace,
				EKeys::F8, EKeys::F9, EKeys::F10, EKeys::F11, EKeys::F12 })
				if (!Keys.Contains(Candidate)) { Cancel = Candidate; break; }
		}
		// Keep custom bindings that already use right mouse; use an available fallback.
		for (FKey Candidate : { EKeys::RightMouseButton, EKeys::MiddleMouseButton, EKeys::R,
			EKeys::F8, EKeys::F9, EKeys::F10, EKeys::F11, EKeys::F12 })
			if (!Keys.Contains(Candidate)) { Keys.Add(Candidate); break; }
	}
	// Scrolling now adjusts rotation speed. Move older scroll turn bindings to free keys.
	if (Keys.Num() >= static_cast<int32>(EInventoryControl::ShowQuick))
		for (EInventoryControl Action : { EInventoryControl::TurnLeft, EInventoryControl::TurnRight })
		{
			FKey& Key = Keys[static_cast<int32>(Action)];
			if (Key != EKeys::MouseScrollUp && Key != EKeys::MouseScrollDown) continue;
			for (FKey Candidate : { EKeys::Q, EKeys::E, EKeys::Left, EKeys::Right,
				EKeys::R, EKeys::F8, EKeys::F9, EKeys::F10, EKeys::F11, EKeys::F12 })
				if (!Keys.Contains(Candidate)) { Key = Candidate; break; }
		}
	// Escape belongs to the interface stack. Migrate only the affected binding.
	const TArray<FKey> Defaults = DefaultKeys();
	for (int32 I = 0; I < Keys.Num() && I < Defaults.Num(); ++I)
		if (Keys[I] == EKeys::Escape)
		{
			TArray<FKey> Candidates { Defaults[I], EKeys::BackSpace };
			Candidates.Append(Defaults);
			for (FKey Candidate : Candidates)
				if (!Keys.Contains(Candidate)) { Keys[I] = Candidate; break; }
		}
	// New panel actions use free keys, preserving existing custom assignments.
	if (Keys.Num() >= static_cast<int32>(EInventoryControl::ShowQuick) && Keys.Num() < Defaults.Num())
		while (Keys.Num() < Defaults.Num())
		{
			TArray<FKey> Candidates { Defaults[Keys.Num()] };
			Candidates.Append({ EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight,
				EKeys::Nine, EKeys::Zero, EKeys::J, EKeys::K, EKeys::L, EKeys::O, EKeys::P,
				EKeys::F5, EKeys::F6, EKeys::F7, EKeys::F8, EKeys::F9, EKeys::F10, EKeys::F11, EKeys::F12 });
			for (FKey Candidate : Candidates)
				if (!Keys.Contains(Candidate)) { Keys.Add(Candidate); break; }
		}
	bool bValid = Keys.Num() == static_cast<int32>(EInventoryControl::Count);
	TSet<FKey> Unique;
	for (int32 I = 0; bValid && I < Keys.Num(); ++I)
	{
		if (I == static_cast<int32>(EInventoryControl::Add)) continue;
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
	if (Key == EKeys::Escape || Key == EKeys::MouseScrollUp || Key == EKeys::MouseScrollDown) return false;
	return true;
}

bool UInventoryInputSettings::TrySetKey(EInventoryControl Action, FKey Key, FString& Error)
{
	if (!Supports(Action, Key))
	{
		Error = Key == EKeys::Escape ? TEXT("Escape is reserved for closing interfaces and pausing.")
			: TEXT("Choose a key or mouse button. Wheel scrolling adjusts rotation speed.");
		return false;
	}
	for (int32 I = 0; I < Keys.Num(); ++I)
		// The hidden laboratory action must not reserve a key in the player's Options.
		if (I != static_cast<int32>(Action) && I != static_cast<int32>(EInventoryControl::Add) && Keys[I] == Key)
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
	case EInventoryControl::Toggle: return TEXT("Open / close inventory");
	case EInventoryControl::Grab: return TEXT("Grab / place");
	case EInventoryControl::TurnLeft: return TEXT("Turn left");
	case EInventoryControl::TurnRight: return TEXT("Turn right");
	case EInventoryControl::Cancel: return TEXT("Cancel placement");
	case EInventoryControl::Drop: return TEXT("Drop selected item");
	case EInventoryControl::Add: return TEXT("Add bandage");
	case EInventoryControl::RotateWithMouse: return TEXT("Hold to rotate");
	case EInventoryControl::ShowQuick: return TEXT("Show quick storage");
	case EInventoryControl::OpenBackpack: return TEXT("Open backpack");
	case EInventoryControl::ToggleBackpack: return TEXT("Equip / unequip backpack");
	case EInventoryControl::ToHands: return TEXT("Take selected item in hands");
	case EInventoryControl::ToQuick: return TEXT("Transfer to quick storage");
	case EInventoryControl::ToBackpack: return TEXT("Transfer to backpack");
	case EInventoryControl::Stow: return TEXT("Stow held item");
	case EInventoryControl::AssignShortcut1: return TEXT("Assign selected to shortcut 1");
	case EInventoryControl::AssignShortcut2: return TEXT("Assign selected to shortcut 2");
	case EInventoryControl::AssignShortcut3: return TEXT("Assign selected to shortcut 3");
	case EInventoryControl::ToggleGrabMode: return TEXT("Switch hold / click grab");
	default: return FString();
	}
}
