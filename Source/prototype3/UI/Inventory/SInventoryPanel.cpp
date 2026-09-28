#include "UI/Inventory/SInventoryPanel.h"
#include "Gameplay/Player/Inventory/InventoryComponent.h"
#include "Gameplay/Items/ItemDefinition.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"

namespace
{
bool InRect(FVector2D P, double X, double Y, double W, double H)
{
	return P.X >= X && P.X <= X + W && P.Y >= Y && P.Y <= Y + H;
}
}

void SInventoryPanel::Construct(const FArguments& Args)
{
	Inventory = Args._Inventory;
	Controls = Args._Controls ? Args._Controls : GetMutableDefault<UInventoryInputSettings>();
	bSaveControls = Args._SaveControls;
	OnClose = Args._OnClose;
	SetCanTick(true);
}

void SInventoryPanel::Tick(const FGeometry& Geometry, double Time, float Delta)
{
	SLeafWidget::Tick(Geometry, Time, Delta);
	if (Controls.IsValid() && Active() && (bTurnLeft || bTurnRight))
		Turn((static_cast<int32>(bTurnRight) - static_cast<int32>(bTurnLeft)) * Controls->TurnSpeed * Delta);
}

bool SInventoryPanel::HitItem(FVector2D Point, FInventoryEntry& OutEntry) const
{
	if (!Inventory.IsValid()) return false;
	const FVector2D Local = Point - PocketOrigin();
	const auto Pockets = Inventory->GetPockets();
	if (Pockets.IsEmpty() || !InRect(Local, 0, 0, Pockets[0].Size.X, Pockets[0].Size.Y)) return false;
	const auto Entries = Inventory->GetEntries();
	for (int32 I = Entries.Num() - 1; I >= 0; --I)
	{
		FInventoryItemProfile Profile;
		if (Entries[I].PocketId == Pockets[0].Id && Inventory->GetProfile(Entries[I].ProfileId, Profile)
			&& Profile.Contains(Local, Entries[I].Position, Entries[I].AngleDegrees))
		{
			OutEntry = Entries[I];
			return true;
		}
	}
	return false;
}

FString SInventoryPanel::Describe(EInventoryResult Result)
{
	switch (Result)
	{
	case EInventoryResult::Success: return TEXT("Posicion valida");
	case EInventoryResult::Occupied: return TEXT("Hay superposicion con otro objeto");
	case EInventoryResult::OutOfBounds: return TEXT("Parte del objeto queda fuera del espacio");
	case EInventoryResult::InvalidRotation: return TEXT("Este objeto no permite ese giro");
	default: return TEXT("No se puede colocar este objeto");
	}
}

EInventoryResult SInventoryPanel::Preview(FName& PocketId, FVector2D& Center) const
{
	Center = Cursor - PocketOrigin() - GrabOffset;
	if (!Inventory.IsValid()) return EInventoryResult::InvalidPocket;
	const auto Pockets = Inventory->GetPockets();
	if (Pockets.IsEmpty()) return EInventoryResult::InvalidPocket;
	PocketId = Pockets[0].Id;
	return bAdding ? Inventory->CheckPlacement(Pending.ProfileId, PocketId, Center, PreviewAngle)
		: Inventory->CheckMove(SelectedId, PocketId, Center, PreviewAngle);
}

