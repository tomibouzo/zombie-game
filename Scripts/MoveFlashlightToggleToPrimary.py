"""One-time update of DA_Item_Flashlight: ToggleLight moves from the secondary to the primary slot.

Run with UnrealEditor-Cmd, -EnablePlugins=PythonScriptPlugin -run=pythonscript
and -script=<absolute path using forward slashes>, after compiling the project.
Safe to run again: it does nothing when the asset already has the new layout.
"""

import unreal

PATH = "/Game/Items/DA_Item_Flashlight"
TAG = "Item.Action.ToggleLight"


def tag_name(action):
    return str(action.get_editor_property("action_tag").get_editor_property("tag_name")) if action else ""


assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
item = assets.load_asset(PATH)
if item is None:
    raise RuntimeError("Missing asset: " + PATH)

primary = item.get_editor_property("primary_action")
secondary = item.get_editor_property("secondary_action")
if tag_name(primary) == TAG and secondary is None:
    unreal.log("Flashlight already toggles on the primary action; nothing changed.")
else:
    if primary is not None and tag_name(primary) != TAG:
        raise RuntimeError("Flashlight has an unexpected primary action; not overwriting it.")
    value = unreal.new_object(unreal.IntentItemActionData, outer=item, name="PrimaryToggleLight")
    tag = unreal.GameplayTag()
    if not tag.import_text('(TagName="' + TAG + '")'):
        raise RuntimeError("Cannot import gameplay tag: " + TAG)
    value.set_editor_property("action_tag", tag)
    item.set_editor_property("primary_action", value)
    item.set_editor_property("secondary_action", None)
    if not item.is_valid_definition():
        raise RuntimeError("Flashlight definition failed validation after the update.")
    if not assets.save_loaded_asset(item):
        raise RuntimeError("Failed to save " + PATH)
    unreal.log("Flashlight: ToggleLight moved to the primary action.")
