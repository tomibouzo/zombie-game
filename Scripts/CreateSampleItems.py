"""Create missing sample assets in Unreal; existing assets are never overwritten.

Run with UnrealEditor-Cmd, -EnablePlugins=PythonScriptPlugin -run=pythonscript
and -script=<absolute path using forward slashes>, after compiling the project.
All masses and action assignments are provisional. No action executes here.
"""

import unreal


# ID, display name, category suffix, mass kg, trait, primary intent, secondary intent
SAMPLES = [
    ("CannedBeans", "Canned beans", "Consumable.Food", 0.45, "Handheld", None, "Eat"),
    ("WaterBottle", "Water bottle", "Consumable.Drink", 0.55, "Handheld", None, "Drink"),
    ("Knife", "Knife", "Weapon.Melee", 0.20, "Handheld", "MeleeAttack", None),
    ("Pistol", "Pistol", "Weapon.Firearm", 0.90, "Handheld", "Fire", "Aim"),
    ("Flashlight", "Flashlight", "Tool.Lighting", 0.15, "Handheld", None, "ToggleLight"),
    ("Jacket", "Jacket", "Equipment.Clothing", 0.80, "Equippable", None, "Equip"),
    ("SmallBackpack", "Small backpack", "Equipment.Backpack", 0.70, "Equippable", "OpenStorage", "Equip"),
    ("ScrapMetal", "Scrap metal", "Material.Salvage", 0.25, None, None, None),
]

DESCRIPTIONS = {
    "CannedBeans": "A can of beans.",
    "WaterBottle": "A bottle of drinking water.",
    "Knife": "A small handheld knife.",
    "Pistol": "A handgun.",
    "Flashlight": "A handheld flashlight.",
    "Jacket": "A jacket.",
    "SmallBackpack": "A small backpack.",
    "ScrapMetal": "A piece of scrap metal.",
}


def tag(name):
    value = unreal.GameplayTag()
    if not value.import_text('(TagName="' + name + '")'):
        raise RuntimeError("Cannot import gameplay tag: " + name)
    return value


def action(owner, slot, intent):
    if intent is None:
        return None
    value = unreal.new_object(unreal.IntentItemActionData, outer=owner, name=slot)
    value.set_editor_property("action_tag", tag("Item.Action." + intent))
    return value


assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
factory = unreal.DataAssetFactory()
factory.set_editor_property("data_asset_class", unreal.ItemDefinition)
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

for item_id, name, category, mass, trait, primary, secondary in SAMPLES:
    asset_name = "DA_Item_" + item_id
    asset_path = "/Game/Items/" + asset_name
    if assets.does_asset_exist(asset_path):
        unreal.log("Preserving existing asset: " + asset_path)
        continue
    item = asset_tools.create_asset(asset_name, "/Game/Items", unreal.ItemDefinition, factory)
    if item is None:
        raise RuntimeError("Failed to create " + asset_path)
    item.set_editor_property("item_id", item_id)
    item.set_editor_property("display_name", name)
    item.set_editor_property("description", DESCRIPTIONS[item_id])
    item.set_editor_property("category", tag("Item.Category." + category))
    item.set_editor_property("mass_kg", mass)
    item.set_editor_property("max_stack_size", 1)
    if trait:
        traits = unreal.GameplayTagContainer()
        if not traits.import_text('(GameplayTags=((TagName="Item.Trait.' + trait + '")))'):
            raise RuntimeError("Cannot import trait for " + item_id)
        item.set_editor_property("traits", traits)
    item.set_editor_property("primary_action", action(item, "PrimaryIntent", primary))
    item.set_editor_property("secondary_action", action(item, "SecondaryIntent", secondary))
    if not item.is_valid_definition():
        raise RuntimeError("Definition validation failed: " + asset_path)
    if not assets.save_loaded_asset(item):
        raise RuntimeError("Failed to save " + asset_path)
    unreal.log("Created sample item: " + asset_path)

unreal.log("Sample item asset creation complete.")