void SInventoryPanel::CancelGesture()
{
	bDragging = bAdding = bTurnLeft = bTurnRight = false;
	Pending = FInventoryEntry();
	GrabOffset = FVector2D::ZeroVector;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SInventoryPanel::CommitGesture()
{
	if (!Active()) return;
	FName Pocket;
	FVector2D Center;
	EInventoryResult Result = Preview(Pocket, Center);
	if (Result == EInventoryResult::Success)
	{
		if (bAdding)
		{
			Result = Inventory->AddItem(Pending.Item, Pending.ProfileId, Pocket, Center, PreviewAngle);
			if (Result == EInventoryResult::Success) SelectedId = Pending.Item.InstanceId;
		}
		else Result = Inventory->MoveItem(SelectedId, Pocket, Center, PreviewAngle);
	}
	Status = Result == EInventoryResult::Success ? TEXT("Objeto colocado.") : Describe(Result) + TEXT(". Colocacion cancelada.");
	CancelGesture();
}

void SInventoryPanel::Turn(double Delta)
{
	if (!Active()) return;
	PreviewAngle = InventoryGeometry::NormalizeAngle(PreviewAngle + Delta);
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SInventoryPanel::RemoveSelected()
{
	if (Active() || !Inventory.IsValid()) return;
	FItemInstance Removed;
	if (Inventory->RemoveItem(SelectedId, Removed) == EInventoryResult::Success)
	{
		SelectedId.Invalidate();
		Status = TEXT("Objeto retirado del inventario de prueba.");
	}
}

void SInventoryPanel::BeginBandage()
{
	CancelGesture();
	FInventoryItemProfile Profile;
	if (!Inventory.IsValid() || !Inventory->GetProfile(TEXT("Bandage_TestOnly"), Profile)) return;
	Pending.Item = FItemInstance::Create(Profile.Definition);
	Pending.ProfileId = Profile.Id;
	bAdding = Pending.Item.IsValid();
	PreviewAngle = 0;
	Status = TEXT("Coloca la venda con tu control de agarrar / soltar.");
}

void SInventoryPanel::SavePreferences()
{
	if (bSaveControls && Controls.IsValid()) Controls->SaveConfig();
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SInventoryPanel::AssignKey(FKey Key)
{
	if (!Controls.IsValid() || Rebinding == INDEX_NONE) return;
	FString Error;
	if (Controls->TrySetKey(static_cast<EInventoryControl>(Rebinding), Key, Error))
	{
		Rebinding = INDEX_NONE;
		SavePreferences();
		Status = TEXT("Control actualizado.");
	}
	else Status = Error;
	Invalidate(EInvalidateWidgetReason::Paint);
}

bool SInventoryPanel::HandleButton()
{
	if (!Controls.IsValid()) return false;
	if (Rebinding != INDEX_NONE)
	{
		if (InRect(Cursor, 630, 150, 330, 28))
		{
			Rebinding = INDEX_NONE;
			Status = TEXT("Asignacion cancelada.");
			return true;
		}
		return false;
	}
	for (int32 I = 0; I < static_cast<int32>(EInventoryControl::Count); ++I)
		if (InRect(Cursor, 630, 190 + I * 42, 330, 36))
		{
			CancelGesture();
			Rebinding = I;
			Status = TEXT("Pulsa la nueva tecla o boton. Haz clic en 'Cancelar asignacion' para salir.");
			return true;
		}
	if (InRect(Cursor, 630, 500, 330, 38))
	{
		Controls->bToggleGrab = !Controls->bToggleGrab;
		SavePreferences();
		return true;
	}
	if (InRect(Cursor, 630, 552, 44, 32) || InRect(Cursor, 916, 552, 44, 32))
	{
		Controls->TurnSpeed = FMath::Clamp(Controls->TurnSpeed + (Cursor.X < 700 ? -15.f : 15.f), 15.f, 360.f);
		SavePreferences();
		return true;
	}
	if (InRect(Cursor, 630, 598, 330, 34))
	{
		Controls->ResetDefaults();
		SavePreferences();
		Status = TEXT("Controles predeterminados restaurados.");
		return true;
	}
	if (InRect(Cursor, 630, 650, 155, 40)) { BeginBandage(); return true; }
	if (InRect(Cursor, 805, 650, 155, 40)) { RemoveSelected(); return true; }
	return false;
}

FReply SInventoryPanel::Reply()
{
	FReply Result = FReply::Handled().SetUserFocus(SharedThis(this));
	// Recapturing the same widget first sends OnMouseCaptureLost, cancelling the gesture.
	// Keep the existing capture while rotating or releasing another held control.
	if (Active() && !HasMouseCapture()) Result.CaptureMouse(SharedThis(this));
	else if (!Active() && HasMouseCapture()) Result.ReleaseMouseCapture();
	return Result;
}

FReply SInventoryPanel::Press(FKey Key)
{
	if (!Controls.IsValid()) return FReply::Handled();
	if (Rebinding != INDEX_NONE) { AssignKey(Key); return Reply(); }
	if (Key == Controls->GetKey(EInventoryControl::Toggle))
	{
		CancelGesture(); OnClose.ExecuteIfBound();
		return FReply::Handled().ReleaseMouseCapture();
	}
	if (Key == Controls->GetKey(EInventoryControl::Cancel))
	{
		CancelGesture();
		Status = TEXT("Cancelado. Posicion y angulo originales conservados.");
		return Reply();
	}
	if (Key == Controls->GetKey(EInventoryControl::Grab))
	{
		if (bAdding || (bDragging && Controls->bToggleGrab)) CommitGesture();
		else if (!bDragging)
		{
			FInventoryEntry Entry;
			if (HitItem(Cursor, Entry))
			{
				SelectedId = Entry.Item.InstanceId;
				Pending = Entry;
				PreviewAngle = Entry.AngleDegrees;
				GrabOffset = Cursor - PocketOrigin() - Entry.Position;
				bDragging = true;
			}
			else SelectedId.Invalidate();
		}
	}
	if (Active())
	{
		if (Key == Controls->GetKey(EInventoryControl::TurnLeft)) bTurnLeft = true;
		if (Key == Controls->GetKey(EInventoryControl::TurnRight)) bTurnRight = true;
	}
	else
	{
		if (Key == Controls->GetKey(EInventoryControl::Remove)) RemoveSelected();
		if (Key == Controls->GetKey(EInventoryControl::Add)) BeginBandage();
	}
	Invalidate(EInvalidateWidgetReason::Paint);
	return Reply();
}

FReply SInventoryPanel::Release(FKey Key)
{
	if (!Controls.IsValid()) return FReply::Handled();
	if (Key == Controls->GetKey(EInventoryControl::TurnLeft)) bTurnLeft = false;
	if (Key == Controls->GetKey(EInventoryControl::TurnRight)) bTurnRight = false;
	if (Key == Controls->GetKey(EInventoryControl::Grab) && bDragging && !Controls->bToggleGrab) CommitGesture();
	return Reply();
}

FReply SInventoryPanel::OnMouseButtonDown(const FGeometry& G, const FPointerEvent& Event)
{
	Cursor = G.AbsoluteToLocal(Event.GetScreenSpacePosition());
	if (!Active() && Event.GetEffectingButton() == EKeys::LeftMouseButton && InRect(Cursor, 880, 25, 80, 42))
	{
		CancelGesture(); Rebinding = INDEX_NONE; OnClose.ExecuteIfBound();
		return FReply::Handled().ReleaseMouseCapture();
	}
	if (!Active() && Event.GetEffectingButton() == EKeys::LeftMouseButton && HandleButton())
	{
		Invalidate(EInvalidateWidgetReason::Paint);
		return Reply();
	}
	return Press(Event.GetEffectingButton());
}

FReply SInventoryPanel::OnMouseButtonDoubleClick(const FGeometry& G, const FPointerEvent& Event)
{
	return OnMouseButtonDown(G, Event);
}

FReply SInventoryPanel::OnMouseButtonUp(const FGeometry& G, const FPointerEvent& Event)
{
	Cursor = G.AbsoluteToLocal(Event.GetScreenSpacePosition());
	return Release(Event.GetEffectingButton());
}

FReply SInventoryPanel::OnMouseMove(const FGeometry& G, const FPointerEvent& Event)
{
	Cursor = G.AbsoluteToLocal(Event.GetScreenSpacePosition());
	Invalidate(EInvalidateWidgetReason::Paint);
	return FReply::Handled();
}

FReply SInventoryPanel::OnMouseWheel(const FGeometry& G, const FPointerEvent& Event)
{
	Cursor = G.AbsoluteToLocal(Event.GetScreenSpacePosition());
	const FKey Key = Event.GetWheelDelta() >= 0 ? EKeys::MouseScrollUp : EKeys::MouseScrollDown;
	if (Rebinding != INDEX_NONE) { AssignKey(Key); return Reply(); }
	if (Controls.IsValid() && Active())
	{
		if (Key == Controls->GetKey(EInventoryControl::TurnLeft)) Turn(-FMath::Abs(Event.GetWheelDelta()) * 2);
		if (Key == Controls->GetKey(EInventoryControl::TurnRight)) Turn(FMath::Abs(Event.GetWheelDelta()) * 2);
	}
	return Reply();
}

FReply SInventoryPanel::OnKeyDown(const FGeometry&, const FKeyEvent& Event)
{
	return Event.IsRepeat() ? FReply::Handled() : Press(Event.GetKey());
}
FReply SInventoryPanel::OnKeyUp(const FGeometry&, const FKeyEvent& Event) { return Release(Event.GetKey()); }
void SInventoryPanel::OnMouseCaptureLost(const FCaptureLostEvent&) { CancelGesture(); }
void SInventoryPanel::OnFocusLost(const FFocusEvent&) { CancelGesture(); Rebinding = INDEX_NONE; }

int32 SInventoryPanel::OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle&, bool) const
{
	const FSlateBrush* Brush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	auto Box = [&](FVector2D Pos, FVector2D Size, FLinearColor Color)
	{
		FSlateDrawElement::MakeBox(Out, Layer++, G.ToPaintGeometry(Size, FSlateLayoutTransform(Pos)), Brush, ESlateDrawEffect::None, Color);
	};
	auto Text = [&](FVector2D Pos, const FString& Value, int32 Size, FLinearColor Color = FLinearColor::White)
	{
		FSlateDrawElement::MakeText(Out, Layer++, G.ToPaintGeometry(FVector2D(1, 1), FSlateLayoutTransform(Pos)),
			Value, FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), Size), ESlateDrawEffect::None, Color);
	};
	auto Shape = [&](const FInventoryItemProfile& Profile, FVector2D Center, double Angle, FLinearColor Color)
	{
		TArray<FSlateVertex> Vertices;
		TArray<SlateIndex> Indices;
		for (const auto& Part : Profile.GetTransformedParts(PocketOrigin() + Center, Angle))
		{
			const int32 Base = Vertices.Num();
			for (FVector2D P : Part.Vertices)
				Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(G.GetAccumulatedRenderTransform(), FVector2f(P), FVector2f::ZeroVector, Color.ToFColor(true)));
			for (int32 I = 1; I + 1 < Part.Vertices.Num(); ++I)
			{
				Indices.Add(static_cast<SlateIndex>(Base));
				Indices.Add(static_cast<SlateIndex>(Base + I));
				Indices.Add(static_cast<SlateIndex>(Base + I + 1));
			}
		}
		const auto Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*Brush);
		FSlateDrawElement::MakeCustomVerts(Out, Layer++, Handle, Vertices, Indices, nullptr, 0, 0);
	};
	const FLinearColor Muted(0.65f, 0.72f, 0.78f), Button(0.12f, 0.16f, 0.21f);
	Box(FVector2D::ZeroVector, FVector2D(1000, 800), FLinearColor(0.022f, 0.03f, 0.043f));
	Text(FVector2D(40, 27), TEXT("INVENTARIO / espacio libre"), 25);
	Text(FVector2D(40, 76), TEXT("Mueve y gira cada silueta. Los bordes pueden tocarse."), 15, Muted);
	Box(FVector2D(880, 25), FVector2D(80, 42), Button);
	Text(FVector2D(890, 36), TEXT("Cerrar"), 13);
	Text(FVector2D(40, 118), TEXT("ESPACIO DE PRUEBA"), 14, Muted);
	if (!Inventory.IsValid() || !Controls.IsValid()) return Layer;
	const auto Pockets = Inventory->GetPockets();
	if (Pockets.IsEmpty()) return Layer;
	Box(PocketOrigin() - FVector2D(2), Pockets[0].Size + FVector2D(4), FLinearColor(0.22f, 0.3f, 0.36f));
	Box(PocketOrigin(), Pockets[0].Size, FLinearColor(0.055f, 0.075f, 0.095f));
	const auto Entries = Inventory->GetEntries();
	for (const auto& Entry : Entries)
	{
		if (Entry.PocketId != Pockets[0].Id || (bDragging && Entry.Item.InstanceId == SelectedId)) continue;
		FInventoryItemProfile Profile;
		if (Inventory->GetProfile(Entry.ProfileId, Profile))
			Shape(Profile, Entry.Position, Entry.AngleDegrees,
				Entry.Item.InstanceId == SelectedId ? FLinearColor(0.43f, 0.62f, 0.73f) : FLinearColor(0.3f, 0.4f, 0.47f));
	}
	Text(FVector2D(630, 150), Rebinding == INDEX_NONE ? TEXT("CONTROLES / clic para cambiar") : TEXT("Cancelar asignacion"), 14, Muted);
	for (int32 I = 0; I < static_cast<int32>(EInventoryControl::Count); ++I)
	{
		const auto Action = static_cast<EInventoryControl>(I);
		const float Y = 190 + I * 42;
		Box(FVector2D(630, Y), FVector2D(330, 36), Rebinding == I ? FLinearColor(0.22f, 0.36f, 0.4f) : Button);
		Text(FVector2D(640, Y + 10), UInventoryInputSettings::Label(Action), 12);
		FString KeyLabel = Controls->GetKey(Action).GetDisplayName().ToString();
		if (Rebinding == I) KeyLabel = TEXT("Pulsa un control...");
		Text(FVector2D(802, Y + 11), KeyLabel, 10, Muted);
	}
	Box(FVector2D(630, 500), FVector2D(330, 38), Button);
	Text(FVector2D(640, 511), Controls->bToggleGrab ? TEXT("Agarre: un clic para tomar y otro para soltar") : TEXT("Agarre: mantener pulsado y soltar"), 11);
	Box(FVector2D(630, 552), FVector2D(44, 32), Button);
	Box(FVector2D(916, 552), FVector2D(44, 32), Button);
	Text(FVector2D(646, 556), TEXT("-"), 18);
	Text(FVector2D(931, 556), TEXT("+"), 18);
	Text(FVector2D(693, 560), FString::Printf(TEXT("Giro: %.0f grados / s"), Controls->TurnSpeed), 12, Muted);
	Box(FVector2D(630, 598), FVector2D(330, 34), Button);
	Text(FVector2D(690, 607), TEXT("Restaurar controles"), 12);
	Box(FVector2D(630, 650), FVector2D(155, 40), FLinearColor(0.12f, 0.28f, 0.25f));
	Text(FVector2D(646, 663), TEXT("+ Anadir venda"), 12);
	Box(FVector2D(805, 650), FVector2D(155, 40), Button);
	Text(FVector2D(824, 663), TEXT("Retirar objeto"), 12);
	FInventoryEntry Selected;
	if (Inventory->GetItem(SelectedId, Selected))
		Text(FVector2D(630, 704), Selected.Item.Definition->DisplayName.ToString() + FString::Printf(TEXT("  /  %.1f grados"), Active() ? PreviewAngle : Selected.AngleDegrees), 11, Muted);

	if (Active())
	{
		FName Pocket;
		FVector2D Center;
		const auto Result = Preview(Pocket, Center);
		FInventoryItemProfile Profile;
		if (Inventory->GetProfile(Pending.ProfileId, Profile))
		{
			// Show the actual rotated silhouette, including the portion outside the storage area.
			Out.PushClip(FSlateClippingZone(G));
			Shape(Profile, Center, PreviewAngle, Result == EInventoryResult::Success ? FLinearColor(0.09f, 0.68f, 0.35f) : FLinearColor(0.84f, 0.12f, 0.12f));
			Out.PopClip();
		}
		Text(FVector2D(40, 738), Describe(Result) + FString::Printf(TEXT("  |  %.1f grados"), PreviewAngle), 14,
			Result == EInventoryResult::Success ? FLinearColor(0.3f, 1, 0.6f) : FLinearColor(1, 0.4f, 0.3f));
	}
	else Text(FVector2D(40, 738), Status, 13, Muted);
	Text(FVector2D(40, 775), FString::Printf(TEXT("%d objetos  |  Formas provisionales  |  Los objetos se reinician al terminar Play"), Entries.Num()), 11, Muted);
	return Layer;
}
