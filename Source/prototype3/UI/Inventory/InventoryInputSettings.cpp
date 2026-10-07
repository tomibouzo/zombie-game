#include "UI/Inventory/InventoryInputSettings.h"
#include "Gameplay/Player/Inventory/ItemUseSettings.h"

namespace
{
using A = EInventoryControl;
namespace InputContexts
{
constexpr uint32 Game = 1, Pockets = 2, Backpack = 4, PocketDrag = 8, BackpackDrag = 16, MenuContext = 32;
constexpr uint32 Inventory = Pockets | Backpack | PocketDrag | BackpackDrag;
}
TArray<FKey> DefaultKeys()
{
	TArray<FKey> Result;
	Result.Init(EKeys::Invalid, static_cast<int32>(A::Count));
	auto Set = [&](A Action, FKey Key) { Result[static_cast<int32>(Action)] = Key; };
	Set(A::Toggle, EKeys::I); Set(A::Grab, EKeys::LeftMouseButton);
	Set(A::TurnLeft, EKeys::Q); Set(A::TurnRight, EKeys::E);
	Set(A::Cancel, EKeys::C); Set(A::Drop, EKeys::F); Set(A::Add, EKeys::B);
	Set(A::RotateWithMouse, EKeys::RightMouseButton); Set(A::ShowQuick, EKeys::Tab);
	Set(A::Stow, EKeys::X);
	Set(A::MoveForward, EKeys::W); Set(A::MoveBackward, EKeys::S);
	Set(A::MoveLeft, EKeys::A); Set(A::MoveRight, EKeys::D);
	Set(A::Run, EKeys::LeftShift); Set(A::Sprint, EKeys::LeftAlt); Set(A::Crouch, EKeys::LeftControl);
	Set(A::Primary, EKeys::LeftMouseButton); Set(A::Secondary, EKeys::RightMouseButton);
	Set(A::Bandage, EKeys::G);
	Set(A::CyclePocket, EKeys::Tab);
	Set(A::FasterRotation, EKeys::MouseScrollUp); Set(A::SlowerRotation, EKeys::MouseScrollDown);
	Set(A::ScrollFloorUp, EKeys::MouseScrollUp); Set(A::ScrollFloorDown, EKeys::MouseScrollDown);
	Set(A::Back, EKeys::Escape);
	return Result;
}
}

