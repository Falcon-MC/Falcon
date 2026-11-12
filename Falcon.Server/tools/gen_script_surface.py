#!/usr/bin/env python3
"""Generate src/Scripting/Binding/ScriptSurface.inc from the official @minecraft npm definitions.

usage: gen_script_surface.py <dir of extracted packages> <output .inc>

The directory holds one folder per package version, named like `npm pack` names the archive
(minecraft-server-2.11.0/package/index.d.ts). Every stable version and the beta of the targeted game
version must be present: the table records, for each name, the first and last version declaring it.
"""

import os
import re
import sys

MODULES = {
    'server': ('minecraft-server-', 'server'),
    'server-ui': ('minecraft-server-ui-', 'ui'),
    'common': ('minecraft-common-', 'common'),
    'server-graphics': ('minecraft-server-graphics-', 'graphics'),
    'server-admin': ('minecraft-server-admin-', 'admin'),
    'server-net': ('minecraft-server-net-', 'net'),
    'debug-utilities': ('minecraft-debug-utilities-', 'debug'),
    'server-gametest': ('minecraft-server-gametest-', 'gametest'),
}

# classes, enums and constants the scripting layer binds; anything else stays out of the table
BOUND = {
    'server': {
        'world', 'system', 'World', 'System', 'WorldAfterEvents', 'WorldBeforeEvents', 'SystemAfterEvents',
        'SystemBeforeEvents', 'Entity', 'Player', 'Block', 'Dimension', 'ItemStack', 'BlockPermutation',
        'BlockType', 'BlockTypes', 'BlockStates', 'BlockStateType', 'ItemType', 'ItemTypes', 'Container',
        'ContainerSlot', 'Component', 'EntityComponent', 'EntityInventoryComponent', 'EntityEquippableComponent',
        'EntityAttributeComponent', 'EntityHealthComponent', 'EntityTypeFamilyComponent',
        'EntityProjectileComponent', 'ItemComponent', 'ItemDurabilityComponent', 'ItemEnchantableComponent',
        'ItemCooldownComponent', 'ScreenDisplay', 'Camera', 'PlayerInputPermissions', 'LocatorBar', 'Scoreboard',
        'ScoreboardObjective', 'ScoreboardIdentity', 'ScoreboardScoreInfo',
        'ScoreboardIdentityType', 'DisplaySlotId', 'ObjectiveSortOrder', 'TickingAreaManager', 'StartupEvent', 'ShutdownEvent', 'BlockComponentRegistry',
        'ItemComponentRegistry', 'CommandResult', 'BlockVolume', 'EntityWaypoint', 'EquipmentSlot', 'GameMode',
        'InputPermissionCategory', 'Direction', 'PlayerInventoryType', 'EntityComponentTypes',
        'ItemComponentTypes', 'BlockComponentTypes', 'BlockEvent', 'GameRules', 'SystemInfo', 'Difficulty',
        'TimeOfDay', 'MoonPhase', 'WeatherType', 'ScriptEventSource', 'GameRule', 'MemoryTier',
        'CustomComponentParameters', 'CustomComponentNameErrorReason', 'NamespaceNameErrorReason',
        'ItemUseOnEvent', 'ItemCompleteUseEvent', 'WorldClock', 'TimeMarker', 'WorldClockRegistry',
        'DimensionRegistry', 'Effect',
        'EffectType', 'EffectTypes', 'EntityType', 'EntityTypes', 'EntityDamageCause', 'EntityInitializationCause',
        'EntityHealCause', 'InputButton', 'ButtonState', 'InputMode', 'InputInfo', 'EntitySwingSource',
        'ContainerAccessSource', 'CommandPermissionLevel', 'PlayerPermissionLevel', 'TickingArea',
        'TickingAreaError', 'TickingAreaErrorReason', 'EntityComponent', 'EntityBaseMovementComponent',
        'EntityNavigationComponent', 'EntityRideableComponent', 'EntityRidingComponent', 'PaletteColor',
        'EntityHealSource', 'BlockVolumeBase', 'ListBlockVolume', 'BlockLocationIterator', 'BlockComponent',
        'BlockInventoryComponent', 'BlockSignComponent', 'BlockPistonComponent', 'BlockFluidContainerComponent',
        'BlockRedstoneProducerComponent', 'BlockMapColorComponent', 'BlockMovableComponent',
        'BlockPrecipitationInteractionsComponent', 'BlockRecordPlayerComponent', 'BlockDynamicPropertiesComponent', 'BiomeType', 'BiomeTypes',
        'Structure', 'StructureManager', 'BlockPistonState', 'BlockVolumeIntersection', 'FluidType', 'LiquidType',
        'SignSide', 'StructureAnimationMode', 'StructureMirrorAxis', 'StructureRotation', 'StructureSaveMode',
        'DyeColor', 'TintMethod', 'MovementType', 'StickyType', 'CloneMode',
        'EntityHealSource', 'ItemBookComponent', 'ItemCompostableComponent', 'ItemDyeableComponent',
        'ItemFoodComponent', 'ItemInventoryComponent', 'ItemPotionComponent', 'PlayerCursorInventoryComponent',
        'EnchantmentType', 'EnchantmentTypes', 'EnchantmentSlot', 'ItemLockMode', 'PotionEffectType',
        'PotionDeliveryType', 'Potions', 'BookErrorReason', 'ContainerRulesErrorReason',
        'ItemBlockDynamicPropertiesComponent', 'CustomCommandRegistry', 'CustomCommandOrigin', 'CustomCommandSource',
        'CustomCommandParamType', 'CustomCommandStatus', 'CustomCommandErrorReason',
        'HudElement', 'HudVisibility', 'HudElementsCount', 'HudVisibilityCount',
        'EasingType', 'CameraShakeType', 'EntityAttachPoint',
        'Waypoint', 'LocationWaypoint', 'PlayerWaypoint', 'WaypointTexture', 'LocatorBarErrorReason',
        'LootTableManager', 'LootTable', 'LootPool', 'LootPoolEntry', 'LootPoolTiers', 'LootItem',
        'LootItemCondition', 'LootItemFunction', 'LootTableEntry', 'LootTableReference', 'EmptyLootItem',
        'EnchantInfo', 'DamagedByEntityCondition', 'EnchantRandomEquipmentFunction', 'EnchantRandomlyFunction',
        'EnchantWithLevelsFunction', 'EntityHasMarkVariantCondition', 'EntityHasVariantCondition',
        'EntityKilledCondition', 'ExplorationMapFunction', 'ExplosionDecayFunction', 'FillContainerFunction',
        'IsBabyCondition', 'KilledByEntityCondition', 'KilledByPlayerCondition', 'KilledByPlayerOrPetsCondition',
        'LootingEnchantFunction', 'MatchToolCondition', 'PassengerOfEntityCondition', 'RandomAuxValueFunction',
        'RandomBlockStateFunction', 'RandomChanceCondition', 'RandomChanceWithLootingCondition',
        'RandomDifficultyChanceCondition', 'RandomDyeFunction', 'RandomRegionalDifficultyChanceCondition',
        'SetArmorTrimFunction', 'SetBannerDetailsFunction', 'SetBookContentsFunction',
        'SetDataFromColorIndexFunction', 'SetItemCountFunction', 'SetItemDamageFunction', 'SetItemDataFunction',
        'SetItemLoreFunction', 'SetItemNameFunction', 'SetOminousBottleFunction', 'SetPotionFunction',
        'SetSpawnEggFunction', 'SetStewEffectFunction', 'SmeltItemFunction', 'SpecificEnchantFunction',
        'SoundInstance', 'SoundDurationInfo', 'SoundDefinition', 'SoundDefinitionRegistry', 'BlockInstrumentComponent',
        'MolangVariableMap', 'TicksPerDay', 'TicksPerSecond', 'MoonPhaseCount', 'FluidContainer', 'HeldItemOption',
        'LiquidSettings', 'PlayerSplitScreenSlot', 'PlatformType', 'GraphicsMode', 'ControlScheme', 'AimAssistTargetMode',
        'Trigger', 'DimensionType',
        'DimensionTypes', 'MinecraftDimensionTypes', 'ISerializable', 'BlockCustomComponentInstance',
        'ItemCustomComponentInstance', 'ClientSystemInfo', 'Seat', 'FeedItem', 'FeedItemEffect',
        'EntityDefinitionFeedItem', 'CatmullRomSpline', 'LinearSpline', 'FogSettings',
        'FogSettingsError', 'PrimitiveShape', 'TextPrimitive', 'PrimitiveShapesManager',
        'PlayerWaypointsMode',
    },
    'server-ui': None,
    'common': None,
    'server-graphics': None,
    'server-admin': None,
    'server-net': None,
    'debug-utilities': None,
    'server-gametest': None,
}

