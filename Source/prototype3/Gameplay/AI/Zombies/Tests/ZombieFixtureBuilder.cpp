#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Gameplay/AI/Zombies/ZombieCharacter.h"
#include "Gameplay/AI/Zombies/ZombieStateTree.h"
#include "StateTree.h"
#include "StateTreeEditorData.h"
#include "StateTreeCompiler.h"
#include "StateTreeCompilerLog.h"
#include "Components/StateTreeAIComponentSchema.h"
#include "Editor.h"
#include "Tests/AutomationEditorCommon.h"
#include "FileHelpers.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/BrushComponent.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavigationSystem.h"
#include "Builders/CubeBuilder.h"
#include "ActorFactories/ActorFactory.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformFileManager.h"
#include "AI/NavDataGenerator.h"
#include "HAL/PlatformTime.h"

class FBuildZombieLabNavigation : public IAutomationLatentCommand
{
public:
	explicit FBuildZombieLabNavigation(FAutomationTestBase* InTest):Test(InTest),Started(FPlatformTime::Seconds()) {}
	bool Update() override
	{
		if (FPlatformTime::Seconds()-Started>30) { Test->AddError(TEXT("Lab navigation remained locked after map load")); return true; }
		UWorld* World=GEditor->GetEditorWorldContext().World();
		auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		if (!Nav || Nav->IsNavigationBuildingLocked(ENavigationBuildLock::AsyncLoadLock)) return false;
		// Temporarily unlock only this world's editor auto-update lock. Do not
		// change the user's global automatic navigation rebuild preference.
		const bool bEditorLock=Nav->IsNavigationBuildingLocked(ENavigationBuildLock::NoUpdateInEditor);
		if (bEditorLock) Nav->RemoveNavigationBuildLock(ENavigationBuildLock::NoUpdateInEditor,UNavigationSystemV1::ELockRemovalRebuildAction::NoRebuild);
		Nav->Build();
		for (const auto& Data : Nav->NavDataSet)
			if (Data && Data->GetGenerator()) Data->GetGenerator()->EnsureBuildCompletion();
		if (bEditorLock) Nav->AddNavigationBuildLock(ENavigationBuildLock::NoUpdateInEditor);
		for (const FVector Point : {FVector(-1500,-1500,0),FVector(1500,-1500,0),FVector(-1500,1500,0),FVector(1500,1500,0)})
		{
			FNavLocation Projected;
			Test->TestTrue(TEXT("Full laboratory floor has navigation"),Nav->ProjectPointToNavigation(Point,Projected,FVector(60,60,200)));
		}
		if (!Test->HasAnyErrors()) Test->TestTrue(TEXT("Save lab with completed navigation"),FEditorFileUtils::SaveLevel(World->PersistentLevel));
		return true;
	}
private:
	FAutomationTestBase* Test;
	double Started;
};

