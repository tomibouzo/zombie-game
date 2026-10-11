#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Editor.h"
#include "Tests/AutomationEditorCommon.h"
#include "Gameplay/Items/World/DroppedItem.h"
#include "Gameplay/Items/ItemDefinition.h"
#include "Gameplay/Player/Inventory/InventoryComponent.h"
#include "Core/PlayerControllers/prototype3PlayerController.h"
#include "UI/Inventory/InventoryDemoWidget.h"
#include "UI/Inventory/SInventoryPanel.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformTime.h"
#include "UnrealClient.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"

/** Actual Slate input, ownership transfer, Chaos falling/contact and cleanup during Play. */
class FWorldItemDropPIECheck : public IAutomationLatentCommand
{
public:
	explicit FWorldItemDropPIECheck(FAutomationTestBase* InTest) : Test(InTest), Started(FPlatformTime::Seconds()) {}
	virtual ~FWorldItemDropPIECheck() override
	{
		if (SavedSettings.IsValid())
		{
			auto* Settings = GetMutableDefault<UInventoryInputSettings>();
			for (TFieldIterator<FProperty> P(Settings->GetClass()); P; ++P)
				if (P->HasAnyPropertyFlags(CPF_Config)) P->CopyCompleteValue_InContainer(Settings, SavedSettings.Get());
			FSlateApplication::Get().ReleaseAllPointerCapture();
			FSlateApplication::Get().SetHandleDeviceInputWhenApplicationNotActive(bPreviousBackgroundInput);
		}
	}
	virtual bool Update() override
	{
		UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
		auto* Controller = World ? Cast<Aprototype3PlayerController>(World->GetFirstPlayerController()) : nullptr;
		auto* Player = Controller ? Cast<ACharacter>(Controller->GetPawn()) : nullptr;
		if (!Player)
		{
			if (FPlatformTime::Seconds() - Started < 60) return false;
			Test->AddError(TEXT("Drop test could not start a player in PIE."));
			return true;
		}
		if (FPlatformTime::Seconds() - Started > 90)
		{
			Test->AddError(TEXT("Drop test exceeded 90 seconds."));
			return true;
		}
		auto& App = FSlateApplication::Get();
		auto* Settings = GetMutableDefault<UInventoryInputSettings>();
		if (Phase == 0)
		{
			SavedSettings.Reset(DuplicateObject<UInventoryInputSettings>(Settings, GetTransientPackage()));
			Settings->ResetDefaults();
			bPreviousBackgroundInput = App.GetHandleDeviceInputWhenApplicationNotActive();
			App.SetHandleDeviceInputWhenApplicationNotActive(true);
			auto* Floor = World->SpawnActor<AStaticMeshActor>(FloorCenter, FRotator::ZeroRotator);
			Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			Floor->SetActorScale3D(FVector(4.4, 4.4, 0.2));
			Floor->GetStaticMeshComponent()->SetCollisionObjectType(ECC_WorldStatic);
			Floor->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Floor->GetStaticMeshComponent()->SetCollisionResponseToAllChannels(ECR_Block);
			Player->GetCharacterMovement()->DisableMovement();
			Controller->ToggleInventoryDemo();
			Phase = 1;
			return false;
		}
		if (Phase == 1)
		{
			TArray<UUserWidget*> Widgets;
			UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, UInventoryDemoWidget::StaticClass(), false);
			for (UUserWidget* Widget : Widgets)
			{
				if (!Widget->IsInViewport()) continue;
				Panel = StaticCastSharedPtr<SInventoryPanel>(CastChecked<UInventoryDemoWidget>(Widget)->GetInventoryFocusTarget());
				ForEachObjectWithOuter(Widget, [&](UObject* Object)
				{
					if (auto* Component = Cast<UInventoryComponent>(Object)) Inventory = Component;
				});
			}
			if (!Panel.IsValid() || !Inventory.IsValid())
			{
				Test->AddError(TEXT("Drop test could not find the live inventory."));
				return true;
			}
			Panel->SetSaveControls(false);
			TSet<FName> Seen;
			for (const auto& Entry : Inventory->GetEntries())
			{
				const FName Id = Entry.Item.Definition->ItemId;
				if (Id.ToString().StartsWith(TEXT("Demo_"))) { FixtureId = Entry.Item.InstanceId; continue; }
				if (Seen.Contains(Id)) { ExtraBandageId = Entry.Item.InstanceId; continue; }
				Seen.Add(Id);
				Items.Add(Entry);
			}
			if (!Test->TestEqual(TEXT("Nine real item types available"), Items.Num(), 9)) return true;
			InitialCount = Inventory->GetEntries().Num();
			Key(EKeys::F);
			Test->TestEqual(TEXT("Drop without selection does not remove an item"), Inventory->GetEntries().Num(), InitialCount);
			FString Error;
			Test->TestNull(TEXT("Missing player preserves source"), ADroppedItem::DropFromInventory(Inventory.Get(), Items[0].Item.InstanceId, nullptr, Error));
			Test->TestNull(TEXT("Missing instance creates no actor"), ADroppedItem::DropFromInventory(Inventory.Get(), FGuid::NewGuid(), Player, Error));
			Test->TestNull(TEXT("Geometry fixtures cannot be dropped"), ADroppedItem::DropFromInventory(Inventory.Get(), FixtureId, Player, Error));
			Test->TestEqual(TEXT("Failed requests preserve all entries"), Inventory->GetEntries().Num(), InitialCount);
			Phase = 2;
		}
		if (Phase == 2)
		{
			const auto& Entry = Items[Index];
			FVector Position = Slot(Index);
			Position.Z += Player->GetSimpleCollisionHalfHeight() + (Index == 1 ? 200 : 0);
			Player->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
			Player->SetActorRotation(FRotator::ZeroRotator);
			Settings->bToggleGrab = Index % 2 != 0;
			FString Error;
			const FKey DropKey = Settings->bToggleGrab ? EKeys::Z : EKeys::F;
			Test->TestTrue(TEXT("Drop can be rebound"), Settings->TrySetKey(EInventoryControl::Drop, DropKey, Error));
			Grab(Entry);
			if (Settings->bToggleGrab) MouseButton(false);
			// Alternate selected-only and held drops through the same mapped control.
			if (Index % 2 == 0) MouseButton(false);
			if (DropKey != EKeys::F)
			{
				Key(EKeys::F);
				FInventoryEntry StillHeld;
				Test->TestTrue(TEXT("Old key does nothing after rebinding"), Inventory->GetItem(Entry.Item.InstanceId, StillHeld));
			}
			Key(DropKey);
			Key(DropKey); // Repeated presses cannot duplicate the transfer.
			MouseButton(false);
			ADroppedItem* Found = nullptr;
			int32 Count = 0;
			for (TActorIterator<ADroppedItem> It(World); It; ++It)
				if (It->GetItem().InstanceId == Entry.Item.InstanceId) { Found = *It; ++Count; }
			Test->TestEqual(TEXT("One world actor per dropped instance"), Count, 1);
			if (!Found) return true;
			Dropped.Add(Found);
			SpawnZ = Found->GetActorLocation().Z;
			Test->TestTrue(TEXT("Definition and quantity preserved"), Found->GetItem().Definition == Entry.Item.Definition && Found->GetItem().Quantity == Entry.Item.Quantity);
			Test->TestEqual(TEXT("Storage profile preserved for future pickup"), Found->GetInventoryProfileId(), Entry.ProfileId);
			Test->TestTrue(TEXT("Drop at player position"), FVector2D(Found->GetActorLocation()).Equals(FVector2D(Position), 0.01));
			Test->TestTrue(TEXT("Gravity and simulation enabled"), Found->GetBody()->IsSimulatingPhysics() && Found->GetBody()->IsGravityEnabled());
			Test->TestTrue(TEXT("Static level geometry blocks"), Found->GetBody()->GetCollisionResponseToChannel(ECC_WorldStatic) == ECR_Block);
			for (ECollisionChannel Channel : { ECC_Pawn, ECC_PhysicsBody, ECC_WorldDynamic })
				Test->TestTrue(TEXT("Player, items and movable props ignored"), Found->GetBody()->GetCollisionResponseToChannel(Channel) == ECR_Ignore);
			Test->TestEqual(TEXT("Source loses exactly one item"), Inventory->GetEntries().Num(), InitialCount - Index - 1);
			Test->TestFalse(TEXT("Drop releases mouse capture"), Panel->HasMouseCapture());
			WaitUntil = FPlatformTime::Seconds() + 0.35;
			Phase = 3;
			return false;
		}
		if (FPlatformTime::Seconds() < WaitUntil) return false;
		if (Phase == 3)
		{
			if (Index == 1)
			{
				Test->TestTrue(TEXT("Airborne drop falls during real Play frames"), Dropped[Index].IsValid() && Dropped[Index]->GetActorLocation().Z < SpawnZ - 10);
			}
			if (++Index < Items.Num()) { Phase = 2; return false; }
			FVector Position = Slot(0); Position.Z += Player->GetSimpleCollisionHalfHeight();
			Player->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
			FString Error;
			OverlapItem = ADroppedItem::DropFromInventory(Inventory.Get(), ExtraBandageId, Player, Error);
			Test->TestTrue(TEXT("Drop permitted over an existing item"), OverlapItem.IsValid());
			WaitUntil = FPlatformTime::Seconds() + 2;
			Phase = 4;
			return false;
		}
		if (Phase == 4)
		{
			for (const auto& Actor : Dropped)
			{
				if (!Test->TestTrue(TEXT("Dropped item survives on the floor"), Actor.IsValid())) continue;
				const double Bottom = Actor->GetActorLocation().Z - Actor->GetBody()->GetScaledBoxExtent().Z;
				Test->TestTrue(TEXT("Placeholder lands on the floor"), FMath::Abs(Bottom - FloorTop()) < 2);
				Test->TestTrue(TEXT("Landed item comes to rest"), Actor->GetVelocity().Size() < 5);
			}
			if (OverlapItem.IsValid() && Dropped[0].IsValid())
			{
				Test->TestTrue(TEXT("Two items can rest at the same spot under the player"), OverlapItem->GetActorLocation().Equals(Dropped[0]->GetActorLocation(), 2));
				OverlapItem->SetActorLocation(FVector(FloorCenter.X, FloorCenter.Y, OverlapItem->GetCleanupZ() - 100), false, nullptr, ETeleportType::TeleportPhysics);
			}
			WaitUntil = FPlatformTime::Seconds() + 0.2;
			Phase = 5;
			return false;
		}
		if (Phase == 5)
		{
			Test->TestFalse(TEXT("Below-world item is destroyed"), OverlapItem.IsValid());
			Test->TestEqual(TEXT("Only the three geometry fixtures remain"), Inventory->GetEntries().Num(), 3);
			Controller->CloseInventoryDemo();
			if (!FParse::Param(FCommandLine::Get(), TEXT("InventoryCapture"))) return true;
			Player->SetActorHiddenInGame(true);
			const FVector CameraPosition = FloorCenter + FVector(-260, -290, 380);
			auto* Camera = World->SpawnActor<ACameraActor>(CameraPosition, (FloorCenter - CameraPosition).Rotation());
			Camera->GetCameraComponent()->SetFieldOfView(45);
			Controller->SetViewTarget(Camera);
			WaitUntil = FPlatformTime::Seconds() + 1;
			Phase = 6;
			return false;
		}
		if (Phase == 6)
		{
			ScreenshotPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("InventoryVerification/dropped-items.png"));
			FScreenshotRequest::RequestScreenshot(ScreenshotPath, false, false);
			WaitUntil = FPlatformTime::Seconds() + 2;
			Phase = 7;
			return false;
		}
		Test->TestTrue(TEXT("World item capture written"), FPlatformFileManager::Get().GetPlatformFile().FileExists(*ScreenshotPath));
		return true;
	}
