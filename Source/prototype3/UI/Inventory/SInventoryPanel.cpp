#include "UI/Inventory/SInventoryPanel.h"
#include "Gameplay/Player/Inventory/InventoryComponent.h"
#include "Gameplay/Items/ItemDefinition.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "InputCoreTypes.h"

namespace { constexpr float CellSize = 52.0f; }

void SInventoryPanel::Construct(const FArguments& Args)
{
	Inventory = Args._Inventory;
	OnClose = Args._OnClose;
	SetCanTick(false);
}

FVector2D SInventoryPanel::PocketOrigin(int32 Index) { return FVector2D(Index == 0 ? 40 : 580, 190); }

bool SInventoryPanel::Locate(FVector2D Point, FName& PocketId, FIntPoint& Cell) const
{
	if (!Inventory.IsValid()) return false;
	const TArray<FInventoryPocket> Pockets = Inventory->GetPockets();
	for (int32 Index = 0; Index < Pockets.Num(); ++Index)
	{
		const FVector2D Local = (Point - PocketOrigin(Index)) / CellSize;
		if (Local.X >= 0 && Local.Y >= 0 && Local.X < Pockets[Index].Size.X && Local.Y < Pockets[Index].Size.Y)
		{
			PocketId = Pockets[Index].Id;
			Cell = FIntPoint(FMath::FloorToInt(Local.X), FMath::FloorToInt(Local.Y));
			return true;
		}
	}
	return false;
}

bool SInventoryPanel::HitItem(FVector2D Point, FInventoryEntry& OutEntry, FIntPoint& Cell) const
{
	FName Pocket;
	if (!Locate(Point, Pocket, Cell)) return false;
	for (const FInventoryEntry& Entry : Inventory->GetEntries())
	{
		FInventoryItemProfile Profile;
		if (Entry.PocketId == Pocket && Inventory->GetProfile(Entry.ProfileId, Profile)
			&& Profile.GetRotatedCells(Entry.QuarterTurns).Contains(Cell - Entry.Position))
		{
			OutEntry = Entry;
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
	case EInventoryResult::Occupied: return TEXT("Colision: hay otra pieza");
	case EInventoryResult::OutOfBounds: return TEXT("Fuera del bolsillo");
	case EInventoryResult::InvalidRotation: return TEXT("Rotacion no permitida");
	default: return TEXT("Operacion rechazada: revisa el objeto y su perfil");
	}
}

EInventoryResult SInventoryPanel::Preview(FName& PocketId, FIntPoint& Position) const
{
	if (!Inventory.IsValid() || !Locate(Cursor, PocketId, Position)) return EInventoryResult::OutOfBounds;
	Position -= GrabOffset;
	return bAdding ? Inventory->CheckPlacement(Pending.ProfileId, PocketId, Position, PreviewRotation)
		: Inventory->CheckMove(SelectedId, PocketId, Position, PreviewRotation);
}

void SInventoryPanel::CancelGesture()
{
	bDragging = false;
	bAdding = false;
	Pending = FInventoryEntry();
	GrabOffset = FIntPoint::ZeroValue;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SInventoryPanel::CommitGesture()
{
	FName Pocket;
	FIntPoint Position;
	EInventoryResult Result = Preview(Pocket, Position);
	if (Result == EInventoryResult::Success)
	{
		if (bAdding)
		{
			Result = Inventory->AddItem(Pending.Item, Pending.ProfileId, Pocket, Position, PreviewRotation);
			if (Result == EInventoryResult::Success) SelectedId = Pending.Item.InstanceId;
		}
		else Result = Inventory->MoveItem(SelectedId, Pocket, Position, PreviewRotation);
	}
	Status = Result == EInventoryResult::Success ? TEXT("Colocado. Identidad conservada al mover.") : Describe(Result) + TEXT(". Sin cambios.");
	CancelGesture();
}

void SInventoryPanel::Rotate()
{
	if (!Inventory.IsValid()) return;
	if (bDragging || bAdding)
	{
		PreviewRotation = (PreviewRotation + 1) % 4;
		// Rotation reanchors the preview at the cursor's cell.
		GrabOffset = FIntPoint::ZeroValue;
	}
	else
	{
		FInventoryEntry Entry;
		if (Inventory->GetItem(SelectedId, Entry))
			Status = Describe(Inventory->MoveItem(SelectedId, Entry.PocketId, Entry.Position, (Entry.QuarterTurns + 1) % 4));
	}
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SInventoryPanel::RemoveSelected()
{
	if (bDragging || bAdding || !Inventory.IsValid()) return;
	FItemInstance Removed;
	if (Inventory->RemoveItem(SelectedId, Removed) == EInventoryResult::Success)
	{
		SelectedId.Invalidate();
		Status = TEXT("Objeto retirado del inventario de prueba.");
	}
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SInventoryPanel::BeginBandage()
{
	CancelGesture();
	FInventoryItemProfile Profile;
	if (!Inventory.IsValid() || !Inventory->GetProfile(TEXT("Bandage_TestOnly"), Profile)) return;
	Pending.Item = FItemInstance::Create(Profile.Definition);
	Pending.ProfileId = Profile.Id;
	bAdding = Pending.Item.IsValid();
	PreviewRotation = 0;
	Status = TEXT("Haz clic en una casilla para colocar la venda. Boton derecho cancela.");
	Invalidate(EInvalidateWidgetReason::Paint);
}

FReply SInventoryPanel::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	Cursor = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
	if (Event.GetEffectingButton() == EKeys::RightMouseButton)
	{
		CancelGesture();
		Status = TEXT("Cancelado. El objeto conserva su posicion.");
		return FReply::Handled().ReleaseMouseCapture();
	}
	if (Event.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Handled();
	if (Cursor.X >= 870 && Cursor.Y >= 25 && Cursor.Y <= 70)
	{
		CancelGesture(); OnClose.ExecuteIfBound(); return FReply::Handled().ReleaseMouseCapture();
	}
	if (Cursor.Y >= 565 && Cursor.Y <= 607)
	{
		if (Cursor.X >= 40 && Cursor.X < 230) BeginBandage();
		else if (Cursor.X >= 245 && Cursor.X < 420) Rotate();
		else if (Cursor.X >= 435 && Cursor.X < 650) RemoveSelected();
		return FReply::Handled().SetUserFocus(SharedThis(this));
	}
	if (bAdding) { CommitGesture(); return FReply::Handled().SetUserFocus(SharedThis(this)); }
	FInventoryEntry Entry;
	FIntPoint Cell;
	if (HitItem(Cursor, Entry, Cell))
	{
		SelectedId = Entry.Item.InstanceId;
		Pending = Entry;
		PreviewRotation = Entry.QuarterTurns;
		GrabOffset = Cell - Entry.Position;
		bDragging = true;
		Invalidate(EInvalidateWidgetReason::Paint);
		return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this));
	}
	SelectedId.Invalidate();
	Invalidate(EInvalidateWidgetReason::Paint);
	return FReply::Handled().SetUserFocus(SharedThis(this));
}

FReply SInventoryPanel::OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
	Cursor = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton && bDragging) CommitGesture();
	return FReply::Handled().ReleaseMouseCapture();
}

