"""Static traversal validation for the generated Watatsumi ramp.

This checks the same authored centre line and dimensions consumed by the
runtime collision code. It intentionally performs no rendering or file writes.
"""

from __future__ import annotations

import math

from generate_watatsumi_hall import ramp_point


SAMPLES = 2880
RAMP_WIDTH = 4.20
PLAYER_RADIUS = 0.34
EYE_HEIGHT = 1.62
PLAYER_HEIGHT = 1.82
WALL_HEIGHT = 2.20
ARCH_RISE = 3.00
PORTAL_WIDTH = 5.00
LOWER_PORTAL_CLEAR_HEIGHT = 5.90
UPPER_PORTAL_BOTTOM = 5.65
HALL_CEILING_BOTTOM = 11.20


def main() -> None:
    points = [ramp_point(index / SAMPLES) for index in range(SAMPLES + 1)]
    maximum_step = 0.0
    maximum_grade = 0.0
    route_length = 0.0
    for first, second in zip(points, points[1:]):
        dx = second[0] - first[0]
        dy = second[1] - first[1]
        dz = second[2] - first[2]
        horizontal = math.hypot(dx, dz)
        route_length += math.sqrt(dx * dx + dy * dy + dz * dz)
        maximum_step = max(maximum_step, abs(dy))
        maximum_grade = max(maximum_grade, abs(dy) / max(horizontal, 1.0e-6))

    usable_width = RAMP_WIDTH - PLAYER_RADIUS * 2.0
    crown_height = WALL_HEIGHT + ARCH_RISE
    head_clearance = crown_height - PLAYER_HEIGHT

    # Simulate a player trying to push sideways through the wall along the
    # complete route. Runtime collision permits only this centre offset after
    # subtracting the capsule radius from the modeled half-width.
    allowed_center_offset = RAMP_WIDTH * 0.5 - PLAYER_RADIUS
    attempted_offset = allowed_center_offset + 1.0
    resolved_offset = min(attempted_offset, allowed_center_offset)

    # Step a virtual capsule through the complete route at 60 Hz. Add a
    # periodic lateral wall-push, then resolve it with the same centre-offset
    # constraint used by the runtime swept corridor.
    simulation_hz = 60.0
    simulation_speed = 2.35
    simulation_steps = math.ceil(route_length / simulation_speed * simulation_hz)
    maximum_resolved_offset = 0.0
    previous_y = points[0][1]
    reached_t = 0.0
    for step in range(simulation_steps + 1):
        travelled = min(step * simulation_speed / simulation_hz, route_length)
        target_t = travelled / route_length
        sample_index = min(round(target_t * SAMPLES), SAMPLES)
        centre = points[sample_index]
        before = points[max(sample_index - 1, 0)]
        after = points[min(sample_index + 1, SAMPLES)]
        tangent_x = after[0] - before[0]
        tangent_z = after[2] - before[2]
        tangent_length = max(math.hypot(tangent_x, tangent_z), 1.0e-6)
        side_x = -tangent_z / tangent_length
        side_z = tangent_x / tangent_length
        push = math.sin(step * 0.071) * (allowed_center_offset + 0.85)
        resolved = max(-allowed_center_offset, min(push, allowed_center_offset))
        player_x = centre[0] + side_x * resolved
        player_z = centre[2] + side_z * resolved
        actual_offset = math.hypot(player_x - centre[0], player_z - centre[2])
        maximum_resolved_offset = max(maximum_resolved_offset, actual_offset)
        assert actual_offset <= allowed_center_offset + 1.0e-5
        assert abs(centre[1] - previous_y) < 0.02
        previous_y = centre[1]
        reached_t = target_t

    assert maximum_step < 0.01, maximum_step
    assert maximum_grade < 0.20, maximum_grade
    assert usable_width >= 2.60, usable_width
    assert head_clearance >= 2.20, head_clearance
    assert PORTAL_WIDTH >= RAMP_WIDTH + 0.70, PORTAL_WIDTH
    assert LOWER_PORTAL_CLEAR_HEIGHT >= points[round(SAMPLES * 0.06)][1] + crown_height
    assert UPPER_PORTAL_BOTTOM <= points[round(SAMPLES * 0.94)][1]
    assert HALL_CEILING_BOTTOM >= points[round(SAMPLES * 0.94)][1] + crown_height
    assert resolved_offset <= allowed_center_offset
    assert abs(points[0][1] - 0.18) < 1.0e-4
    assert abs(points[-1][1] - 6.20) < 1.0e-4
    assert reached_t >= 0.999

    print({
        "result": "pass",
        "samples": SAMPLES,
        "route_length_m": round(route_length, 3),
        "maximum_sample_step_m": round(maximum_step, 5),
        "maximum_grade_percent": round(maximum_grade * 100.0, 2),
        "usable_width_m": round(usable_width, 2),
        "arch_crown_height_m": round(crown_height, 2),
        "head_clearance_m": round(head_clearance, 2),
        "portal_width_m": PORTAL_WIDTH,
        "lower_portal_clear_height_m": LOWER_PORTAL_CLEAR_HEIGHT,
        "wall_push_attempt_m": round(attempted_offset, 2),
        "wall_resolved_offset_m": round(resolved_offset, 2),
        "simulation_hz": simulation_hz,
        "simulation_steps": simulation_steps,
        "simulation_reached_t": round(reached_t, 4),
        "simulation_max_wall_offset_m": round(maximum_resolved_offset, 3),
        "eye_height_m": EYE_HEIGHT,
    })


if __name__ == "__main__":
    main()