# every signal of these classes is subscribable; their signal and event classes join the bound set
SIGNAL_OWNERS = ('WorldAfterEvents', 'WorldBeforeEvents', 'SystemAfterEvents', 'SystemBeforeEvents')


def version_key(text):
    match = re.match(r'(\d+)\.(\d+)\.(\d+)(.*)', text)
    return int(match.group(1)), int(match.group(2)), int(match.group(3)), match.group(4)


def strip_comments(source):
    return re.sub(r'/\*.*?\*/', '', source, flags=re.S)


def parse(path):
    raw = open(path, encoding='utf-8').read()
    exports = {}
    for match in re.finditer(r'\nexport (?:declare )?(class|enum|const|function) (\w+)', strip_comments(raw)):
        exports[match.group(2)] = match.group(1)

    classes = {}
    # BlockLocationIterator also implements Iterable<Vector3>
    for match in re.finditer(r'\nexport class (\w+)(?: extends ([\w.]+))?(?: implements [^{\n]+)? \{(.*?)\n\}', raw,
                             re.S):
        members = {}
        # a reserved word is quoted as a member name: readonly 'type': ItemType
        for member in re.finditer(r'(/\*\*(?:(?!\*/).)*\*/)?\s*\n {4}((?:readonly |static |private )*)'
                                  r"'?(\w+)'?\??(\(|:|<| =)([^;]*)", match.group(3), re.S):
            doc = member.group(1) or ''
            modifiers = member.group(2)
            if 'private' in modifiers:
                continue
            name = member.group(3)
            kind = 'method' if member.group(4) in '(<' else ('readonly' if 'readonly' in modifiers else 'prop')
            tags = set(re.findall(r'@privilege ([a-z-]+)', doc))
            if kind == 'method':
                early = 'early-execution-allowed' in tags or 'can be called in early-execution' in doc
                blocked = 'no-restricted-execution' in tags or "can't be called in read-only mode" in doc
            else:
                early = 'early-execution-readable' in tags or 'early-execution-editable' in tags
                blocked = 'restricted-execution-read-only' in tags
            if 'static ' in modifiers:
                name = 'static ' + name
            signature = member.group(5)
            previous = members.get(name)
            members[name] = (kind, early or (previous is not None and previous[1]),
                             blocked or (previous is not None and previous[2]), signature)
        # no constructor declared at all: the implicit public one (MolangVariableMap, the splines)
        if 'constructor(' not in match.group(3):
            members['constructor'] = ('method', False, False, '')
        classes[match.group(1)] = (match.group(2), members)

    # free functions carry their privileges in the doc right above them
    functions = {}
    for match in re.finditer(r'(/\*\*(?:(?!\*/).)*\*/)\s*\nexport function (\w+)', raw, re.S):
        tags = set(re.findall(r'@privilege ([a-z-]+)', match.group(1)))
        functions[match.group(2)] = ('early-execution-allowed' in tags, 'no-restricted-execution' in tags)

    enums = {}
    for match in re.finditer(r'\nexport enum (\w+) \{(.*?)\}', strip_comments(raw), re.S):
        values = []
        for line in match.group(2).split('\n'):
            line = line.strip().rstrip(',')
            if '=' in line:
                key, value = [part.strip() for part in line.split('=', 1)]
                # a reserved word is quoted as a key: void = 'void'
                values.append((key.strip("'"), value.strip("'")))
        enums[match.group(1)] = values
    return exports, classes, enums, functions