FReply SInventoryPanel::OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	Cursor = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
	Invalidate(EInvalidateWidgetReason::Paint);
	return FReply::Handled();
}

FReply SInventoryPanel::OnKeyDown(const FGeometry&, const FKeyEvent& Event)
{
	if (Event.IsRepeat()) return FReply::Handled();
	if (Event.GetKey() == EKeys::I)
	{
		CancelGesture(); OnClose.ExecuteIfBound(); return FReply::Handled().ReleaseMouseCapture();
	}
	if (Event.GetKey() == EKeys::Escape) { CancelGesture(); return FReply::Handled().ReleaseMouseCapture(); }
	if (Event.GetKey() == EKeys::R) Rotate();
	if (Event.GetKey() == EKeys::Delete) RemoveSelected();
	return FReply::Handled();
}

void SInventoryPanel::OnMouseCaptureLost(const FCaptureLostEvent&) { CancelGesture(); }
void SInventoryPanel::OnFocusLost(const FFocusEvent&) { CancelGesture(); }

int32 SInventoryPanel::OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle&, bool) const
{
	const FSlateBrush* Brush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	auto Box = [&](FVector2D Pos, FVector2D Size, FLinearColor Color)
	{
		FSlateDrawElement::MakeBox(Out, Layer++, G.ToPaintGeometry(Size, FSlateLayoutTransform(Pos)), Brush, ESlateDrawEffect::None, Color);
	};
	auto Text = [&](FVector2D Pos, const FString& Value, int32 Size, FLinearColor Color = FLinearColor::White)
	{
		FSlateDrawElement::MakeText(Out, Layer++, G.ToPaintGeometry(FVector2D(1, 1), FSlateLayoutTransform(Pos)), Value,
			FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), Size), ESlateDrawEffect::None, Color);
	};
	Box(FVector2D::ZeroVector, FVector2D(1000, 650), FLinearColor(0.025f, 0.032f, 0.043f, 1));
	Text(FVector2D(40, 27), TEXT("INVENTARIO / laboratorio"), 25);
	Text(FVector2D(40, 75), TEXT("Formas provisionales. La venda 1 x 1 no tiene dimensiones finales."), 14, FLinearColor(0.7f, 0.76f, 0.82f));
	Text(FVector2D(40, 103), TEXT("Arrastrar: mover   |   R: girar   |   Boton derecho: cancelar   |   I: cerrar"), 13);
	Box(FVector2D(870, 25), FVector2D(95, 45), FLinearColor(0.16f, 0.19f, 0.24f));
	Text(FVector2D(881, 35), TEXT("Cerrar I"), 14);
	if (!Inventory.IsValid()) { Text(FVector2D(40, 170), TEXT("Inventario no disponible"), 18); return Layer; }
	const auto Pockets = Inventory->GetPockets();
	const auto Entries = Inventory->GetEntries();
	for (int32 Index = 0; Index < Pockets.Num(); ++Index)
	{
		const auto& Pocket = Pockets[Index];
		const FVector2D Origin = PocketOrigin(Index);
		Text(Origin - FVector2D(0, 33), FString::Printf(TEXT("%s   /   %d x %d"), Index == 0 ? TEXT("BOLSILLO PRINCIPAL") : TEXT("BOLSILLO LATERAL"), Pocket.Size.X, Pocket.Size.Y), 15);
		for (int32 Y = 0; Y < Pocket.Size.Y; ++Y)
			for (int32 X = 0; X < Pocket.Size.X; ++X)
				Box(Origin + FVector2D(X, Y) * CellSize, FVector2D(CellSize - 2), FLinearColor(0.085f, 0.10f, 0.13f));
		for (const auto& Entry : Entries)
		{
			if (Entry.PocketId != Pocket.Id) continue;
			FInventoryItemProfile Profile;
			if (!Inventory->GetProfile(Entry.ProfileId, Profile)) continue;
			const auto Cells = Profile.GetRotatedCells(Entry.QuarterTurns);
			const bool bSelected = Entry.Item.InstanceId == SelectedId;
			for (FIntPoint Cell : Cells)
			{
				const FVector2D Pos = Origin + FVector2D(Cell + Entry.Position) * CellSize;
				Box(Pos + FVector2D(2), FVector2D(CellSize - 6), bSelected ? FLinearColor(0.49f, 0.59f, 0.67f) : FLinearColor(0.32f, 0.35f, 0.39f));
			}
			if (Cells.Num()) Text(Origin + FVector2D(Cells[0] + Entry.Position) * CellSize + FVector2D(9, 12), Entry.ProfileId == TEXT("Bandage_TestOnly") ? TEXT("V") : TEXT("T"), 16);
		}
	}
	FName HoverPocket;
	FIntPoint HoverPosition;
	if (bDragging || bAdding)
	{
		const EInventoryResult Result = Preview(HoverPocket, HoverPosition);
		FInventoryItemProfile Profile;
		if (Inventory->GetProfile(Pending.ProfileId, Profile))
		{
			const int32 Index = Pockets.IndexOfByPredicate([&](const FInventoryPocket& P) { return P.Id == HoverPocket; });
			if (Index != INDEX_NONE)
				for (FIntPoint Cell : Profile.GetRotatedCells(PreviewRotation))
				{
					const FIntPoint Location = Cell + HoverPosition;
					// Keep out-of-pocket previews away from labels and controls.
					if (Location.X < 0 || Location.Y < 0 || Location.X >= Pockets[Index].Size.X || Location.Y >= Pockets[Index].Size.Y) continue;
					Box(PocketOrigin(Index) + FVector2D(Location) * CellSize + FVector2D(5), FVector2D(CellSize - 12),
						Result == EInventoryResult::Success ? FLinearColor(0.1f, 0.7f, 0.4f, 0.85f) : FLinearColor(0.9f, 0.18f, 0.14f, 0.85f));
				}
		}
		Text(FVector2D(40, 527), Describe(Result), 15, Result == EInventoryResult::Success ? FLinearColor(0.3f, 1, 0.6f) : FLinearColor(1, 0.4f, 0.3f));
	}
	else Text(FVector2D(40, 527), Status, 14);
	FInventoryEntry Selected;
	if (Inventory->GetItem(SelectedId, Selected))
	{
		Text(FVector2D(580, 385), Selected.Item.Definition->DisplayName.ToString(), 16);
		Text(FVector2D(580, 415), FString::Printf(TEXT("Cantidad: %d   Giro: %d grados"), Selected.Item.Quantity, Selected.QuarterTurns * 90), 13);
		Text(FVector2D(580, 441), TEXT("ID: ") + SelectedId.ToString().Left(13), 12, FLinearColor(0.65f, 0.7f, 0.76f));
	}
	Box(FVector2D(40, 565), FVector2D(190, 42), FLinearColor(0.14f, 0.29f, 0.26f));
	Text(FVector2D(54, 575), TEXT("+ Anadir venda"), 14);
	Box(FVector2D(245, 565), FVector2D(175, 42), FLinearColor(0.16f, 0.19f, 0.24f));
	Text(FVector2D(258, 575), TEXT("Girar / R"), 14);
	Box(FVector2D(435, 565), FVector2D(215, 42), FLinearColor(0.16f, 0.19f, 0.24f));
	Text(FVector2D(448, 575), TEXT("Retirar / Supr"), 14);
	Text(FVector2D(40, 620), FString::Printf(TEXT("%d objetos   |   V = venda real; T = pieza de prueba   |   Datos temporales de esta sesion"), Entries.Num()), 12, FLinearColor(0.65f, 0.7f, 0.76f));
	return Layer;
}
