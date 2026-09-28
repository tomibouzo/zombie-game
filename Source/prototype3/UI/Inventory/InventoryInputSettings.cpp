#include "UI/Inventory/InventoryInputSettings.h"

UInventoryInputSettings::UInventoryInputSettings() { ResetDefaults(); }

void UInventoryInputSettings::ResetDefaults()
{
	Keys = { EKeys::I, EKeys::LeftMouseButton, EKeys::Q, EKeys::E, EKeys::RightMouseButton, EKeys::Delete, EKeys::B };
	bToggleGrab = false;
	TurnSpeed = 120;
}

void UInventoryInputSettings::PostInitProperties()
{
	Super::PostInitProperties();
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
	if (Key == EKeys::MouseScrollUp || Key == EKeys::MouseScrollDown)
		return Action == EInventoryControl::TurnLeft || Action == EInventoryControl::TurnRight;
	return true;
}

bool UInventoryInputSettings::TrySetKey(EInventoryControl Action, FKey Key, FString& Error)
{
	if (!Supports(Action, Key))
	{
		Error = TEXT("Usa una tecla o boton. La rueda se admite para girar.");
		return false;
	}
	for (int32 I = 0; I < Keys.Num(); ++I)
		if (I != static_cast<int32>(Action) && Keys[I] == Key)
		{
			Error = TEXT("Ya asignado a: ") + Label(static_cast<EInventoryControl>(I));
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
	case EInventoryControl::Toggle: return TEXT("Abrir / cerrar");
	case EInventoryControl::Grab: return TEXT("Agarrar / soltar");
	case EInventoryControl::TurnLeft: return TEXT("Girar izquierda");
	case EInventoryControl::TurnRight: return TEXT("Girar derecha");
	case EInventoryControl::Cancel: return TEXT("Cancelar");
	case EInventoryControl::Remove: return TEXT("Retirar objeto");
	case EInventoryControl::Add: return TEXT("Anadir venda");
	default: return FString();
	}
}
