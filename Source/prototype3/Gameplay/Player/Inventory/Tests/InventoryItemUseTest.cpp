#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Gameplay/Player/Inventory/InventoryComponent.h"
#include "Gameplay/Player/Inventory/PlayerItemUseComponent.h"
#include "Gameplay/Player/Inventory/ItemUseSettings.h"
#include "Gameplay/Player/Inventory/InventoryTestSpikes.h"
#include "Gameplay/Player/Vitals/PlayerVitalsComponent.h"
#include "Gameplay/Items/ItemDefinition.h"
#include "Gameplay/Items/ItemActionData.h"
#include "UI/Inventory/InventoryDemoData.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuickStorageRulesTest, "Prototype.Inventory.QuickStorageRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FQuickStorageRulesTest::RunTest(const FString&)
{
	TStrongObjectPtr<UInventoryComponent> Inventory(NewObject<UInventoryComponent>());
	TestTrue(TEXT("Existing catalog loaded"), InventoryDemo::Populate(Inventory.Get()));
	FInventoryPocket Quick; Quick.Id=TEXT("Quick"); Quick.Size=FVector2D(60,60); Quick.MaxItemMassKg=.6; Quick.bAllowFirearms=false;
	Inventory->AddPocket(Quick);
	FGuid Bandage;
	for (const auto& Entry : Inventory->GetEntries())
	{
		if (Entry.Item.Definition->ItemId==TEXT("Bandage")) Bandage=Entry.Item.InstanceId;
	}
	FInventoryEntry Before;
	Inventory->GetItem(Bandage,Before);
	TestTrue(TEXT("Move valid bandage"), Inventory->MoveItem(Bandage,Quick.Id,FVector2D(30,30),0)==EInventoryResult::Success);
	TestTrue(TEXT("Reserve hands without duplicating item"), Inventory->ReserveItem(Bandage));
	const int32 Count=Inventory->GetEntries().Num();
	FItemInstance Removed;
	TestTrue(TEXT("Cannot remove held item"), Inventory->RemoveItem(Bandage,Removed)==EInventoryResult::InUse);
	TestTrue(TEXT("Cannot move held item"), Inventory->MoveItem(Bandage,TEXT("Main"),Before.Position,0)==EInventoryResult::InUse);
	TestTrue(TEXT("Consume exactly once"), Inventory->ConsumeReservedItem(Bandage));
	TestFalse(TEXT("Cannot double consume"), Inventory->ConsumeReservedItem(Bandage));
	TestEqual(TEXT("One entry consumed"),Inventory->GetEntries().Num(),Count-1);
	for (const auto& Entry : Inventory->GetEntries())
	{
		if (Entry.Item.Definition->ItemId==TEXT("WaterBottle"))
			TestTrue(TEXT("Light but too large is rejected"),Inventory->CheckMove(Entry.Item.InstanceId,Quick.Id,FVector2D(30,30),0)==EInventoryResult::OutOfBounds);
		if (Entry.Item.Definition->ItemId==TEXT("Jacket"))
			TestTrue(TEXT("Per-object weight limit"),Inventory->CheckMove(Entry.Item.InstanceId,Quick.Id,FVector2D(30,30),0)==EInventoryResult::TooHeavy);
	}
	FInventoryItemProfile P;
	P.Id=TEXT("LightFirearm"); P.Definition=NewObject<UItemDefinition>(Inventory.Get());
	P.Definition->ItemId=P.Id; P.Definition->DisplayName=FText::FromString(TEXT("Light firearm")); P.Definition->MassKg=.1;
	P.Definition->Category=FGameplayTag::RequestGameplayTag(TEXT("Item.Category.Weapon.Firearm"));
	FInventoryShapePart Shape; Shape.Vertices={{-5,-5},{5,-5},{5,5},{-5,5}}; P.ShapeParts={Shape};
	Inventory->RegisterProfile(P);
	TestTrue(TEXT("Even small light firearm excluded"),Inventory->CheckPlacement(P.Id,Quick.Id,FVector2D(30,30),0)==EInventoryResult::Incompatible);
	P.Id=TEXT("Throwable"); P.Definition=DuplicateObject<UItemDefinition>(P.Definition,Inventory.Get());
	P.Definition->ItemId=P.Id; P.Definition->Category=FGameplayTag::RequestGameplayTag(TEXT("Item.Category.Weapon.Melee"));
	Inventory->RegisterProfile(P);
	TestTrue(TEXT("Non-firearm tool passes same size and weight policy"),Inventory->CheckPlacement(P.Id,Quick.Id,FVector2D(30,30),0)==EInventoryResult::Success);
	return true;
}