private:
	double FloorTop() const { return FloorCenter.Z + 10; }
	FVector Slot(int32 I) const { return FVector(FloorCenter.X - 100 + (I % 3) * 100, FloorCenter.Y - 100 + (I / 3) * 100, FloorTop()); }
	void Key(FKey K)
	{
		FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(K, FModifierKeysState(), 0, false, 0, 0));
		FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(K, FModifierKeysState(), 0, false, 0, 0));
	}
	void Grab(const FInventoryEntry& Entry)
	{
		FInventoryItemProfile Profile;
		Inventory->GetProfile(Entry.ProfileId, Profile);
		const auto Parts = Profile.GetTransformedParts(Entry.Position, Entry.AngleDegrees);
		Pointer = FVector2D::ZeroVector;
		for (FVector2D Vertex : Parts[0].Vertices) Pointer += Vertex;
		Pointer = Pointer / Parts[0].Vertices.Num() + FVector2D(40, 150);
		MouseButton(true);
	}
	void MouseButton(bool Down)
	{
		FWidgetPath Path;
		FSlateApplication::Get().GeneratePathToWidgetChecked(Panel.ToSharedRef(), Path);
		TArray<FWidgetAndPointer> PointerPath;
		for (int32 I = 0; I < Path.Widgets.Num(); ++I) PointerPath.Emplace(Path.Widgets[I], TOptional<FVirtualPointerPosition>());
		Path = FWidgetPath(PointerPath);
		const FVector2D Absolute = Panel->GetCachedGeometry().LocalToAbsolute(Pointer);
		const TSet<FKey> Buttons = Down ? TSet<FKey>{EKeys::LeftMouseButton} : TSet<FKey>();
		const FPointerEvent Event(0, FSlateApplication::CursorPointerIndex, Absolute, Absolute, Buttons, EKeys::LeftMouseButton, 0, FModifierKeysState());
		if (Down) FSlateApplication::Get().RoutePointerDownEvent(Path, Event);
		else FSlateApplication::Get().RoutePointerUpEvent(Path, Event);
	}
	FAutomationTestBase* Test;
	TStrongObjectPtr<UInventoryInputSettings> SavedSettings;
	TWeakObjectPtr<UInventoryComponent> Inventory;
	TSharedPtr<SInventoryPanel> Panel;
	TArray<FInventoryEntry> Items;
	TArray<TWeakObjectPtr<ADroppedItem>> Dropped;
	TWeakObjectPtr<ADroppedItem> OverlapItem;
	FGuid FixtureId, ExtraBandageId;
	const FVector FloorCenter = FVector(5000, 5000, 1000);
	FVector2D Pointer;
	FString ScreenshotPath;
	double Started, WaitUntil = 0, SpawnZ = 0;
	int32 Phase = 0, Index = 0, InitialCount = 0;
	bool bPreviousBackgroundInput = false;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldItemDropPIETest, "Prototype.Inventory.DropPlayIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorldItemDropPIETest::RunTest(const FString&)
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("InventoryPIE")))
	{
		AddInfo(TEXT("World drop integration skipped: use -InventoryPIE in a fresh verification editor."));
		return true;
	}
	if (GEditor->PlayWorld) { AddError(TEXT("Run outside an existing Play session.")); return false; }
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/FirstPerson/Lvl_FirstPerson")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWorldItemDropPIECheck(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif
