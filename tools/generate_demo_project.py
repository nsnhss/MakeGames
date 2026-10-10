#!/usr/bin/env python3
"""Generate the 魔塔村物语 demo project under <repo>/demo_project/."""

import json
import os
from pathlib import Path
from PIL import Image

REPO = Path(__file__).resolve().parent.parent
PROJECT = REPO / "demo_project"

ASSETS = PROJECT / "assets"
TILESETS = ASSETS / "tilesets"
SPRITES = ASSETS / "sprites"
AUDIO = ASSETS / "audio" / "bgm"
SCRIPTS = ASSETS / "scripts"
DATA = PROJECT / "data"
MAPS = PROJECT / "maps"


def ensure_dirs():
    for d in [TILESETS, SPRITES, AUDIO, SCRIPTS, DATA, MAPS]:
        d.mkdir(parents=True, exist_ok=True)


def save_png(path: Path, color, size=(32, 32)):
    img = Image.new("RGBA", size, color)
    img.save(path, "PNG")


def generate_tileset():
    """256x128 tileset, 8x4 tiles of 32x32."""
    cols, rows = 8, 4
    tw, th = 32, 32
    w, h = cols * tw, rows * th
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))

    palette = [
        (0x22, 0x8B, 0x22, 0xFF),  # 0  grass
        (0x22, 0x8B, 0x22, 0xFF),  # 1  grass variant
        (0x8B, 0x69, 0x14, 0xFF),  # 2  dirt road
        (0x8B, 0x69, 0x14, 0xFF),  # 3  dirt road variant
        (0x80, 0x80, 0x80, 0xFF),  # 4  stone wall
        (0x80, 0x80, 0x80, 0xFF),  # 5  stone wall variant
        (0x41, 0x69, 0xE1, 0xFF),  # 6  water
        (0x41, 0x69, 0xE1, 0xFF),  # 7  water variant
        (0x22, 0x8B, 0x22, 0xFF),  # 8  grass
        (0x22, 0x8B, 0x22, 0xFF),  # 9  grass
        (0x8B, 0x45, 0x13, 0xFF),  # 10 house wall
        (0x8B, 0x45, 0x13, 0xFF),  # 11 house wall
        (0x70, 0x40, 0x10, 0xFF),  # 12 roof
        (0x70, 0x40, 0x10, 0xFF),  # 13 roof
        (0x55, 0x55, 0x55, 0xFF),  # 14 door
        (0x55, 0x55, 0x55, 0xFF),  # 15 door
        (0x22, 0x8B, 0x22, 0xFF),  # 16
        (0x22, 0x8B, 0x22, 0xFF),  # 17
        (0x8B, 0x69, 0x14, 0xFF),  # 18
        (0x8B, 0x69, 0x14, 0xFF),  # 19
        (0x80, 0x80, 0x80, 0xFF),  # 20
        (0x80, 0x80, 0x80, 0xFF),  # 21
        (0x41, 0x69, 0xE1, 0xFF),  # 22
        (0x41, 0x69, 0xE1, 0xFF),  # 23
        (0x22, 0x8B, 0x22, 0xFF),  # 24
        (0x22, 0x8B, 0x22, 0xFF),  # 25
        (0x8B, 0x45, 0x13, 0xFF),  # 26
        (0x8B, 0x45, 0x13, 0xFF),  # 27
        (0x70, 0x40, 0x10, 0xFF),  # 28
        (0x70, 0x40, 0x10, 0xFF),  # 29
        (0x55, 0x55, 0x55, 0xFF),  # 30
        (0x55, 0x55, 0x55, 0xFF),  # 31
    ]

    for i, color in enumerate(palette):
        x = (i % cols) * tw
        y = (i // cols) * th
        img.paste(color, (x, y, x + tw, y + th))

    img.save(TILESETS / "Overworld.png", "PNG")


def generate_sprites():
    save_png(SPRITES / "Player.png", (0x41, 0x69, 0xE1, 0xFF))     # blue
    save_png(SPRITES / "NPC_Elder.png", (0xB2, 0x22, 0x22, 0xFF))  # red
    save_png(SPRITES / "Chest.png", (0xFF, 0xD7, 0x00, 0xFF))      # gold
    save_png(SPRITES / "Slime.png", (0x22, 0x8B, 0x22, 0xFF))      # green
    save_png(SPRITES / "Boss.png", (0x80, 0x00, 0x80, 0xFF))       # purple


def write_json(path: Path, data):
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=4)