def collect(root, prefix):
    versions = []
    for entry in os.listdir(root):
        if not entry.startswith(prefix):
            continue
        version = entry[len(prefix):]
        if not re.match(r'\d+\.\d+\.\d+', version):
            continue
        path = os.path.join(root, entry, 'package', 'index.d.ts')
        if os.path.exists(path):
            versions.append((version, path))
    versions.sort(key=lambda item: version_key(item[0]))
    return versions


class Range:
    def __init__(self):
        self.first = {}
        self.last = {}
        self.beta = False
        self.early = False
        self.blocked = False

    def add(self, version):
        major, minor, _, suffix = version_key(version)
        if suffix:
            self.beta = True
            return
        self.first.setdefault(major, minor)
        self.last[major] = minor


def main():
    root, output = sys.argv[1], sys.argv[2]
    entries = {}
    signals = {}
    parents = {}

    def entry(key):
        if key not in entries:
            entries[key] = Range()
        return entries[key]

    bound_events = set()
    for version, path in collect(root, MODULES['server'][0]):
        _, classes, _, _ = parse(path)
        for owner in SIGNAL_OWNERS:
            if owner not in classes:
                continue
            for member, (_, _, _, signature) in classes[owner][1].items():
                signal = signature.strip().rstrip(';').split('.')[-1]
                bound_events.add(signal)
                subscribe = classes.get(signal, (None, {}))[1].get('subscribe')
                match = re.search(r'arg0: (\w+)\)\s*=>', subscribe[3]) if subscribe else None
                if match:
                    bound_events.add(match.group(1))

    for module, (prefix, short) in MODULES.items():
        bound = BOUND[module]
        for version, path in collect(root, prefix):
            exports, classes, enums, functions = parse(path)
            beta = bool(version_key(version)[3])
            for name, kind in exports.items():
                is_error = kind == 'class' and name.endswith('Error')
                # custom component hook payloads
                payload = name in bound_events or re.match(r'(Block|Item)Component\w+Event$', name) is not None
                component = name.startswith('Entity') and name.endswith('Component')
                if bound is not None and name not in bound and not is_error and not payload and not component:
                    continue
                entry('%s:%s' % (short, name)).add(version)
                if kind == 'function' and name in functions:
                    entries['%s:%s' % (short, name)].early, entries['%s:%s' % (short, name)].blocked = functions[name]
                if kind == 'class' and name in classes:
                    parent, members = classes[name]
                    if parent is not None and (beta or name not in parents):
                        parents[name] = parent.split('.')[-1]
                    for member, (member_kind, early, blocked, signature) in members.items():
                        item = entry('%s:%s.%s' % (short, name, member))
                        item.add(version)
                        # the newest declaration wins: versions arrive in ascending order
                        item.early = early
                        item.blocked = blocked
                        if name in SIGNAL_OWNERS:
                            signal = signature.strip().rstrip(';').split('.')[-1]
                            signals['%s.%s' % (name, member)] = signal
                if kind == 'enum':
                    for key, value in enums.get(name, []):
                        entry('%s:%s.%s=%s' % (short, name, key, value)).add(version)

    payloads = {}
    for module, (prefix, short) in MODULES.items():
        if short != 'server':
            continue
        for version, path in collect(root, prefix):
            _, classes, _, _ = parse(path)
            for name, (_, members) in classes.items():
                subscribe = members.get('subscribe')
                if name.endswith('Signal') and subscribe is not None:
                    match = re.search(r'arg0: (\w+)\)\s*=>', subscribe[3])
                    if match:
                        payloads[name] = match.group(1)

    def bound_minor(range_, major, which):
        table = range_.first if which == 'first' else range_.last
        return table.get(major, 0xff)

    with open(output, 'w', encoding='utf-8') as out:
        out.write('// generated by tools/gen_script_surface.py from the @minecraft npm definitions; do not edit\n')
        out.write('// name, first/last 1.x minor, first/last 2.x minor (255: absent), beta, early, restricted-blocked\n')
        out.write('static const ScriptSurfaceEntry SCRIPT_SURFACE[] = {\n')
        for key in sorted(entries):
            item = entries[key]
            out.write('        {"%s", %d, %d, %d, %d, %s, %s, %s},\n' % (
                key.replace('\\', '\\\\').replace('"', '\\"'), bound_minor(item, 1, 'first'),
                bound_minor(item, 1, 'last'), bound_minor(item, 2, 'first'), bound_minor(item, 2, 'last'),
                'true' if item.beta else 'false', 'true' if item.early else 'false',
                'true' if item.blocked else 'false'))
        out.write('};\n\n')
        out.write('// signal property, its signal class and the class of the event it fires\n')
        out.write('static const ScriptSignalEntry SCRIPT_SIGNALS[] = {\n')
        for key in sorted(signals):
            out.write('        {"%s", "%s", "%s"},\n' % (key, signals[key], payloads.get(signals[key], '')))
        out.write('};\n\n')
        out.write('static const ScriptParentEntry SCRIPT_PARENTS[] = {\n')
        for name in sorted(parents):
            out.write('        {"%s", "%s"},\n' % (name, parents[name]))
        out.write('};\n')


if __name__ == '__main__':
    main()
