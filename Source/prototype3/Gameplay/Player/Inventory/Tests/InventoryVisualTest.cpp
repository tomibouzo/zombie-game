#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "UI/Inventory/SInventoryPanel.h"
#include "UI/Inventory/InventoryDemoData.h"
#include "Gameplay/Player/Inventory/InventoryComponent.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/StrongObjectPtr.h"
#include "InputCoreTypes.h"
#include "HAL/FileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryVisualTest, "Prototype.Inventory.VisualCapture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInventoryVisualTest::RunTest(const FString&)
{
	// Opt-in because the normal data/interaction suite can run with -NullRHI.
	if (!FParse::Param(FCommandLine::Get(), TEXT("InventoryCapture")))
	{
		AddInfo(TEXT("Image capture skipped: use -InventoryCapture with a rendering RHI."));
		return true;
	}
	TStrongObjectPtr<UInventoryComponent> Inventory(NewObject<UInventoryComponent>());
	if (!TestTrue(TEXT("Fixture loaded"), InventoryDemo::Populate(Inventory.Get()))) return false;
	const auto Panel = SNew(SInventoryPanel).Inventory(Inventory.Get());
	FWidgetRenderer Renderer(true);
	auto Capture = [&](const TCHAR* Filename)
	{
		TStrongObjectPtr<UTextureRenderTarget2D> Target(Renderer.DrawWidget(Panel, FVector2D(1000, 650)));
		if (!TestNotNull(TEXT("UI render target"), Target.Get())) return;
		TArray<FColor> Pixels;
		FReadSurfaceDataFlags ReadFlags;
		ReadFlags.SetLinearToGamma(false); // Slate has already applied display gamma.
		if (!TestTrue(TEXT("UI pixels readable"), Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, ReadFlags))) return;
		TArray64<uint8> PNG;
		FImageUtils::PNGCompressImageArray(1000, 650, Pixels, PNG);
		const FString Path = FPaths::ProjectSavedDir() / TEXT("InventoryVerification") / Filename;
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		TestTrue(TEXT("Screenshot saved"), FFileHelper::SaveArrayToFile(PNG, *Path));
		AddInfo(Path);
	};
	Capture(TEXT("inventory-initial.png"));
	const FGeometry G = FGeometry::MakeRoot(FVector2D(1000, 650), FSlateLayoutTransform());
	auto Mouse = [](FVector2D Point, FKey Button) { return FPointerEvent(0, Point, Point, TSet<FKey>(), Button, 0, FModifierKeysState()); };
	Panel->OnMouseButtonDown(G, Mouse(FVector2D(66, 216), EKeys::LeftMouseButton));
	Panel->OnMouseMove(G, Mouse(FVector2D(658, 268), EKeys::Invalid));
	Capture(TEXT("inventory-collision.png"));
	Panel->OnMouseMove(G, Mouse(FVector2D(762, 320), EKeys::Invalid));
	Capture(TEXT("inventory-valid.png"));
	return true;
}
#endif