def generate_project_json():
    write_json(PROJECT / "project.json", {
        "name": "魔塔村物语",
        "engine_version": "0.1.0",
        "mode": "2d_topdown",
        "start_scene": "maps/Map001.json",
        "tile_size": 32,
        "display": {
            "width": 1280,
            "height": 720,
            "fullscreen": False,
            "vsync": True,
            "pixel_perfect": True
        }
    })


def generate_tilesets_json():
    write_json(DATA / "tilesets.json", [
        {
            "name": "Overworld",
            "image_path": "assets/tilesets/Overworld.png",
            "tile_width": 32,
            "tile_height": 32,
            "columns": 8,
            "rows": 4,
            "auto_tile": False
        }
    ])


def generate_data():
    write_json(DATA / "items.json", [
        {"id": "potion", "name": "生命药水", "description": "恢复 30 HP", "price": 50},
        {"id": "bomb", "name": "炸弹", "description": "造成范围伤害", "price": 80}
    ])
    write_json(DATA / "actors.json", [
        {"id": "player", "name": "勇者", "hp": 100, "mp": 30, "atk": 15, "def": 8, "sprite": "assets/sprites/Player.png"},
        {"id": "elder", "name": "村长", "hp": 999, "mp": 0, "atk": 1, "def": 99, "sprite": "assets/sprites/NPC_Elder.png"}
    ])
    write_json(DATA / "skills.json", [
        {"id": "fireball", "name": "火球术", "mp_cost": 10, "damage": 30, "description": "发射火球"},
        {"id": "heal", "name": "治愈", "mp_cost": 8, "damage": -20, "description": "恢复生命"}
    ])
    write_json(DATA / "enemies.json", [
        {"id": "slime", "name": "史莱姆", "hp": 30, "atk": 5, "def": 2, "gold": 10, "exp": 5, "sprite": "assets/sprites/Slime.png"},
        {"id": "boss", "name": "魔塔守卫", "hp": 80, "atk": 12, "def": 8, "gold": 50, "exp": 20, "sprite": "assets/sprites/Boss.png"}
    ])
    write_json(DATA / "shops.json", [
        {"id": "village_shop", "items": [
            {"item_id": "potion", "price": 50},
            {"item_id": "bomb", "price": 80}
        ]}
    ])


def make_layer(w, h, fill=-1):
    return [[fill for _ in range(w)] for _ in range(h)]