#if WITH_EDITOR
#include "Editor.h"
#include "Tests/AutomationEditorCommon.h"
#include "Core/PlayerControllers/prototype3PlayerController.h"
#include "Core/Characters/prototype3Character.h"
#include "InputKeyEventArgs.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "UI/Inventory/InventoryDemoWidget.h"
#include "UI/Inventory/SInventoryPanel.h"
#include "UI/Inventory/InventoryInputSettings.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

class FItemUsePIECheck : public IAutomationLatentCommand
{
public:
	explicit FItemUsePIECheck(FAutomationTestBase* InTest) : Test(InTest), Started(FPlatformTime::Seconds()) {}
	~FItemUsePIECheck() { FSlateApplication::Get().SetHandleDeviceInputWhenApplicationNotActive(bPreviousInput); }
	bool Update() override
	{
		UWorld* World=GEditor ? GEditor->PlayWorld : nullptr;
		auto* PC=World ? Cast<Aprototype3PlayerController>(World->GetFirstPlayerController()) : nullptr;
		auto* Pawn=PC ? Cast<Aprototype3Character>(PC->GetPawn()) : nullptr;
		auto* Use=Pawn ? Pawn->FindComponentByClass<UPlayerItemUseComponent>() : nullptr;
		auto* Vitals=Pawn ? Pawn->FindComponentByClass<UPlayerVitalsComponent>() : nullptr;
		if (!Use || !Use->Inventory || !Use->Shortcuts || !Vitals || !Vitals->IsAlive())
		{
			if (FPlatformTime::Seconds()-Started<60) return false;
			Test->AddError(TEXT("Item-use PIE failed to initialize.")); return true;
		}
		if (World->GetTimeSeconds() < WaitUntil) return false;
		auto Wait=[&](double Delay) { WaitUntil=World->GetTimeSeconds()+Delay; ++Phase; return false; };
		if (Phase==0)
		{
			bPreviousInput=FSlateApplication::Get().GetHandleDeviceInputWhenApplicationNotActive();
			FSlateApplication::Get().SetHandleDeviceInputWhenApplicationNotActive(true);
			Use->bSavePreferences=false;
			Use->Shortcuts->Keys={EKeys::E,EKeys::F,EKeys::G}; Use->Shortcuts->ItemTypes={FName(TEXT("Bandage")),FName(TEXT("CannedBeans")),NAME_None};
			for (const auto& Entry : Use->Inventory->GetEntries())
			{
				Test->TestTrue(TEXT("No sample stranded in setup"),Entry.PocketId!=TEXT("Setup"));
				if (Entry.Item.Definition->ItemId==TEXT("Bandage")) { if (Entry.PocketId==TEXT("Quick")) QuickBandage=Entry.Item.InstanceId; else BagBandage=Entry.Item.InstanceId; }
			}
			Count=Use->Inventory->GetEntries().Num();
			Vitals->ApplyDamage(40);
			Key(PC,EKeys::E,true); Key(PC,EKeys::E,false);
			Test->TestEqual(TEXT("One press puts bandage in hands"),Use->GetHeldId(),QuickBandage);
			Test->TestFalse(TEXT("Shortcut never starts consumption"),Use->IsUsing());
			Test->TestEqual(TEXT("Shortcut preserves health"),Vitals->GetCurrentHealth(),60.f);
			Key(PC,EKeys::LeftMouseButton,true);
			return Wait(.25);
		}
		if (Phase==1)
		{
			if (FParse::Param(FCommandLine::Get(),TEXT("InventoryCapture"))) FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("InventoryVerification/player-held.png")),false,false);
			Key(PC,EKeys::LeftMouseButton,false);
			Test->TestTrue(TEXT("Real primary input starts use"),Use->IsUsing());
			Key(PC,EKeys::W,true); Key(PC,EKeys::LeftShift,true);
			return Wait(.65);
		}
		if (Phase==2)
		{
			if (FParse::Param(FCommandLine::Get(),TEXT("InventoryCapture"))) FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("InventoryVerification/player-use-hud.png")),true,false);
			Test->TestTrue(TEXT("Only walking during consumption"),Pawn->GetActiveGait()==EPlayerLocomotionGait::Walking);
			Key(PC,EKeys::W,false); Key(PC,EKeys::LeftShift,false);
			Test->TestTrue(TEXT("Use accumulated real elapsed time"),Use->GetProgress()>.1f);
			Vitals->ApplyDamage(10);
			Test->TestEqual(TEXT("Damage resets progress"),Use->GetProgress(),0.f);
			Test->TestFalse(TEXT("Damage cancels use immediately"),Use->IsUsing());
			Test->TestEqual(TEXT("Interrupted bandage remains in hands"),Use->GetHeldId(),QuickBandage);
			return Wait(3.2);
		}
		if (Phase==3)
		{
			Test->TestFalse(TEXT("Healing does not resume automatically"),Use->IsUsing());
			Test->TestEqual(TEXT("No healing after waiting a full use duration"),Vitals->GetCurrentHealth(),50.f);
			Test->TestEqual(TEXT("Interrupted bandage is not consumed"),Use->Inventory->GetEntries().Num(),Count);
			Key(PC,EKeys::LeftMouseButton,true);
			return Wait(.25);
		}
		if (Phase==4)
		{
			Key(PC,EKeys::LeftMouseButton,false);
			Test->TestTrue(TEXT("A new click starts healing again"),Use->IsUsing());
			Test->TestTrue(TEXT("Retry begins with a fresh timer"),Use->GetProgress()>.01f && Use->GetProgress()<.3f);
			return Wait(1.0);
		}
		if (Phase==5)
		{
			Test->TestEqual(TEXT("Retry cannot heal early"),Vitals->GetCurrentHealth(),50.f);
			Test->TestEqual(TEXT("Retry cannot consume early"),Use->Inventory->GetEntries().Num(),Count);
			return Wait(2.0);
		}
		if (Phase==6)
		{
			Test->TestEqual(TEXT("Bandage restores 25 health after manual retry"),Vitals->GetCurrentHealth(),75.f);
			Test->TestEqual(TEXT("Single item consumed"),Use->Inventory->GetEntries().Num(),Count-1);
			Test->TestFalse(TEXT("Hands cleared"),Use->HasHeldItem());
			Key(PC,EKeys::E,true); Key(PC,EKeys::E,false);
			Test->TestFalse(TEXT("Missing quick bandage does not pull from bag"),Use->HasHeldItem());
			Test->TestFalse(TEXT("Closed bag cannot supply hands"),Use->EquipToHands(BagBandage));
			PC->ToggleInventoryDemo();
			return Wait(.15);
		}
		if (Phase==7)
		{
			TArray<UUserWidget*> Widgets; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World,Widgets,UInventoryDemoWidget::StaticClass(),false);
			for (auto* Widget:Widgets) if (Widget->IsInViewport()) Panel=StaticCastSharedPtr<SInventoryPanel>(CastChecked<UInventoryDemoWidget>(Widget)->GetInventoryFocusTarget());
			if (!Panel.IsValid()) { Test->AddError(TEXT("Player inventory UI missing")); return true; }
			Panel->SetSaveControls(false); Capture(TEXT("player-quick.png"));
			Test->TestTrue(TEXT("Backpack UI starts opening delay"),Use->IsOpeningBackpack());
			Test->TestFalse(TEXT("Not accessible during opening"),Use->EquipToHands(BagBandage));
			return Wait(2.25);
		}
		if (Phase==8)
		{
			Test->TestTrue(TEXT("Backpack opens after real frames"),Use->IsBackpackOpen());
			Capture(TEXT("player-backpack.png"));
			FInventoryEntry Entry; Use->Inventory->GetItem(BagBandage,Entry);
			Click(FVector2D(40,150)+Entry.Position);
			if (GetDefault<UInventoryInputSettings>()->bToggleGrab) Click(FVector2D(40,150)+Entry.Position);
			Panel->OnKeyDown(Panel->GetCachedGeometry(), FKeyEvent(GetDefault<UInventoryInputSettings>()->GetKey(EInventoryControl::ToHands),FModifierKeysState(),0,false,0,0));
			Test->TestEqual(TEXT("UI selected bandage reaches hands"),Use->GetHeldId(),BagBandage);
			Test->TestFalse(TEXT("Inventory closed restores movement"),PC->IsMoveInputIgnored());
			Key(PC,EKeys::LeftMouseButton,true);
			return Wait(.25);
		}
		if (Phase==9)
		{
			Key(PC,EKeys::LeftMouseButton,false);
			Test->TestTrue(TEXT("Bag bandage has same use duration"),Use->GetProgress()>.01f && Use->GetProgress()<.3f);
			Use->CancelUse();
			Test->TestEqual(TEXT("Cancel preserves inventory"),Use->Inventory->GetEntries().Num(),Count-1);
			Vitals->Heal(100);
			Use->HandlePrimaryAction();
			Test->TestFalse(TEXT("Full health cannot waste a bandage"),Use->IsUsing());
			Use->Stow();
			const FGuid BagIdentity=Use->Backpack.InstanceId;
			Use->ToggleBackpackEquipment();
			Test->TestFalse(TEXT("Unequipped bag cannot open"),Use->BeginOpenBackpack());
			Use->ToggleBackpackEquipment();
			Test->TestEqual(TEXT("Reequip preserves backpack identity"),Use->Backpack.InstanceId,BagIdentity);
			Test->TestEqual(TEXT("Reequip preserves contents"),Use->Inventory->GetEntries().Num(),Count-1);
			Use->HandleShortcut(1,100); Use->HandleShortcut(1,100.1);
			Test->TestTrue(TEXT("Double tap cycles type"),Use->Shortcuts->ItemTypes[1]!=TEXT("CannedBeans"));
			Test->TestFalse(TEXT("Double tap never consumes"),Use->IsUsing());
			Use->Stow();
			Test->TestTrue(TEXT("Rebind shortcut"),Use->Shortcuts->TryBind(0,EKeys::J,GetDefault<UInventoryInputSettings>()->GetKey(EInventoryControl::Toggle)));
			Test->TestFalse(TEXT("Duplicate shortcut rejected"),Use->Shortcuts->TryBind(1,EKeys::J,EKeys::I));
			int32 SpikeCount=0;
			for (TActorIterator<AInventoryTestSpikes> It(World); It; ++It)
			{
				++SpikeCount;
				Pawn->SetActorLocation(It->GetActorLocation()+FVector(0,0,96),false,nullptr,ETeleportType::TeleportPhysics);
			}
			Test->TestEqual(TEXT("One test hazard spawned"),SpikeCount,1);
			return Wait(1.2);
		}
		if (Phase==10)
		{
			Test->TestTrue(TEXT("Touching spikes causes damage through real overlap"),Vitals->GetCurrentHealth()<100.f);
			for (TActorIterator<AInventoryTestSpikes> It(World); It; ++It) for (int32 I=0; I<20; ++I) It->Hurt(Pawn);
			Test->TestEqual(TEXT("Hazard never kills"),Vitals->GetCurrentHealth(),1.f);
			PC->CloseInventoryDemo();
			return true;
		}
		return true;
	}
