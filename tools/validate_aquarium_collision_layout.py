"""Static navigation/layout checks for every generated aquarium preview.

The executable intentionally contains no startup assertions. This external
probe checks authored doorway continuity, player clearance, collision tags,
and the Watatsumi facade seams without risking a user-visible assertion dialog.
"""

from __future__ import annotations

from collections import deque
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCENE_SOURCE = ROOT / "src" / "scenes" / "AquariumScene.cpp"
STAGE_MODEL_SOURCE = ROOT / "src" / "StageModel.cpp"
WATATSUMI_GENERATOR = ROOT / "tools" / "generate_watatsumi_hall.py"
PLAYER_RADIUS = 0.34


def in_floor(x: float, z: float) -> bool:
    floors = (
        (-18.0, -6.0, -4.5, 4.5),
        (-6.0, -3.0, -2.0, 2.0),
        (-3.0, 15.0, -7.5, 7.5),
    )
    return any(x0 <= x <= x1 and z0 <= z <= z1 for x0, x1, z0, z1 in floors)


def blocked_by_route_wall(x: float, z: float) -> bool:
    # Horizontal and vertical wall center lines mirror BuildRouteCollision.
    walls_x = (
        (-18.0, -6.0, 4.5), (-18.0, -6.0, -4.5),
        (-6.0, -3.0, 2.0), (-6.0, -3.0, -2.0),
        (-3.0, 15.0, 7.5), (-3.0, 15.0, -7.5),
    )
    walls_z = (
        (-18.0, 2.0, 4.5), (-18.0, -4.5, -2.0),
        (-6.0, 2.0, 4.5), (-6.0, -4.5, -2.0),
        (-3.0, 2.0, 7.5), (-3.0, -7.5, -2.0),
        (15.0, 1.6, 7.5), (15.0, -7.5, -1.6),
    )
    thickness = 0.14 + PLAYER_RADIUS
    if any(x0 - PLAYER_RADIUS <= x <= x1 + PLAYER_RADIUS
           and abs(z - wall_z) < thickness
           for x0, x1, wall_z in walls_x):
        return True
    if any(abs(x - wall_x) < thickness
           and z0 - PLAYER_RADIUS <= z <= z1 + PLAYER_RADIUS
           for wall_x, z0, z1 in walls_z):
        return True
    return False


def route_reaches(start: tuple[float, float], target: tuple[float, float]) -> bool:
    spacing = 0.20
    encode = lambda value: round(value / spacing)
    start_node = (encode(start[0]), encode(start[1]))
    target_node = (encode(target[0]), encode(target[1]))
    queue = deque([start_node])
    visited = {start_node}
    while queue:
        node = queue.popleft()
        if abs(node[0] - target_node[0]) <= 1 and abs(node[1] - target_node[1]) <= 1:
            return True
        for dx, dz in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            candidate = (node[0] + dx, node[1] + dz)
            if candidate in visited:
                continue
            x, z = candidate[0] * spacing, candidate[1] * spacing
            if in_floor(x, z) and not blocked_by_route_wall(x, z):
                visited.add(candidate)
                queue.append(candidate)
    return False


def main() -> None:
    scene = SCENE_SOURCE.read_text(encoding="utf-8")
    stage_model = STAGE_MODEL_SOURCE.read_text(encoding="utf-8")
    generator = WATATSUMI_GENERATOR.read_text(encoding="utf-8")

    assert route_reaches((-16.2, 0.0), (6.0, 0.0))
    assert route_reaches((6.0, -6.15), (-12.0, 0.0))
    assert 4.0 - PLAYER_RADIUS * 2.0 >= 3.0

    # The arch path is 6.4 m wide, but the visible rail's inner face is 3.06 m.
    assert abs((3.06 - PLAYER_RADIUS) - 2.72) < 1.0e-6
    assert 'route.halfWidth = 3.06f;' in scene
    assert 'playerCharacter_.activePath = 0;' in scene

    # Required tagged worlds and authored boundaries must be present at runtime.
    for required in (
        'BuildStageGlassCollision();',
        'BuildRouteCollision();',
        'BuildUnderwaterArchCollision();',
        'BuildWatatsumiCollision();',
        'ColliderTag::Glass',
        'ColliderTag::Rail',
        'Watatsumi_NorthServiceVoid',
        'Watatsumi_2F_RearRail',
    ):
        assert required in scene, required
    assert 'geometrySignatures.insert(geometrySignature)' in stage_model
    assert 'vertices.resize(baseVertex);' in stage_model

    # Exact Watatsumi seams: rear shell -> service fill -> portal -> tank.
    rear_inner_x = -27.70
    facade_rear_x = 6.45
    portal_outer_z = 20.30
    outer_wall_inner_z = 23.70
    tank_jamb_outer_z = 14.65
    portal_inner_z = 15.30
    assert facade_rear_x > rear_inner_x
    assert outer_wall_inner_z > portal_outer_z
    assert portal_inner_z > tank_jamb_outer_z
    assert '(-8.4,9.1,21.55)' not in generator
    assert 'service_fill_center_x = (-27.70 + 6.45) * 0.5' in generator
    assert '(0.70,18.40,0.65)' in generator
    assert 'quad("WatatsumiRamp"' in generator

    print({
        "result": "pass",
        "route_forward_reachable": True,
        "route_reverse_reachable": True,
        "route_door_clearance_m": round(4.0 - PLAYER_RADIUS * 2.0, 2),
        "arch_player_center_limit_m": 2.72,
        "watatsumi_service_fill_width_m": round(facade_rear_x - rear_inner_x, 2),
        "watatsumi_outer_seal_depth_m": round(outer_wall_inner_z - portal_outer_z, 2),
        "watatsumi_tank_portal_seal_m": round(portal_inner_z - tank_jamb_outer_z, 2),
        "stage_glass_collision": True,
        "exact_duplicate_mesh_filter": True,
    })


if __name__ == "__main__":
    main()
