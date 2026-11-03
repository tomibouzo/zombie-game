#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Gameplay/Procedural/HouseLayout.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseRetryTest, "Prototype.House.GenerationRetries",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseRetryTest::RunTest(const FString& Parameters)
{
 FHouseGenerationSettings S, Effective;
 S.Navigation.Pattern = EHouseLayoutPattern::CentralHub;
 S.Navigation.Difficulty = EHouseNavigationDifficulty::Normal;
 S.WidthCells = S.DepthCells = 24;
 S.RoomCount = 5;
 S.Household.Profile = EHouseholdProfile::Couple;
 FHouseLayout L;
 FString Error;
 int32 Attempts = 0;
 if (!TestTrue(TEXT("Undersized preset is repaired"), FHouseLayoutGenerator::GenerateWithRetries(S, Effective, L, Attempts, Error))) return false;
 TestEqual(TEXT("Minimum normal room count"), Effective.RoomCount, 8);
 TestEqual(TEXT("Input stays intact"), S.RoomCount, 5);
 TestTrue(TEXT("Successful house validates with effective settings"), FHouseLayoutGenerator::Validate(Effective, L, Error));
 FHouseLayout Repeat;
 TestTrue(TEXT("Successful seed reproduces directly"), FHouseLayoutGenerator::Generate(Effective, Repeat, Error));
 TestEqual(TEXT("Repeat furniture count"), Repeat.Furniture.Num(), L.Furniture.Num());
 for (int32 I = 0; I < L.Furniture.Num(); ++I) TestEqual(TEXT("Repeat furniture placement"), Repeat.Furniture[I].Center, L.Furniture[I].Center);
 // Old map defaults must work across preset changes without mutating the request.
 for (int32 Pattern = 1; Pattern <= 2; ++Pattern)
  for (int32 Difficulty = 0; Difficulty < 3; ++Difficulty)
   for (int32 Profile : {1, 3, 5})
    for (int32 Seed = 0; Seed < 8; ++Seed)
    {
     FHouseGenerationSettings OldMap;
     OldMap.Seed = Seed;
     OldMap.Navigation.Pattern = static_cast<EHouseLayoutPattern>(Pattern);
     OldMap.Navigation.Difficulty = static_cast<EHouseNavigationDifficulty>(Difficulty);
     OldMap.Household.Profile = static_cast<EHouseholdProfile>(Profile);
     const bool Success = FHouseLayoutGenerator::GenerateWithRetries(OldMap, Effective, L, Attempts, Error);
     if (!TestTrue(*FString::Printf(TEXT("Old defaults pattern %d difficulty %d profile %d seed %d: %s"), Pattern, Difficulty, Profile, Seed, *Error), Success)) return false;
     TestEqual(TEXT("Difficulty achieved"), L.Navigation.MaxDoorDepth, Difficulty + 2);
     TestEqual(TEXT("Envelope input preserved"), OldMap.DepthCells, 10);
     TestTrue(TEXT("Generated candidate validates"), FHouseLayoutGenerator::Validate(Effective, L, Error));
    }
 FHouseGenerationSettings Strict;
 Strict.Navigation.Pattern = EHouseLayoutPattern::CentralHub;
 Strict.Navigation.Difficulty = EHouseNavigationDifficulty::Hard;
 Strict.bAllowLargerFootprint = false;
 TestFalse(TEXT("Strict small envelope remains a hard limit"), FHouseLayoutGenerator::GenerateWithRetries(Strict, Effective, L, Attempts, Error));
 TestEqual(TEXT("Strict envelope never grows"), Effective.DepthCells, Strict.DepthCells);
 // A larger custom bed fits some plans but not others: exercise real placement recovery.
 bool bRecovered = false;
 S.RoomCount = 8;
 for (const FVector Size : {FVector(240, 280, 60), FVector(220, 260, 60), FVector(260, 300, 60)})
 {
 if (bRecovered) break;
 S.FurnitureRules[0].Size = Size;
 for (int32 Seed = 0; Seed < 40 && !bRecovered; ++Seed)
 {
  S.Seed = Seed;
  if (FHouseLayoutGenerator::Generate(S, Repeat, Error)) continue;
  if (!FHouseLayoutGenerator::GenerateWithRetries(S, Effective, L, Attempts, Error)) continue;
  bRecovered = Attempts > 1;
  TestTrue(TEXT("Retry chooses another seed"), Effective.Seed != S.Seed);
  FHouseGenerationSettings Again;
  int32 AgainAttempts = 0;
  TestTrue(TEXT("Retry sequence is reproducible"), FHouseLayoutGenerator::GenerateWithRetries(S, Again, Repeat, AgainAttempts, Error));
  TestEqual(TEXT("Same effective seed"), Again.Seed, Effective.Seed);
  TestEqual(TEXT("Same retry count"), AgainAttempts, Attempts);
 }
 }
 TestTrue(TEXT("Impossible placement recovers with a new house"), bRecovered);
 S.FurnitureRules[0].Size = FVector(9000, 9000, 60);
 TestFalse(TEXT("Impossible furniture terminates"), FHouseLayoutGenerator::GenerateWithRetries(S, Effective, L, Attempts, Error));
 TestEqual(TEXT("Bounded retry count"), Attempts, 16);
 TestTrue(TEXT("No partial house on exhaustion"), L.Rooms.IsEmpty());
 S.CellSize = -1;
 TestFalse(TEXT("Malformed settings rejected immediately"), FHouseLayoutGenerator::GenerateWithRetries(S, Effective, L, Attempts, Error));
 TestEqual(TEXT("No seed retries for invalid settings"), Attempts, 0);
 return true;
}
#endif