private:
	void Key(Aprototype3PlayerController* PC,FKey K,bool Down)
	{
		FInputKeyEventArgs Args; Args.Key=K; Args.Event=Down?IE_Pressed:IE_Released; Args.AmountDepressed=Down?1.f:0.f; PC->InputKey(Args);
	}
	void Click(FVector2D Point)
	{
		FWidgetPath Found; auto& App=FSlateApplication::Get(); App.GeneratePathToWidgetChecked(Panel.ToSharedRef(),Found);
		TArray<FWidgetAndPointer> Widgets; for (int32 I=0; I<Found.Widgets.Num(); ++I) Widgets.Emplace(Found.Widgets[I],TOptional<FVirtualPointerPosition>());
		const FWidgetPath Path(Widgets); const FVector2D Abs=Panel->GetCachedGeometry().LocalToAbsolute(Point);
		App.RoutePointerDownEvent(Path,FPointerEvent(0,App.CursorPointerIndex,Abs,Abs,TSet<FKey>{EKeys::LeftMouseButton},EKeys::LeftMouseButton,0,FModifierKeysState()));
		App.RoutePointerUpEvent(Path,FPointerEvent(0,App.CursorPointerIndex,Abs,Abs,TSet<FKey>(),EKeys::LeftMouseButton,0,FModifierKeysState()));
	}
	void Capture(const TCHAR* Name)
	{
		if (!FParse::Param(FCommandLine::Get(),TEXT("InventoryCapture"))) return;
		// Render a snapshot panel so capture cannot replace the live panel's cached geometry.
		auto* Use=GEditor->PlayWorld->GetFirstPlayerController()->GetPawn()->FindComponentByClass<UPlayerItemUseComponent>();
		const auto Snapshot=SNew(SInventoryPanel).Inventory(Use->Inventory).ItemUse(Use).SaveControls(false);
		FWidgetRenderer Renderer(true); TStrongObjectPtr<UTextureRenderTarget2D> Target(Renderer.DrawWidget(Snapshot,FVector2D(1000,800)));
		TArray<FColor> Pixels; FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
		if (!Target.IsValid() || !Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags)) { Test->AddError(TEXT("Player UI capture failed")); return; }
		TArray64<uint8> PNG; FImageUtils::PNGCompressImageArray(1000,800,Pixels,PNG);
		Test->TestTrue(TEXT("Capture saved"),FFileHelper::SaveArrayToFile(PNG,*(FPaths::ProjectSavedDir()/TEXT("InventoryVerification")/Name)));
	}
	FAutomationTestBase* Test;
	double Started,WaitUntil=0;
	int32 Phase=0,Count=0;
	bool bPreviousInput=false;
	FGuid QuickBandage,BagBandage;
	TSharedPtr<SInventoryPanel> Panel;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FItemUsePIETest,"Prototype.Inventory.ItemUsePlayIntegration",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FItemUsePIETest::RunTest(const FString&)
{
	if (!FParse::Param(FCommandLine::Get(),TEXT("InventoryPIE"))) { AddInfo(TEXT("Use -InventoryPIE for item use integration.")); return true; }
	if (GEditor->PlayWorld) { AddError(TEXT("Run in a fresh verification editor.")); return false; }
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/FirstPerson/Lvl_FirstPerson")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FItemUsePIECheck(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif
#endif
