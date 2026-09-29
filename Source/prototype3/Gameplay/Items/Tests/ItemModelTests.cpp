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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FItemIntentValidationTest, "Prototype.Items.IntentValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FItemIntentValidationTest::RunTest(const FString& Parameters)
{
	UIntentItemActionData* Intent = NewObject<UIntentItemActionData>();
	TestFalse(TEXT("Missing intent is invalid"), Intent->IsValidActionData());
	Intent->ActionTag = FGameplayTag::RequestGameplayTag(TEXT("Item.Category.Weapon.Firearm"));
	TestFalse(TEXT("An item category is not an action"), Intent->IsValidActionData());
	Intent->ActionTag = FGameplayTag::RequestGameplayTag(TEXT("Item.Action"));
	TestFalse(TEXT("The root does not identify an action"), Intent->IsValidActionData());
	Intent->ActionTag = FGameplayTag::RequestGameplayTag(TEXT("Item.Action.Fire"));
	TestTrue(TEXT("A concrete intent is valid"), Intent->IsValidActionData());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FItemSampleAssetsTest, "Prototype.Items.SampleAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FItemSampleAssetsTest::RunTest(const FString& Parameters)
{
	struct FExpected
	{
		const TCHAR* Id;
		const TCHAR* Category;
		double MassKg;
		const TCHAR* Trait;
		const TCHAR* Primary;
		const TCHAR* Secondary;
	};
	const FExpected Expected[] = {
		{ TEXT("CannedBeans"), TEXT("Consumable.Food"), 0.45, TEXT("Handheld"), nullptr, TEXT("Eat") },
		{ TEXT("WaterBottle"), TEXT("Consumable.Drink"), 0.55, TEXT("Handheld"), nullptr, TEXT("Drink") },
		{ TEXT("Knife"), TEXT("Weapon.Melee"), 0.20, TEXT("Handheld"), TEXT("MeleeAttack"), nullptr },
		{ TEXT("Pistol"), TEXT("Weapon.Firearm"), 0.90, TEXT("Handheld"), TEXT("Fire"), TEXT("Aim") },
		{ TEXT("Flashlight"), TEXT("Tool.Lighting"), 0.15, TEXT("Handheld"), nullptr, TEXT("ToggleLight") },
		{ TEXT("Jacket"), TEXT("Equipment.Clothing"), 0.80, TEXT("Equippable"), nullptr, TEXT("Equip") },
		{ TEXT("SmallBackpack"), TEXT("Equipment.Backpack"), 0.70, TEXT("Equippable"), TEXT("OpenStorage"), TEXT("Equip") },
		{ TEXT("ScrapMetal"), TEXT("Material.Salvage"), 0.25, nullptr, nullptr, nullptr },
	};
	TSet<FName> Ids;
	for (const FExpected& Sample : Expected)
	{
		const FString Path = FString::Printf(TEXT("/Game/Items/DA_Item_%s.DA_Item_%s"), Sample.Id, Sample.Id);
		UItemDefinition* Item = LoadObject<UItemDefinition>(nullptr, *Path);
		if (!TestNotNull(Path, Item)) continue;
		const FString Label = FString(Sample.Id) + TEXT(": ");
		TestTrue(Label + TEXT("definition validates"), Item->IsValidDefinition());
		TestEqual(Label + TEXT("stable ID"), Item->ItemId, FName(Sample.Id));
		TestFalse(Label + TEXT("ID is unique in catalog"), Ids.Contains(Item->ItemId));
		Ids.Add(Item->ItemId);
		TestEqual(Label + TEXT("category"), Item->Category.ToString(), FString(TEXT("Item.Category.")) + Sample.Category);
		TestEqual(Label + TEXT("provisional mass per unit"), Item->MassKg, Sample.MassKg);
		TestEqual(Label + TEXT("unstackable"), Item->MaxStackSize, 1);
		TestEqual(Label + TEXT("trait count"), Item->Traits.Num(), Sample.Trait ? 1 : 0);
		if (Sample.Trait)
			TestTrue(Label + TEXT("handling trait"), Item->Traits.HasTagExact(FGameplayTag::RequestGameplayTag(FName(FString(TEXT("Item.Trait.")) + Sample.Trait))));
		auto CheckAction = [&](const UItemActionData* Action, const TCHAR* IntentName, const TCHAR* Slot)
		{
			if (!IntentName) { TestNull(Label + Slot + TEXT(" unassigned"), Action); return; }
			const UIntentItemActionData* Intent = Cast<UIntentItemActionData>(Action);
			if (!TestNotNull(Label + Slot + TEXT(" intent survives reload"), Intent)) return;
			TestTrue(Label + Slot + TEXT(" owned by definition"), Intent->GetOuter() == Item);
			TestEqual(Label + Slot + TEXT(" action"), Intent->ActionTag.ToString(), FString(TEXT("Item.Action.")) + IntentName);
		};
		CheckAction(Item->PrimaryAction, Sample.Primary, TEXT("primary"));
		CheckAction(Item->SecondaryAction, Sample.Secondary, TEXT("secondary"));
		TestTrue(Label + TEXT("instance can be created"), FItemInstance::Create(Item).IsValid());
		TestFalse(Label + TEXT("stack of two rejected"), FItemInstance::Create(Item, 2).IsValid());
	}
	return true;
}
#endif