def generate_map001():
    w, h = 20, 15
    ground = make_layer(w, h, 0)
    # dirt road cross
    for x in range(w):
        ground[7][x] = 2
    for y in range(h):
        ground[y][10] = 2

    buildings = make_layer(w, h, -1)
    # 3 houses
    houses = [(2, 2, 4, 3), (12, 2, 4, 3), (14, 8, 4, 3)]
    for hx, hy, hw, hh in houses:
        for y in range(hy, hy + hh):
            for x in range(hx, hx + hw):
                buildings[y][x] = 10
        # roof
        for y in range(hy, hy + hh - 1):
            for x in range(hx, hx + hw):
                if y == hy:
                    buildings[y][x] = 12
        # door
        buildings[hy + hh - 1][hx + hw // 2] = 14

    collision = make_layer(w, h, -1)
    for hx, hy, hw, hh in houses:
        for y in range(hy, hy + hh):
            for x in range(hx, hx + hw):
                if buildings[y][x] in (10, 12):
                    collision[y][x] = 1

    scene = {
        "name": "魔塔村",
        "width": w,
        "height": h,
        "tile_size": 32,
        "mode": "topdown",
        "bgm": "",
        "background_color": [0.07, 0.07, 0.08, 1.0],
        "entities": [
            # Tilemap
            {
                "id": 1,
                "components": {
                    "Name": {"value": "Tilemap"},
                    "Transform": {"position": [0, 0], "rotation": 0, "scale": [1, 1], "sorting_order": 0},
                    "Tilemap": {
                        "tileset_name": "Overworld",
                        "tile_width": 32,
                        "tile_height": 32,
                        "layers": [
                            {"name": "Ground", "width": w, "height": h, "tiles": [t for row in ground for t in row], "visible": True, "collision": False, "edge_thickness": 1.0},
                            {"name": "Buildings", "width": w, "height": h, "tiles": [t for row in buildings for t in row], "visible": True, "collision": False, "edge_thickness": 1.0},
                            {"name": "Collision", "width": w, "height": h, "tiles": [t for row in collision for t in row], "visible": False, "collision": True, "edge_thickness": 1.0}
                        ]
                    }
                }
            },
            # Camera
            {
                "id": 2,
                "components": {
                    "Name": {"value": "Camera"},
                    "Transform": {"position": [320, 240], "rotation": 0, "scale": [1, 1], "sorting_order": 0},
                    "Camera": {
                        "projection": "orthographic",
                        "ortho_size": 240,
                        "near_plane": -100,
                        "far_plane": 100,
                        "clear_color": [0.07, 0.07, 0.08, 1.0],
                        "viewport": [0, 0, 1, 1],
                        "is_primary": True,
                        "follow_target": 0,
                        "smooth_follow": True,
                        "follow_speed": 5,
                        "dead_zone": [0, 0]
                    }
                }
            },
            # Player
            {
                "id": 3,
                "components": {
                    "Name": {"value": "Player"},
                    "Transform": {"position": [144, 176], "rotation": 0, "scale": [1, 1], "sorting_order": 0},  # 门口正下方空地；(160,120) 会嵌进建筑碰撞体卡死
                    "Movement": {},
                    "CameraFollowTarget": {},
                    "Sprite": {"texture_path": "assets/sprites/Player.png", "pivot": [0, 0], "tint": [1, 1, 1, 1], "flip_x": False, "flip_y": False, "anim_frame": 0}
                }
            },
            # 村长 Elder
            {
                "id": 4,
                "components": {
                    "Name": {"value": "村长"},
                    "Transform": {"position": [5 * 32, 4 * 32], "rotation": 0, "scale": [1, 1], "sorting_order": 0},
                    "Sprite": {"texture_path": "assets/sprites/NPC_Elder.png", "pivot": [0, 0], "tint": [1, 1, 1, 1], "flip_x": False, "flip_y": False, "anim_frame": 0},
                    "EventTrigger": {
                        "pages": [
                            {
                                "trigger": "OnInteract",
                                "run_once": False,
                                "condition": {"type": "None", "name": "", "compare_op": 0, "int_value": 0, "bool_value": False},
                                "actions": [
                                    {"cmd": "ShowMessage", "text": "欢迎来到魔塔村！", "target": "elder"},
                                    {"cmd": "ShowChoices", "text": "打听魔塔 / 再见"},
                                    {"cmd": "If", "int_value": 1},
                                    {"cmd": "ShowMessage", "text": "魔塔在北方野外，小心史莱姆。"},
                                    {"cmd": "EndIf"},
                                    {"cmd": "If", "int_value": 2},
                                    {"cmd": "ShowMessage", "text": "旅途愉快！"},
                                    {"cmd": "EndIf"},
                                    {"cmd": "CallScript", "text": "assets/scripts/test.lua"}
                                ]
                            }
                        ],
                        "self_switch_a": False,
                        "self_switch_b": False,
                        "self_switch_c": False,
                        "self_switch_d": False
                    }
                }
            },
            # 宝箱 Chest
            {
                "id": 5,
                "components": {
                    "Name": {"value": "宝箱"},
                    "Transform": {"position": [10 * 32, 6 * 32], "rotation": 0, "scale": [1, 1], "sorting_order": 0},
                    "Sprite": {"texture_path": "assets/sprites/Chest.png", "pivot": [0, 0], "tint": [1, 1, 1, 1], "flip_x": False, "flip_y": False, "anim_frame": 0},
                    "EventTrigger": {
                        "pages": [
                            {
                                "trigger": "OnInteract",
                                "run_once": True,
                                "condition": {"type": "None", "name": "", "compare_op": 0, "int_value": 0, "bool_value": False},
                                "actions": [
                                    {"cmd": "ShowToast", "text": "获得了生命药水！"},
                                    {"cmd": "AddItem", "target": "potion", "int_value": 1},
                                    {"cmd": "SetVariable", "target": "gold", "int_value": 50}
                                ]
                            }
                        ],
                        "self_switch_a": False,
                        "self_switch_b": False,
                        "self_switch_c": False,
                        "self_switch_d": False
                    }
                }
            },
            # 商人 Merchant
            {
                "id": 6,
                "components": {
                    "Name": {"value": "商人"},
                    "Transform": {"position": [15 * 32, 5 * 32], "rotation": 0, "scale": [1, 1], "sorting_order": 0},
                    "Sprite": {"texture_path": "assets/sprites/NPC_Elder.png", "pivot": [0, 0], "tint": [1, 1, 1, 1], "flip_x": False, "flip_y": False, "anim_frame": 0},
                    "EventTrigger": {
                        "pages": [
                            {
                                "trigger": "OnInteract",
                                "run_once": False,
                                "condition": {"type": "None", "name": "", "compare_op": 0, "int_value": 0, "bool_value": False},
                                "actions": [
                                    {"cmd": "ShopProcessing", "target": "village_shop"}
                                ]
                            }
                        ],
                        "self_switch_a": False,
                        "self_switch_b": False,
                        "self_switch_c": False,
                        "self_switch_d": False
                    }
                }
            },
            # 传送门 Portal to Map002
            {
                "id": 7,
                "components": {
                    "Name": {"value": "传送门"},
                    "Transform": {"position": [19 * 32, 12 * 32], "rotation": 0, "scale": [1, 1], "sorting_order": 0},
                    "Sprite": {"texture_path": "assets/sprites/Chest.png", "pivot": [0, 0], "tint": [1, 1, 1, 1], "flip_x": False, "flip_y": False, "anim_frame": 0},
                    "EventTrigger": {
                        "pages": [
                            {
                                "trigger": "OnTouch",
                                "run_once": False,
                                "condition": {"type": "None", "name": "", "compare_op": 0, "int_value": 0, "bool_value": False},
                                "actions": [
                                    {"cmd": "TransferPlayer", "target": "maps/Map002.json", "int_value": 5, "float_value": 5}
                                ]
                            }
                        ],
                        "self_switch_a": False,
                        "self_switch_b": False,
                        "self_switch_c": False,
                        "self_switch_d": False
                    }
                }
            }
        ]
    }
    write_json(MAPS / "Map001.json", scene)


def generate_map002():
    w, h = 25, 20
    ground = make_layer(w, h, 0)
    for x in range(w):
        ground[10][x] = 2
    for y in range(h):
        ground[y][5] = 2

    trees = make_layer(w, h, -1)
    # tree border at top and right
    for x in range(w):
        trees[0][x] = 4
        trees[1][x] = 4
    for y in range(h):
        trees[y][w - 1] = 4
        trees[y][w - 2] = 4

    collision = make_layer(w, h, -1)
    for y in range(h):
        for x in range(w):
            if trees[y][x] == 4:
                collision[y][x] = 1

    scene = {
        "name": "野外",
        "width": w,
        "height": h,
        "tile_size": 32,
        "mode": "topdown",
        "bgm": "",
        "background_color": [0.07, 0.07, 0.08, 1.0],
        "entities": [
            {
                "id": 1,
                "components": {
                    "Name": {"value": "Tilemap"},
                    "Transform": {"position": [0, 0], "rotation": 0, "scale": [1, 1], "sorting_order": 0},
                    "Tilemap": {
                        "tileset_name": "Overworld",
                        "tile_width": 32,
                        "tile_height": 32,
                        "layers": [
                            {"name": "Ground", "width": w, "height": h, "tiles": [t for row in ground for t in row], "visible": True, "collision": False, "edge_thickness": 1.0},
                            {"name": "Trees", "width": w, "height": h, "tiles": [t for row in trees for t in row], "visible": True, "collision": False, "edge_thickness": 1.0},
                            {"name": "Collision", "width": w, "height": h, "tiles": [t for row in collision for t in row], "visible": False, "collision": True, "edge_thickness": 1.0}
                        ]
                    }
                }
            },
            {
                "id": 2,
                "components": {
                    "Name": {"value": "Camera"},
                    "Transform": {"position": [400, 320], "rotation": 0, "scale": [1, 1], "sorting_order": 0},
                    "Camera": {
                        "projection": "orthographic",
                        "ortho_size": 240,
                        "near_plane": -100,
                        "far_plane": 100,
                        "clear_color": [0.07, 0.07, 0.08, 1.0],
                        "viewport": [0, 0, 1, 1],
                        "is_primary": True,
                        "follow_target": 0,
                        "smooth_follow": True,
                        "follow_speed": 5,
                        "dead_zone": [0, 0]
                    }
                }
            },
            {
                "id": 3,
                "components": {
                    "Name": {"value": "Player"},
                    "Transform": {"position": [5 * 32, 5 * 32], "rotation": 0, "scale": [1, 1], "sorting_order": 0},
                    "Movement": {},
                    "CameraFollowTarget": {},
                    "Sprite": {"texture_path": "assets/sprites/Player.png", "pivot": [0, 0], "tint": [1, 1, 1, 1], "flip_x": False, "flip_y": False, "anim_frame": 0}
                }
            },
            # Slime x2
            {
                "id": 4,
                "components": {
                    "Name": {"value": "史莱姆"},
                    "Transform": {"position": [12 * 32, 9 * 32], "rotation": 0, "scale": [1, 1], "sorting_order": 0},
                    "Sprite": {"texture_path": "assets/sprites/Slime.png", "pivot": [0, 0], "tint": [1, 1, 1, 1], "flip_x": False, "flip_y": False, "anim_frame": 0},
                    "EventTrigger": {
                        "pages": [
                            {
                                "trigger": "OnTouch",
                                "run_once": False,
                                "condition": {"type": "None", "name": "", "compare_op": 0, "int_value": 0, "bool_value": False},
                                "actions": [
                                    {"cmd": "BattleProcessing", "target": "slime", "int_value": 2, "param": "player"}
                                ]
                            }
                        ],
                        "self_switch_a": False,
                        "self_switch_b": False,
                        "self_switch_c": False,
                        "self_switch_d": False
                    }
                }
            },
            {
                "id": 5,
                "components": {
                    "Name": {"value": "史莱姆"},
                    "Transform": {"position": [14 * 32, 9 * 32], "rotation": 0, "scale": [1, 1], "sorting_order": 0},
                    "Sprite": {"texture_path": "assets/sprites/Slime.png", "pivot": [0, 0], "tint": [1, 1, 1, 1], "flip_x": False, "flip_y": False, "anim_frame": 0},
                    "EventTrigger": {
                        "pages": [
                            {
                                "trigger": "OnTouch",
                                "run_once": False,
                                "condition": {"type": "None", "name": "", "compare_op": 0, "int_value": 0, "bool_value": False},
                                "actions": [
                                    {"cmd": "BattleProcessing", "target": "slime", "int_value": 2, "param": "player"}
                                ]
                            }
                        ],
                        "self_switch_a": False,
                        "self_switch_b": False,
                        "self_switch_c": False,
                        "self_switch_d": False
                    }
                }
            },
            # Boss
            {
                "id": 6,
                "components": {
                    "Name": {"value": "魔塔守卫"},
                    "Transform": {"position": [18 * 32, 12 * 32], "rotation": 0, "scale": [1, 1], "sorting_order": 0},
                    "Sprite": {"texture_path": "assets/sprites/Boss.png", "pivot": [0, 0], "tint": [1, 1, 1, 1], "flip_x": False, "flip_y": False, "anim_frame": 0},
                    "EventTrigger": {
                        "pages": [
                            {
                                "trigger": "OnTouch",
                                "run_once": False,
                                "condition": {"type": "None", "name": "", "compare_op": 0, "int_value": 0, "bool_value": False},
                                "actions": [
                                    {"cmd": "BattleProcessing", "target": "boss", "int_value": 1, "param": "player"}
                                ]
                            }
                        ],
                        "self_switch_a": False,
                        "self_switch_b": False,
                        "self_switch_c": False,
                        "self_switch_d": False
                    }
                }
            },
            # Return signpost
            {
                "id": 7,
                "components": {
                    "Name": {"value": "归途路标"},
                    "Transform": {"position": [1 * 32, 1 * 32], "rotation": 0, "scale": [1, 1], "sorting_order": 0},
                    "Sprite": {"texture_path": "assets/sprites/Chest.png", "pivot": [0, 0], "tint": [1, 1, 1, 1], "flip_x": False, "flip_y": False, "anim_frame": 0},
                    "EventTrigger": {
                        "pages": [
                            {
                                "trigger": "OnTouch",
                                "run_once": False,
                                "condition": {"type": "None", "name": "", "compare_op": 0, "int_value": 0, "bool_value": False},
                                "actions": [
                                    {"cmd": "TransferPlayer", "target": "maps/Map001.json", "int_value": 18, "float_value": 12}
                                ]
                            }
                        ],
                        "self_switch_a": False,
                        "self_switch_b": False,
                        "self_switch_c": False,
                        "self_switch_d": False
                    }
                }
            }
        ]
    }
    write_json(MAPS / "Map002.json", scene)


def generate_mapinfos():
    write_json(MAPS / "MapInfos.json", [
        {"id": 1, "name": "魔塔村", "parent_id": 0},
        {"id": 2, "name": "野外", "parent_id": 0}
    ])


def generate_script():
    with open(SCRIPTS / "test.lua", "w", encoding="utf-8") as f:
        f.write("set_int(\"gold\", 999)\n")


def main():
    ensure_dirs()
    generate_tileset()
    generate_sprites()
    generate_project_json()
    generate_tilesets_json()
    generate_data()
    generate_map001()
    generate_map002()
    generate_mapinfos()
    generate_script()
    print(f"Demo project generated at: {PROJECT}")


if __name__ == "__main__":
    main()