UInventoryInputSettings::UInventoryInputSettings()
{
	Keys = DefaultKeys(); AlternateKeys.Init(EKeys::Invalid, Keys.Num());
}
void UInventoryInputSettings::PostInitProperties() { Super::PostInitProperties(); ValidateSettings(); }
void UInventoryInputSettings::PostReloadConfig(FProperty* Property) { Super::PostReloadConfig(Property); ValidateSettings(); }
void UInventoryInputSettings::ValidateSettings()
{
	const auto Defaults = DefaultKeys();
	const int32 OldNum = Keys.Num();
	Keys.SetNum(Defaults.Num()); AlternateKeys.SetNum(Defaults.Num());
	for (int32 I = OldNum; I < Keys.Num(); ++I) Keys[I] = Defaults[I];
	if (ControlsVersion < 2)
	{
		const auto* Legacy = GetDefault<UItemUseSettings>();
		for (int32 I = 0; I < 3; ++I) Keys[static_cast<int32>(A::Bandage) + I] = Legacy->MigrationKey(I);
		Keys[static_cast<int32>(A::CyclePocket)] = Keys[static_cast<int32>(A::ShowQuick)];
		FKey& Cancel = Keys[static_cast<int32>(A::Cancel)];
		if (Cancel == EKeys::MiddleMouseButton || Cancel == EKeys::RightMouseButton || Cancel == EKeys::Escape) Cancel = EKeys::C;
		if (Keys[static_cast<int32>(A::Drop)] == EKeys::Delete) Keys[static_cast<int32>(A::Drop)] = EKeys::F;
		for (auto Action : { A::TurnLeft, A::TurnRight })
		{
			FKey& Key = Keys[static_cast<int32>(Action)];
			if (Key == EKeys::MouseScrollUp || Key == EKeys::MouseScrollDown) Key = Defaults[static_cast<int32>(Action)];
		}
		ControlsVersion = 2;
	}
	for (int32 I = 0; I < Keys.Num(); ++I)
	{
		const auto Action = static_cast<A>(I);
		if (!IsBindable(Action))
		{
			if (Action != A::Add) Keys[I] = Action == A::Back ? EKeys::Escape : EKeys::Invalid;
			AlternateKeys[I] = EKeys::Invalid; continue;
		}
		// Explicitly empty slots survive loading; they are not missing defaults.
		for (FKey* Key : { &Keys[I], &AlternateKeys[I] })
			if (Key->IsValid() && !Supports(Action, *Key)) *Key = EKeys::Invalid;
	}
	if (ControlsVersion < 3)
	{
		if (Keys[static_cast<int32>(A::Bandage)] == EKeys::One)
		{
			Keys[static_cast<int32>(A::Bandage)] = EKeys::Invalid;
			for (FKey Candidate : { EKeys::G, EKeys::H, EKeys::J, EKeys::K, EKeys::B, EKeys::L, EKeys::O, EKeys::P })
				if (Conflicts(A::Bandage, Candidate).IsEmpty()) { Keys[static_cast<int32>(A::Bandage)] = Candidate; break; }
		}
		ControlsVersion = 3;
	}
	// New defaults must yield to saved custom keys in overlapping contexts.
	// Malformed files are also normalized once, without reviving empty slots.
	TArray<int32> Order;
	for (int32 I = 0; I < Keys.Num() * 2; ++I) Order.Add(I);
	Order.StableSort([&](int32 L, int32 R)
	{
		auto Custom = [&](int32 Index) { return Index >= Keys.Num() || Keys[Index] != Defaults[Index]; };
		return Custom(L) && !Custom(R);
	});
	TMap<FKey, uint32> Occupied;
	for (int32 Index : Order)
	{
		const int32 I = Index % Keys.Num();
		FKey& Key = Index < Keys.Num() ? Keys[I] : AlternateKeys[I];
		const uint32 Context = Contexts(static_cast<A>(I));
		if (!Key.IsValid() || !Context) continue;
		if (Occupied.FindOrAdd(Key) & Context) Key = EKeys::Invalid;
		else Occupied[Key] |= Context;
	}
	SetTurnSpeed(TurnSpeed);
	LookSensitivity = FMath::IsFinite(LookSensitivity) ? FMath::Clamp(LookSensitivity, .1f, 5.f) : 1.f;
}
void UInventoryInputSettings::ResetDefaults()
{
	Keys = DefaultKeys(); AlternateKeys.Init(EKeys::Invalid, Keys.Num()); ControlsVersion = 3;
	bToggleGrab = false; TurnSpeed = 120; LookSensitivity = 1; bInvertLookY = false;
}
bool UInventoryInputSettings::ResetSection(EControlSection Group)
{
	const auto Defaults = DefaultKeys();
	for (int32 I = 0; I < Defaults.Num(); ++I)
		if (IsBindable(static_cast<A>(I)) && Section(static_cast<A>(I)) == Group)
			Keys[I] = AlternateKeys[I] = EKeys::Invalid;
	bool bComplete = true;
	for (int32 I = 0; I < Defaults.Num(); ++I)
		if (IsBindable(static_cast<A>(I)) && Section(static_cast<A>(I)) == Group)
		{
			FString Error;
			bComplete &= AssignKey(static_cast<A>(I), 0, Defaults[I], false, Error);
		}
	if (Group == EControlSection::Inventory) { bToggleGrab = false; TurnSpeed = 120; }
	if (Group == EControlSection::Movement) { LookSensitivity = 1; bInvertLookY = false; }
	return bComplete;
}
void UInventoryInputSettings::SetTurnSpeed(float Value) { TurnSpeed = FMath::IsFinite(Value) ? FMath::Clamp(Value, 15.f, 360.f) : 120.f; }
FKey UInventoryInputSettings::GetKey(A Action, int32 Slot) const
{
	if (Action == A::Back) return Slot == 0 ? EKeys::Escape : EKeys::Invalid;
	const auto& List = Slot == 0 ? Keys : AlternateKeys;
	return Slot >= 0 && Slot < 2 && List.IsValidIndex(static_cast<int32>(Action)) ? List[static_cast<int32>(Action)] : EKeys::Invalid;
}
bool UInventoryInputSettings::Matches(A Action, FKey Key) const
{
	if (Action == A::Back) return Key == EKeys::Escape;
	return Key.IsValid() && IsBindable(Action) && (GetKey(Action) == Key || GetKey(Action, 1) == Key);
}
FString UInventoryInputSettings::KeyLabel(A Action) const
{
	FString Result;
	for (int32 Slot = 0; Slot < 2; ++Slot)
	{
		const FKey Key = GetKey(Action, Slot);
		if (Key.IsValid()) { if (!Result.IsEmpty()) Result += TEXT(" / "); Result += ShortKeyLabel(Key); }
	}
	return Result.IsEmpty() ? TEXT("Unbound") : Result;
}
FString UInventoryInputSettings::ShortKeyLabel(FKey Key)
{
	if (!Key.IsValid()) return TEXT("Unbound");
	static const TMap<FKey, FString> Names = {
		{ EKeys::LeftMouseButton, TEXT("LMC") }, { EKeys::RightMouseButton, TEXT("RMC") },
		{ EKeys::MiddleMouseButton, TEXT("MMC") }, { EKeys::ThumbMouseButton, TEXT("Mouse 4") },
		{ EKeys::ThumbMouseButton2, TEXT("Mouse 5") },
		{ EKeys::MouseScrollUp, TEXT("Wheel Up") }, { EKeys::MouseScrollDown, TEXT("Wheel Down") },
		{ EKeys::Escape, TEXT("Esc") }, { EKeys::Delete, TEXT("Del") },
		{ EKeys::BackSpace, TEXT("Bksp") }, { EKeys::Insert, TEXT("Ins") },
		{ EKeys::PageUp, TEXT("PgUp") }, { EKeys::PageDown, TEXT("PgDn") },
		{ EKeys::CapsLock, TEXT("Caps") }, { EKeys::SpaceBar, TEXT("Space") },
		{ EKeys::LeftShift, TEXT("L Shift") }, { EKeys::RightShift, TEXT("R Shift") },
		{ EKeys::LeftControl, TEXT("L Ctrl") }, { EKeys::RightControl, TEXT("R Ctrl") },
		{ EKeys::LeftAlt, TEXT("L Alt") }, { EKeys::RightAlt, TEXT("R Alt") },
		{ EKeys::LeftCommand, TEXT("L Cmd") }, { EKeys::RightCommand, TEXT("R Cmd") }
	};
	if (const FString* Name = Names.Find(Key)) return *Name;
	return Key.GetDisplayName().ToString();
}
bool UInventoryInputSettings::IsBindable(A Action)
{
	return Action < A::Count && Action != A::Back && Action != A::Food && Action != A::Water
		&& Action != A::ToggleBackpack && Action != A::Add && Action != A::OpenBackpack
		&& Action != A::ToHands && Action != A::ToQuick && Action != A::ToBackpack
		&& Action != A::ToggleGrabMode && Action != A::AssignShortcut1
		&& Action != A::AssignShortcut2 && Action != A::AssignShortcut3;
}
uint32 UInventoryInputSettings::Contexts(A Action)
{
	using namespace InputContexts;
	if (Action == A::Back) return Game | Inventory | MenuContext;
	if (!IsBindable(Action)) return 0;
	if (Action == A::Toggle || Action == A::Stow) return Game | Inventory;
	if (Action == A::ShowQuick) return Game | Pockets | PocketDrag;
	if (Action == A::CyclePocket) return Backpack | BackpackDrag;
	if (Action == A::FasterRotation || Action == A::SlowerRotation) return PocketDrag | BackpackDrag;
	if (Action == A::ScrollFloorUp || Action == A::ScrollFloorDown) return Pockets | Backpack;
	if (Action >= A::MoveForward && Action <= A::Water) return Game;
	return Inventory;
}
bool UInventoryInputSettings::IsGameplayControl(A Action) { return (Contexts(Action) & InputContexts::Game) != 0; }
bool UInventoryInputSettings::IsGameplayReserved(FKey Key) { return Key == EKeys::SpaceBar || Key == EKeys::Delete || Key == EKeys::Escape; }
bool UInventoryInputSettings::Supports(A Action, FKey Key)
{
	if (!IsBindable(Action)) return false;
	if (!Key.IsValid()) return true;
	if (Key == EKeys::AnyKey || IsGameplayReserved(Key) || Key.IsGamepadKey() || Key.IsTouch()) return false;
	const bool Wheel = Key == EKeys::MouseScrollUp || Key == EKeys::MouseScrollDown;
	if (Wheel) return Action == A::FasterRotation || Action == A::SlowerRotation || Action == A::ScrollFloorUp || Action == A::ScrollFloorDown;
	return Key.IsDigital();
}
TArray<A> UInventoryInputSettings::Conflicts(A Action, FKey Key) const
{
	TArray<A> Result;
	if (!Key.IsValid()) return Result;
	for (int32 I = 0; I < static_cast<int32>(A::Count); ++I)
	{
		const auto Other = static_cast<A>(I);
		if (Other != Action && (Contexts(Other) & Contexts(Action)) && Matches(Other, Key)) Result.Add(Other);
	}
	return Result;
}
bool UInventoryInputSettings::AssignKey(A Action, int32 Slot, FKey Key, bool bReplace, FString& Error)
{
	if (Slot < 0 || Slot > 1 || !Supports(Action, Key)) { Error = TEXT("Choose a supported key or mouse button. Esc is fixed; Space is reserved; Delete clears this key."); return false; }
	const auto Conflicting = Conflicts(Action, Key);
	if (!bReplace && !Conflicting.IsEmpty())
	{
		Error = TEXT("Already assigned to: ");
		for (auto Other : Conflicting) { if (Other != Conflicting[0]) Error += TEXT(", "); Error += Label(Other); }
		return false;
	}
	for (auto Other : Conflicting)
		for (int32 S = 0; S < 2; ++S)
			if (GetKey(Other, S) == Key) (S == 0 ? Keys : AlternateKeys)[static_cast<int32>(Other)] = EKeys::Invalid;
	if (Key.IsValid() && GetKey(Action, 1 - Slot) == Key) (Slot == 0 ? AlternateKeys : Keys)[static_cast<int32>(Action)] = EKeys::Invalid;
	(Slot == 0 ? Keys : AlternateKeys)[static_cast<int32>(Action)] = Key;
	Error.Empty(); return true;
}
bool UInventoryInputSettings::TrySetKey(A Action, FKey Key, FString& Error) { return AssignKey(Action, 0, Key, false, Error); }
EControlSection UInventoryInputSettings::Section(A Action)
{
	if (Action >= A::MoveForward && Action <= A::Crouch) return EControlSection::Movement;
	if ((Action >= A::Primary && Action <= A::Water) || Action == A::Stow) return EControlSection::ItemActions;
	if (Action == A::Toggle || Action == A::ShowQuick) return EControlSection::Gameplay;
	return EControlSection::Inventory;
}
void UInventoryInputSettings::CopySettingsFrom(const UInventoryInputSettings& Other)
{
	Keys = Other.Keys; AlternateKeys = Other.AlternateKeys; ControlsVersion = Other.ControlsVersion;
	bToggleGrab = Other.bToggleGrab; TurnSpeed = Other.TurnSpeed;
	LookSensitivity = Other.LookSensitivity; bInvertLookY = Other.bInvertLookY;
}
bool UInventoryInputSettings::HasSameSettings(const UInventoryInputSettings& Other) const
{
	return Keys == Other.Keys && AlternateKeys == Other.AlternateKeys
		&& bToggleGrab == Other.bToggleGrab && TurnSpeed == Other.TurnSpeed
		&& LookSensitivity == Other.LookSensitivity && bInvertLookY == Other.bInvertLookY;
}
FString UInventoryInputSettings::Label(A Action)
{
	switch (Action)
	{
	case A::Toggle: return TEXT("Open / close backpack");
	case A::Grab: return TEXT("Select / drag / place item");
	case A::TurnLeft: return TEXT("Rotate item left");
	case A::TurnRight: return TEXT("Rotate item right");
	case A::Cancel: return TEXT("Cancel item placement");
	case A::Drop: return TEXT("Drop selected item");
	case A::Add: return TEXT("Add bandage");
	case A::RotateWithMouse: return TEXT("Rotate item with mouse");
	case A::ShowQuick: return TEXT("Open / close pockets");
	case A::CyclePocket: return TEXT("Next pocket in backpack view");
	case A::ToggleBackpack: return TEXT("Equip / unequip backpack");
	case A::Stow: return TEXT("Stow held item");
	case A::MoveForward: return TEXT("Move forward");
	case A::MoveBackward: return TEXT("Move backward");
	case A::MoveLeft: return TEXT("Move left");
	case A::MoveRight: return TEXT("Move right");
	case A::Run: return TEXT("Run");
	case A::Sprint: return TEXT("Sprint");
	case A::Crouch: return TEXT("Crouch");
	case A::Primary: return TEXT("Primary item action");
	case A::Secondary: return TEXT("Secondary item action");
	case A::Bandage: return TEXT("Take / use bandage");
	case A::Food: return TEXT("Take / use canned food");
	case A::Water: return TEXT("Take / use water");
	case A::FasterRotation: return TEXT("Increase item rotation speed");
	case A::SlowerRotation: return TEXT("Decrease item rotation speed");
	case A::ScrollFloorUp: return TEXT("Scroll floor items up");
	case A::ScrollFloorDown: return TEXT("Scroll floor items down");
	case A::Back: return TEXT("Close interface / Back / Pause");
	default: return TEXT("Retired action");
	}
}
