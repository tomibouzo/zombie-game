#include "Gameplay/Items/ItemActionData.h"
#include "Gameplay/Items/ItemDefinition.h"
#include "Gameplay/Items/ItemInstance.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FItemModelValidationTest, "Prototype.Items.ModelValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FItemModelValidationTest::RunTest(const FString& Parameters)
{
	UItemDefinition* Definition = NewObject<UItemDefinition>();
	Definition->ItemId = TEXT("TestMedicalItem");
	Definition->DisplayName = FText::FromString(TEXT("Test medical item"));
	Definition->Category = FGameplayTag::RequestGameplayTag(TEXT("Item.Category.Consumable.Medical"));
	Definition->MassKg = 0.05;

	TestTrue(TEXT("Complete item definition is valid"), Definition->IsValidDefinition());
	const FItemInstance First = FItemInstance::Create(Definition);
	const FItemInstance Second = FItemInstance::Create(Definition);
	TestTrue(TEXT("Created item is valid"), First.IsValid());
	TestTrue(TEXT("Separate items get separate identities"), First.InstanceId != Second.InstanceId);
	TestFalse(TEXT("Zero quantity is rejected"), FItemInstance::Create(Definition, 0).IsValid());
	TestFalse(TEXT("Quantity above maximum is rejected"), FItemInstance::Create(Definition, 2).IsValid());

	UHealingItemActionData* Healing = NewObject<UHealingItemActionData>(Definition);
	Healing->HealAmount = 25.0;
	Definition->SecondaryAction = Healing;
	TestTrue(TEXT("Positive healing data is valid"), Definition->IsValidDefinition());
	Healing->HealAmount = 0.0;
	TestFalse(TEXT("Zero healing data is rejected"), Definition->IsValidDefinition());
	Healing->HealAmount = 25.0;
	Definition->MassKg = 0.0;
	TestFalse(TEXT("Missing mass is rejected"), Definition->IsValidDefinition());
	Definition->MassKg = -1.0;
	TestFalse(TEXT("Negative mass is rejected"), Definition->IsValidDefinition());
	Definition->MassKg = 0.05;
	Definition->ItemId = NAME_None;
	TestFalse(TEXT("Missing item ID is rejected"), Definition->IsValidDefinition());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBandageAssetTest, "Prototype.Items.BandageAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBandageAssetTest::RunTest(const FString& Parameters)
{
	const UItemDefinition* Bandage = LoadObject<UItemDefinition>(nullptr,
		TEXT("/Game/Items/DA_Item_Bandage.DA_Item_Bandage"));
	if (!TestNotNull(TEXT("Bandage Data Asset loads"), Bandage))
	{
		return false;
	}
	TestTrue(TEXT("Bandage definition validates"), Bandage->IsValidDefinition());
	TestEqual(TEXT("Stable item ID"), Bandage->ItemId, FName(TEXT("Bandage")));
	TestEqual(TEXT("Display name"), Bandage->DisplayName.ToString(), FString(TEXT("Bandage")));
	TestEqual(TEXT("Mass in kilograms"), Bandage->MassKg, 0.05);
	TestEqual(TEXT("Bandage maximum quantity"), Bandage->MaxStackSize, 1);
	TestEqual(TEXT("Medical category"), Bandage->Category,
		FGameplayTag::RequestGameplayTag(TEXT("Item.Category.Consumable.Medical")));
	TestTrue(TEXT("Handheld trait"), Bandage->Traits.HasTagExact(
		FGameplayTag::RequestGameplayTag(TEXT("Item.Trait.Handheld"))));
	TestNull(TEXT("No primary action yet"), Bandage->PrimaryAction.Get());
	const UHealingItemActionData* Healing = Cast<UHealingItemActionData>(Bandage->SecondaryAction.Get());
	if (TestNotNull(TEXT("Secondary action contains healing data"), Healing))
	{
		TestEqual(TEXT("Healing amount"), Healing->HealAmount, 25.0);
	}
	return true;
}
#endif
