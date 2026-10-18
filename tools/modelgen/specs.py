"""Per-slot specification table for every P0 model in assets/manifest.json.

Each entry carries the source citations that pin its scale/origin/axis so
reviewers can re-derive the numbers without opening an original model.
Sources (see docs/ASSET-CONTRACT.md sections 4.2, 5, 6, 7 for the full
derivation):

- Humans:  object.cpp:2154-2205 (part transforms), §5.3 derived limits.
- Weapons: parameter.cpp (per-weapon size/grip), §6.1-6.2 derived placement.
- Bullet:  parameter.cpp:1434-1436, object.cpp:2897 (nose toward -Z, size 1.0).
- Sky:     resource.cpp:748-764, gamemain.cpp:2769-2772 (scale 2.0, centred).

All part meshes are centred on the Y axis (x=z=0) and face -Z, per §5.3/§4.2.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Callable, Dict, List, Optional


@dataclass
class ModelSpec:
    slot: str  # path relative to assets/pack/data/, e.g. "model/up0.x"
    kind: str  # "up" | "arm" | "leg" | "walk" | "run" | "weapon" | "bullet" | "sky"
    priority: str
    summary: str
    generator: str  # key into generators.GENERATORS
    gen_args: dict = field(default_factory=dict)
    morph_group: Optional[str] = None  # walk/run morph-partner group id
    source_refs: List[str] = field(default_factory=list)


# -- Human upper-body variants (up0..up5): same topology, slightly different
#    proportions/accessories so each looks distinct even bare (no skin yet).
#    object.cpp:2154-2159: translate pos+(0,-1,0), scale 9.4, derived file-y
#    limits in §5.3 (hip 1.223, shoulder 1.809, eye 2.128, head top 2.277,
#    torso radius <= 0.255, head radius <= 0.16).
_UP_REFS = ["src/object.cpp:2154-2159,178", "docs/ASSET-CONTRACT.md#5.3"]

UP_VARIANTS = {
    "up0": dict(backpack=False, cap=False, label="plain torso+head"),
    "up1": dict(backpack=False, cap=False, label="civilian variant (shirt proportions)"),
    "up2": dict(backpack=False, cap=True, label="police cap"),
    "up3": dict(backpack=False, cap=False, label="woman/zombie slimmer torso"),
    "up4": dict(backpack=True, cap=False, label="soldier backpack"),
    "up5": dict(backpack=True, cap=True, label="special forces backpack+helmet"),
}

ARM_VARIANTS = {
    # object.cpp:3431-3461: WeaponP 2 (no weapon/case) -> arm00, WeaponP 0
    # (2-handed) -> arm01, WeaponP 1 (1-handed) -> arm02. Pivot at origin =
    # shoulder line (§5.3); pose angle differs per grip style.
    "arm00": dict(pose="down", label="arms down / case"),
    "arm01": dict(pose="twohand", label="two-handed grip"),
    "arm02": dict(pose="onehand", label="one-handed grip"),
}

# leg.x / walk01-04.x / walk morphs (0,2,4,6 real; 1,3,5,7 morphed) /
# run01-06.x / run morphs, resource.cpp:152-175. Scale 9.0, translate
# pos+(0,-0.2,0) (object.cpp:2168-2173,180). Stride phase in [0,1) selects a
# forward leg-swing pose; leg.x is the stride=0 "standing" pose.
_LEG_REFS = ["src/resource.cpp:152-175", "src/object.cpp:2168-2173,180", "docs/ASSET-CONTRACT.md#5.1"]

WALK_PHASES = {  # key frames only; odd slots are engine-computed morphs
    "walk01": 0.00, "walk02": 0.25, "walk03": 0.50, "walk04": 0.75,
}
RUN_PHASES = {
    "run01": 0.00, "run02": 1 / 6, "run03": 2 / 6,
    "run04": 3 / 6, "run05": 4 / 6, "run06": 5 / 6,
}

# Weapons: parameter.cpp size + grip m=(mx,my,mz) per §6.2, muzzle/eject
# derived in §6.1. origin sits at the grip (contract assumption). All P0
# weapon slots from assets/manifest.json:
WEAPON_SPECS = {
    "de": dict(name="DESERT EAGLE", size=11, grip=(2, 11, 59), muzzle=(0.0, 0.027, -0.145),
               kind="pistol", source="parameter.cpp:676-710"),
    "grenade": dict(name="GRENADE", size=8, grip=(1, 6, 57), muzzle=(0.0, 0.0, 0.0),
                     kind="grenade", source="parameter.cpp:956-990"),
    "m4": dict(name="M4", size=6.8, grip=(9, 6, 37), muzzle=(0.0, 0.103, -0.794),
               kind="rifle", source="parameter.cpp:816-850"),
    "m92f": dict(name="M92F", size=7, grip=(2, 7, 57), muzzle=(0.0, 0.114, -0.229),
                 kind="pistol", source="parameter.cpp:606-640"),
    "mac10": dict(name="MAC10", size=5.5, grip=(2, 4, 54), muzzle=(0.0, 0.2, -0.345),
                  kind="smg", source="parameter.cpp:711-745"),
}

# Bullet: parameter.cpp:1434-1436, object.cpp:2897 (nose toward file -Z, size 1.0)
BULLET_REF = ["src/parameter.cpp:1434-1436", "src/object.cpp:2897"]

# Sky: resource.cpp:748-764 (one model, 5 skies share it), scale 2.0, centred
# on camera (gamemain.cpp:2769-2772), visible from inside -> faces wound
# clockwise as seen from the origin (§7).
SKY_REF = ["src/resource.cpp:748-764", "src/gamemain.cpp:2769-2772", "docs/ASSET-CONTRACT.md#7"]


def build_specs() -> Dict[str, ModelSpec]:
    specs: Dict[str, ModelSpec] = {}

    for key, v in UP_VARIANTS.items():
        specs[key] = ModelSpec(
            slot=f"model/{key}.x", kind="up", priority="P0",
            summary=f"Upper body mesh ({v['label']})",
            generator="human_upper", gen_args=v, source_refs=_UP_REFS,
        )

    for key, v in ARM_VARIANTS.items():
        specs[key] = ModelSpec(
            slot=f"model/{key}.x", kind="arm", priority="P0",
            summary=f"Arms mesh ({v['label']})",
            generator="human_arms", gen_args=v,
            source_refs=["src/resource.cpp:149-151", "src/object.cpp:3431-3461", "docs/ASSET-CONTRACT.md#5.3"],
        )

    specs["leg"] = ModelSpec(
        slot="model/leg.x", kind="leg", priority="P0", summary="Legs mesh, standing still",
        generator="human_legs", gen_args=dict(phase=0.0, closed=True), source_refs=_LEG_REFS,
    )
    for key, phase in WALK_PHASES.items():
        specs[key] = ModelSpec(
            slot=f"model/{key}.x", kind="walk", priority="P0",
            summary=f"Legs mesh, walking cycle keyframe (phase {phase:.2f})",
            generator="human_legs", gen_args=dict(phase=phase, closed=True),
            morph_group="walk", source_refs=_LEG_REFS,
        )
    for key, phase in RUN_PHASES.items():
        specs[key] = ModelSpec(
            slot=f"model/{key}.x", kind="run", priority="P0",
            summary=f"Legs mesh, running cycle keyframe (phase {phase:.2f})",
            generator="human_legs", gen_args=dict(phase=phase, closed=True, stride=1.6),
            morph_group="run", source_refs=_LEG_REFS,
        )

    for key, w in WEAPON_SPECS.items():
        specs[key] = ModelSpec(
            slot=f"model/weapon/{key}.x", kind="weapon", priority="P0",
            summary=f"Weapon model: {w['name']}",
            generator="weapon", gen_args=w, source_refs=["src/" + w["source"], "docs/ASSET-CONTRACT.md#6"],
        )

    specs["bullet"] = ModelSpec(
        slot="model/bullet.x", kind="bullet", priority="P0",
        summary="Bullet object model, nose toward -Z",
        generator="bullet", gen_args={}, source_refs=BULLET_REF,
    )

    specs["sky"] = ModelSpec(
        slot="sky/sky.x", kind="sky", priority="P0",
        summary="Sky box mesh, visible from inside",
        generator="sky", gen_args={}, source_refs=SKY_REF,
    )

    return specs


SPECS: Dict[str, ModelSpec] = build_specs()

P0_KEYS = [k for k, s in SPECS.items() if s.priority == "P0"]