// Repair an existing authored map without recreating its actors or StateTree.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZombieRebuildNavigation,"Prototype.Fixtures.RebuildZombieNavigation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FZombieRebuildNavigation::RunTest(const FString&)
{
	if (!FParse::Param(FCommandLine::Get(),TEXT("ZombieRebuildNavigation")))
	{ AddInfo(TEXT("Opt in with -ZombieRebuildNavigation to rebuild and save lab navigation.")); return true; }
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Zombies/Lvl_ZombieLab")));
	ADD_LATENT_AUTOMATION_COMMAND(FBuildZombieLabNavigation(this));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZombieBuildFixtures, "Prototype.Fixtures.ZombieLab",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FZombieBuildFixtures::RunTest(const FString&)
{
	if (!FParse::Param(FCommandLine::Get(),TEXT("ZombieCreateFixtures")))
	{ AddInfo(TEXT("Opt in with -ZombieCreateFixtures to create the zombie assets.")); return true; }
	const FString TreePath = TEXT("/Game/Zombies/ST_ZombieBasic");
	const FString MapPath = TEXT("/Game/Zombies/Lvl_ZombieLab");
	const FString TreeFile = FPackageName::LongPackageNameToFilename(TreePath,FPackageName::GetAssetPackageExtension());
	const FString MapFile = FPackageName::LongPackageNameToFilename(MapPath,FPackageName::GetMapPackageExtension());
	if (FPaths::FileExists(TreeFile) || FPaths::FileExists(MapFile))
	{ AddError(TEXT("Fixture assets already exist. Keep authored edits; this generator only creates new files.")); return false; }
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(TreeFile),true);
	UPackage* Package = CreatePackage(*TreePath);
	UStateTree* Tree = NewObject<UStateTree>(Package,TEXT("ST_ZombieBasic"),RF_Public|RF_Standalone);
	auto* Data = NewObject<UStateTreeEditorData>(Tree,TEXT("EditorData"));
	Tree->EditorData = Data;
	Data->Schema = NewObject<UStateTreeAIComponentSchema>(Data);
	auto& Root = Data->AddSubTree(TEXT("Zombie"));
	for (int32 Index=0; Index<=static_cast<int32>(EZombieBehavior::Dead); ++Index)
	{
		const auto State = static_cast<EZombieBehavior>(Index);
		auto& Child = Root.AddChildState(FName(*StaticEnum<EZombieBehavior>()->GetNameStringByValue(Index)));
		Child.AddEnterCondition<FZombieStateCondition>().GetNode().Behavior = State;
		Child.AddTask<FZombieBehaviorTask>().GetNode().Behavior = State;
		auto& Transition = Child.AddTransition(EStateTreeTransitionTrigger::OnTick,EStateTreeTransitionType::GotoState,&Root);
		auto& Condition = Transition.AddConditionWithOuter<FZombieStateCondition>(&Child).GetNode();
		Condition.Behavior = State; Condition.bInvert = true;
	}
	FStateTreeCompilerLog Log;
	FStateTreeCompiler Compiler(Log);
	if (!TestTrue(TEXT("Zombie StateTree compiles"),Compiler.Compile(*Tree))) return false;
	FAssetRegistryModule::AssetCreated(Tree);
	FSavePackageArgs Args; Args.TopLevelFlags = RF_Public|RF_Standalone; Args.SaveFlags = SAVE_NoError;
	if (!TestTrue(TEXT("Save zombie StateTree"),UPackage::SavePackage(Package,Tree,*TreeFile,Args))) return false;

	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("Lab world"),World)) return false;
	World->GetWorldSettings()->DefaultGameMode = LoadClass<AGameModeBase>(nullptr,TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode.BP_FirstPersonGameMode_C"));
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
	auto Block = [World,Cube](const TCHAR* Name,FVector Position,FVector Scale)
	{
		auto* Actor = World->SpawnActor<AStaticMeshActor>(Position,FRotator::ZeroRotator);
		Actor->SetActorLabel(Name);
		Actor->GetStaticMeshComponent()->SetStaticMesh(Cube);
		Actor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
		Actor->SetActorScale3D(Scale);
		return Actor;
	};
	Block(TEXT("LabFloor"),FVector(0,0,-20),FVector(40,40,.4));
	Block(TEXT("VisionWall"),FVector(0,250,130),FVector(.4,10,2.6));
	Block(TEXT("CornerWall"),FVector(250,750,130),FVector(5.4,.4,2.6));
	Block(TEXT("LowObstacle"),FVector(750,-600,45),FVector(2.5,1.5,.9));
	World->SpawnActor<APlayerStart>(FVector(-900,-500,100),FRotator::ZeroRotator);
	auto* Z = World->SpawnActor<AZombieCharacter>(FVector(200,-500,90),FRotator::ZeroRotator);
	Z->BehaviorTree = Tree;
	Z->SetActorLabel(TEXT("Zombie_Basic_Edit_Tuning_Here"));
	Z->Tuning.IdleSeconds = 8;
	// Let the existing fists demonstrate interruption in this introductory lab.
	Z->Tuning.InterruptThreshold = 10;
	auto* Light = World->SpawnActor<ADirectionalLight>(FVector(0,0,500),FRotator(-50,-30,0));
	Light->GetLightComponent()->SetIntensity(5);
	auto* Sky = World->SpawnActor<ASkyLight>();
	Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
	Sky->GetLightComponent()->SetIntensity(1);
	auto* Bounds = World->SpawnActor<ANavMeshBoundsVolume>(FVector(0,0,150),FRotator::ZeroRotator);
	auto* Builder = NewObject<UCubeBuilder>(); Builder->X=3950; Builder->Y=3950; Builder->Z=500;
	UActorFactory::CreateBrushForVolumeActor(Bounds,Builder);
	Bounds->GetBrushComponent()->SetCanEverAffectNavigation(false);
	FNavigationSystem::AddNavigationSystemToWorld(*World,FNavigationSystemRunMode::EditorMode);
	if (auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		Nav->OnNavigationBoundsUpdated(Bounds);
		if (auto* Recast = Cast<ARecastNavMesh>(Nav->GetDefaultNavDataInstance(FNavigationSystem::Create)))
		{
			// Edit this map's nav actor only; do not change project-wide navigation settings.
			auto* Mode = FindFProperty<FEnumProperty>(Recast->GetClass(),TEXT("RuntimeGeneration"));
			if (Mode) Mode->GetUnderlyingProperty()->SetIntPropertyValue(Mode->ContainerPtrToValuePtr<void>(Recast),static_cast<int64>(ERuntimeGenerationType::Dynamic));
			Recast->PostEditChange();
		}
		// Bake the complete floor before saving. PIE duplicates the editor's nav
		// data; runtime dirty updates alone only rebuild tiles near moving objects.
	}
	TestTrue(TEXT("Save zombie lab"),FEditorFileUtils::SaveLevel(World->PersistentLevel,MapFile));
	ADD_LATENT_AUTOMATION_COMMAND(FBuildZombieLabNavigation(this));
	return !HasAnyErrors();
}
#endif

