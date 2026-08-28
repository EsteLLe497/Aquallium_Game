"""Generate a multi-level hero-tank hall inspired by Shikoku Aquarium.

Public facts are kept separate from inferred dimensions: the reference tank is
650 t and is wrapped by a right-side ramp that connects the first and second
floors. Exact construction drawings are not public. The official floor maps
show a flat viewing face and semi-elliptical rear tank, so this preview uses a
14.5 m wide x 6.1 m high x 9.8 m deep half-ellipse (about 681 m3 gross, close
to 650 m3 after rock displacement), a 2.8 m visitor ramp and one metre units.
"""

from __future__ import annotations

import json
import math
import struct
from dataclasses import dataclass, field
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "model" / "aquarium_watatsumi_hall.glb"


@dataclass
class MeshGroup:
    name: str
    positions: list[float] = field(default_factory=list)
    normals: list[float] = field(default_factory=list)
    texcoords: list[float] = field(default_factory=list)
    indices: list[int] = field(default_factory=list)


MATERIALS = {
    "WatatsumiArchitecture": (0.020, 0.028, 0.038, 1.0),
    "WatatsumiRamp": (0.040, 0.050, 0.060, 1.0),
    "WatatsumiRock": (0.025, 0.045, 0.055, 1.0),
    "WatatsumiWater": (0.010, 0.135, 0.245, 0.84),
    "WatatsumiGlass": (0.055, 0.220, 0.310, 0.16),
    "WatatsumiWaterSurface": (0.025, 0.300, 0.430, 0.58),
    "WatatsumiEmitter": (0.080, 0.640, 0.920, 1.0),
}
groups = {name: MeshGroup(name) for name in MATERIALS}


def quad(material, vertices, normal, uvs=((0, 0), (0, 1), (1, 1), (1, 0))):
    group = groups[material]
    base = len(group.positions) // 3
    for position, uv in zip(vertices, uvs):
        group.positions.extend(position)
        group.normals.extend(normal)
        group.texcoords.extend(uv)
    group.indices.extend((base, base + 1, base + 2, base, base + 2, base + 3))


def box(material, center, size):
    cx, cy, cz = center
    sx, sy, sz = (value * 0.5 for value in size)
    points = ((cx-sx,cy-sy,cz-sz),(cx+sx,cy-sy,cz-sz),
              (cx+sx,cy+sy,cz-sz),(cx-sx,cy+sy,cz-sz),
              (cx-sx,cy-sy,cz+sz),(cx+sx,cy-sy,cz+sz),
              (cx+sx,cy+sy,cz+sz),(cx-sx,cy+sy,cz+sz))
    for ids, normal in (((0,3,2,1),(0,0,-1)),((4,5,6,7),(0,0,1)),
                        ((0,4,7,3),(-1,0,0)),((1,2,6,5),(1,0,0)),
                        ((3,7,6,2),(0,1,0)),((0,1,5,4),(0,-1,0))):
        quad(material, [points[i] for i in ids], normal)


def ellipsoid(material, center, radii, seed, latitudes=7, longitudes=12):
    group = groups[material]
    base = len(group.positions) // 3
    cx, cy, cz = center
    rx, ry, rz = radii
    for latitude in range(latitudes + 1):
        v = latitude / latitudes
        phi = math.pi * v
        for longitude in range(longitudes + 1):
            u = longitude / longitudes
            theta = math.tau * u
            dx = math.sin(phi) * math.cos(theta)
            dy = math.cos(phi)
            dz = math.sin(phi) * math.sin(theta)
            rough = 1.0 + math.sin(phi) * 0.10 * math.sin(theta * 3 + seed)
            group.positions.extend((cx + dx*rx*rough, cy + dy*ry*rough, cz + dz*rz*rough))
            nx, ny, nz = dx/rx, dy/ry, dz/rz
            inv = 1.0 / max(math.sqrt(nx*nx + ny*ny + nz*nz), 0.0001)
            group.normals.extend((nx*inv, ny*inv, nz*inv))
            group.texcoords.extend((u, v))
    row = longitudes + 1
    for latitude in range(latitudes):
        for longitude in range(longitudes):
            a = base + latitude*row + longitude
            b = a + row
            group.indices.extend((a,b,a+1,a+1,b,b+1))


TANK_FRONT_X = 7.0
TANK_HALF_WIDTH = 14.50
TANK_DEPTH = 14.70
TANK_WATER_BOTTOM = 0.35
TANK_WATER_SURFACE = 12.45
UPPER_FLOOR_Y = 12.28
HALL_CEILING_Y = 18.60
PORTAL_Z = 17.80
RAMP_RISE = 12.22


def ramp_point(t):
    """Lower-right portal to upper-left portal via a concealed half helix."""
    t = max(0.0, min(1.0, t))
    y = 0.18 + RAMP_RISE * t

    def line(a, b, u):
        return (a[0] + (b[0]-a[0])*u, y, a[1] + (b[1]-a[1])*u)

    if t < 0.06:
        return line((-0.5, PORTAL_Z), (5.8, PORTAL_Z), t / 0.06)
    if t < 0.20:
        return line((5.8, PORTAL_Z), (17.8, PORTAL_Z), (t-0.06) / 0.14)
    if t < 0.80:
        u = (t-0.20) / 0.60
        angle = math.pi*0.5 - math.pi*u
        return (17.8 + math.cos(angle)*17.8, y, math.sin(angle)*17.8)
    if t < 0.97:
        return line((17.8, -PORTAL_Z), (5.8, -PORTAL_Z), (t-0.80) / 0.17)
    return line((5.8, -PORTAL_Z), (3.6, -PORTAL_Z), (t-0.97) / 0.03)


def ramp_strip(
        segments=144,
        width=4.2,
        wall_height=2.2,
        arch_rise=3.0,
        arch_segments=8):
    left, right = [], []
    for index in range(segments + 1):
        t = index / segments
        point = ramp_point(t)
        before = ramp_point(max(0.0, t - 0.002))
        after = ramp_point(min(1.0, t + 0.002))
        dx, dz = after[0] - before[0], after[2] - before[2]
        inv = 1.0 / max(math.hypot(dx, dz), 0.0001)
        side = (-dz*inv*width*0.5, dx*inv*width*0.5)
        left.append((point[0]+side[0], point[1], point[2]+side[1]))
        right.append((point[0]-side[0], point[1], point[2]-side[1]))
    for index in range(segments):
        quad("WatatsumiRamp", (left[index], right[index], right[index+1], left[index+1]), (0,1,0))
        midpoint_t = (index + 0.5) / segments
        enclosed = 0.06 <= midpoint_t <= 0.97
        for edge, sign in ((left, 1.0), (right, -1.0)):
            p0, p1 = edge[index], edge[index+1]
            normal = (sign*(p1[2]-p0[2]), 0.0, sign*(p0[0]-p1[0]))
            length = max(math.hypot(normal[0], normal[2]), 0.0001)
            normal = (normal[0]/length, 0, normal[2]/length)
            if enclosed:
                quad("WatatsumiArchitecture",
                     ((p0[0],p0[1]-0.18,p0[2]),
                      (p0[0],p0[1]+wall_height,p0[2]),
                      (p1[0],p1[1]+wall_height,p1[2]),
                      (p1[0],p1[1]-0.18,p1[2])), normal)
            else:
                quad("WatatsumiRamp",
                     ((p0[0],p0[1]-0.18,p0[2]),(p0[0],p0[1]+0.32,p0[2]),
                      (p1[0],p1[1]+0.32,p1[2]),(p1[0],p1[1]-0.18,p1[2])), normal)
                quad("WatatsumiRamp",
                     ((p0[0],p0[1]+1.08,p0[2]),(p0[0],p0[1]+1.16,p0[2]),
                      (p1[0],p1[1]+1.16,p1[2]),(p1[0],p1[1]+1.08,p1[2])), normal)
        if enclosed:
            # Elliptical arch: vertical side walls remain readable at eye
            # height while the roof rises generously above the player.
            for arch_index in range(arch_segments):
                u0 = arch_index / arch_segments
                u1 = (arch_index + 1) / arch_segments
                a0 = math.pi * u0
                a1 = math.pi * u1

                def roof_point(segment_index, u, angle):
                    lp = left[segment_index]
                    rp = right[segment_index]
                    return (
                        lp[0] + (rp[0] - lp[0]) * u,
                        lp[1] + wall_height + math.sin(angle) * arch_rise,
                        lp[2] + (rp[2] - lp[2]) * u)

                roof = (
                    roof_point(index, u0, a0),
                    roof_point(index, u1, a1),
                    roof_point(index + 1, u1, a1),
                    roof_point(index + 1, u0, a0))
                quad("WatatsumiArchitecture", roof, (0,-1,0))
        elif index % 8 == 0:
            for edge in (left, right):
                point = edge[index]
                box("WatatsumiRamp", (point[0], point[1]+0.72, point[2]),
                    (0.08, 0.88, 0.08))


def half_ellipse_cap(material, y, normal_y, segments=40):
    """Horizontal cap for the flat-front, curved-back tank footprint."""
    group = groups[material]
    perimeter = [(TANK_FRONT_X, y, -TANK_HALF_WIDTH)]
    for index in range(segments + 1):
        angle = -math.pi*0.5 + math.pi*index/segments
        perimeter.append((TANK_FRONT_X + math.cos(angle)*TANK_DEPTH, y,
                          math.sin(angle)*TANK_HALF_WIDTH))
    perimeter.append((TANK_FRONT_X, y, TANK_HALF_WIDTH))
    center = (TANK_FRONT_X + TANK_DEPTH*0.42, y, 0.0)
    base = len(group.positions)//3
    group.positions.extend(center); group.normals.extend((0,normal_y,0)); group.texcoords.extend((0.5,0.5))
    for x, py, z in perimeter:
        group.positions.extend((x,py,z)); group.normals.extend((0,normal_y,0))
        group.texcoords.extend(((x-TANK_FRONT_X)/(TANK_DEPTH*2.0)+0.5,
                                z/(TANK_HALF_WIDTH*2.0)+0.5))
    count = len(perimeter)
    for index in range(count):
        a = base + 1 + index
        b = base + 1 + (index+1)%count
        group.indices.extend((base,a,b) if normal_y > 0 else (base,b,a))


def curved_tank_wall(material, segments=40):
    """Semi-elliptical rear wall taken from the official floor-map silhouette."""
    for index in range(segments):
        a0 = -math.pi*0.5 + math.pi*index/segments
        a1 = -math.pi*0.5 + math.pi*(index+1)/segments
        p0 = (TANK_FRONT_X+math.cos(a0)*TANK_DEPTH,
              TANK_WATER_BOTTOM, math.sin(a0)*TANK_HALF_WIDTH)
        p1 = (TANK_FRONT_X+math.cos(a0)*TANK_DEPTH,
              TANK_WATER_SURFACE, math.sin(a0)*TANK_HALF_WIDTH)
        p2 = (TANK_FRONT_X+math.cos(a1)*TANK_DEPTH,
              TANK_WATER_SURFACE, math.sin(a1)*TANK_HALF_WIDTH)
        p3 = (TANK_FRONT_X+math.cos(a1)*TANK_DEPTH,
              TANK_WATER_BOTTOM, math.sin(a1)*TANK_HALF_WIDTH)
        middle = (a0+a1)*0.5
        normal = (math.cos(middle)/TANK_DEPTH, 0,
                  math.sin(middle)/TANK_HALF_WIDTH)
        inv = 1.0/max(math.hypot(normal[0],normal[2]),0.0001)
        quad(material, (p0,p1,p2,p3), (normal[0]*inv,0,normal[2]*inv))


def arch_portal_collar(center_z, floor_y, x=6.42, width=4.2,
                       wall_height=2.2, arch_rise=3.0,
                       outer_width=5.0, top_clearance=5.9, segments=12):
    """Close the rectangular facade void tightly around an arch section."""
    normal = (-1.0, 0.0, 0.0)
    half_width = width * 0.5
    outer_half = outer_width * 0.5
    # Thin vertical shoulders close the gap beside both tunnel walls.
    for side in (-1.0, 1.0):
        z0 = center_z + side * half_width
        z1 = center_z + side * outer_half
        lo, hi = min(z0, z1), max(z0, z1)
        quad("WatatsumiArchitecture",
             ((x,floor_y,lo),(x,floor_y+top_clearance,lo),
              (x,floor_y+top_clearance,hi),(x,floor_y,hi)), normal)
    # Curved spandrel fills the space above the elliptical crown.
    for index in range(segments):
        u0 = index / segments
        u1 = (index + 1) / segments
        z0 = center_z - half_width + width * u0
        z1 = center_z - half_width + width * u1
        inner0 = floor_y + wall_height + math.sin(math.pi*u0) * arch_rise
        inner1 = floor_y + wall_height + math.sin(math.pi*u1) * arch_rise
        top = floor_y + top_clearance
        quad("WatatsumiArchitecture",
             ((x,inner0,z0),(x,top,z0),(x,top,z1),(x,inner1,z1)), normal)


def build():
    # Enlarged two-storey central hall; the exhibit remains the only source.
    box("WatatsumiArchitecture", (5.0,-0.18,0), (66,0.36,48))
    box("WatatsumiArchitecture", (5.0,9.2,23.85), (66,18.4,0.30))
    box("WatatsumiArchitecture", (5.0,9.2,-23.85), (66,18.4,0.30))
    box("WatatsumiArchitecture", (-27.85,9.2,0), (0.30,18.4,48))
    box("WatatsumiArchitecture", (5.0,HALL_CEILING_Y,0), (66,0.40,48))

    # Fill both unused outer-edge voids up to the enlarged 5.0 m portals.
    # Their inner faces now terminate on the same authored portal edge, so the
    # facade no longer contains the thin slits left by the former patchwork.
    box("WatatsumiArchitecture", (-8.4,9.1,21.55), (30.6,18.2,4.60))
    box("WatatsumiArchitecture", (-8.4,9.1,-21.55), (30.6,18.2,4.60))

    # The enlarged 29 x 12.2 m acrylic view is the hall's dominant landmark.
    box("WatatsumiArchitecture", (7.0,0.18,0), (1.0,0.36,31.2))
    # A continuous opaque facade closes the entire void above the acrylic.
    # Narrow full-height jambs meet it exactly at the tank's side edges.
    box("WatatsumiArchitecture", (7.0,15.425,0), (1.0,5.95,29.0))
    box("WatatsumiArchitecture", (7.0,9.275,-14.575), (1.0,18.2,0.15))
    box("WatatsumiArchitecture", (7.0,9.275,14.575), (1.0,18.2,0.15))
    curved_tank_wall("WatatsumiWater")
    half_ellipse_cap("WatatsumiRock", 0.20, 1.0)
    # Front water interface then acrylic; both are separate transparent batches.
    quad("WatatsumiWater", ((7.12,TANK_WATER_BOTTOM,-TANK_HALF_WIDTH),
                             (7.12,TANK_WATER_SURFACE,-TANK_HALF_WIDTH),
                             (7.12,TANK_WATER_SURFACE,TANK_HALF_WIDTH),
                             (7.12,TANK_WATER_BOTTOM,TANK_HALF_WIDTH)), (-1,0,0))
    quad("WatatsumiGlass", ((6.94,0.30,-14.62),(6.94,12.58,-14.62),
                             (6.94,12.58,14.62),(6.94,0.30,14.62)), (-1,0,0))
    half_ellipse_cap("WatatsumiWaterSurface", TANK_WATER_SURFACE, 1.0)
    for index, data in enumerate(((10.0,-9.6,3.8,1.5,2.8),(12.5,7.6,4.5,2.0,3.2),
                                  (17.0,-3.6,4.0,2.4,3.8),(19.0,10.2,2.8,1.4,2.4),
                                  (16.0,-12.0,3.0,1.5,2.2))):
        x,z,rx,ry,rz=data
        ellipsoid("WatatsumiRock", (x,0.35+ry*0.55,z), (rx,ry,rz), index+0.3)

    # The 1F entrance and 2F exit are cut from the same facade grid. The lower
    # header begins above the complete arch crown; the upper opening remains
    # clear all the way to the raised hall ceiling.
    box("WatatsumiArchitecture", (6.80,12.28,PORTAL_Z), (0.70,12.24,5.00))
    box("WatatsumiArchitecture", (6.80,5.975,-PORTAL_Z), (0.70,11.95,5.00))
    arch_portal_collar(PORTAL_Z, 0.18, top_clearance=5.98)
    arch_portal_collar(
        -PORTAL_Z,
        UPPER_FLOOR_Y,
        top_clearance=HALL_CEILING_Y - 0.20 - UPPER_FLOOR_Y)
    ramp_strip()

    # Sparse blue practicals follow the enclosed ramp. They are navigation
    # cues, not general hall lighting, and sit near the arch crown.
    for t in (0.075, 0.16, 0.34, 0.52, 0.70, 0.88):
        light = ramp_point(t)
        box("WatatsumiEmitter", (light[0], light[1]+4.72, light[2]),
            (1.15,0.06,0.18))

    # 2F layout: remove the deck directly in front of the acrylic and relocate
    # circulation to the opposite side of the atrium. Two side arms connect
    # the portal ends to a rear cross-passage, leaving the tank sightline open.
    box("WatatsumiRamp", (-8.50,UPPER_FLOOR_Y,-PORTAL_Z), (25.0,0.24,4.2))
    box("WatatsumiRamp", (-8.50,UPPER_FLOOR_Y,PORTAL_Z), (25.0,0.24,4.2))
    box("WatatsumiRamp", (-21.0,UPPER_FLOOR_Y,0.0), (4.2,0.24,39.8))

    # Low solid fascias and slim rails protect every open atrium edge without
    # restoring the former tank-front deck.
    for z in (-19.9, -15.7, 15.7, 19.9):
        box("WatatsumiRamp", (-8.50,UPPER_FLOOR_Y+0.17,z), (25.0,0.34,0.12))
        box("WatatsumiRamp", (-8.50,UPPER_FLOOR_Y+1.12,z), (25.0,0.08,0.10))
        for x in (-19.0, -15.5, -12.0, -8.5, -5.0, -1.5, 2.0):
            box("WatatsumiRamp", (x,UPPER_FLOOR_Y+0.64,z), (0.07,0.96,0.07))
    # A single restrained practical at the upper landing. The tank remains
    # the dominant source; this only gives visitors a distant navigation cue.
    box("WatatsumiEmitter", (3.8,UPPER_FLOOR_Y+2.2,-PORTAL_Z), (1.2,0.04,0.05))
    # A restrained waterline source lets the hall be lit by the exhibit only.
    box("WatatsumiEmitter", (6.82,TANK_WATER_SURFACE+0.17,0), (0.10,0.08,28.6))


def pad4(data, value=0):
    while len(data)%4: data.append(value)


def write_glb():
    binary=bytearray(); views=[]; accessors=[]; meshes=[]; nodes=[]; materials=[]
    material_indices={}
    transparent={"WatatsumiWater","WatatsumiGlass","WatatsumiWaterSurface"}
    for name, rgba in MATERIALS.items():
        material_indices[name]=len(materials)
        materials.append({"name":name,"pbrMetallicRoughness":{"baseColorFactor":list(rgba),
                          "metallicFactor":0.05,"roughnessFactor":0.20 if name in transparent else 0.72},
                          "alphaMode":"BLEND" if name in transparent else "OPAQUE",
                          "doubleSided":name in transparent})
    def view(payload,target):
        pad4(binary); offset=len(binary); binary.extend(payload)
        views.append({"buffer":0,"byteOffset":offset,"byteLength":len(payload),"target":target})
        return len(views)-1
    def accessor(index,component,count,kind,minimum=None,maximum=None):
        item={"bufferView":index,"componentType":component,"count":count,"type":kind}
        if minimum is not None: item.update(min=minimum,max=maximum)
        accessors.append(item); return len(accessors)-1
    for name, group in groups.items():
        if not group.indices: continue
        points=list(zip(group.positions[0::3],group.positions[1::3],group.positions[2::3]))
        pa=accessor(view(struct.pack(f"<{len(group.positions)}f",*group.positions),34962),5126,len(points),"VEC3",
                    [min(p[a] for p in points) for a in range(3)],[max(p[a] for p in points) for a in range(3)])
        na=accessor(view(struct.pack(f"<{len(group.normals)}f",*group.normals),34962),5126,len(points),"VEC3")
        ta=accessor(view(struct.pack(f"<{len(group.texcoords)}f",*group.texcoords),34962),5126,len(points),"VEC2")
        ia=accessor(view(struct.pack(f"<{len(group.indices)}I",*group.indices),34963),5125,len(group.indices),"SCALAR")
        meshes.append({"name":name,"primitives":[{"attributes":{"POSITION":pa,"NORMAL":na,"TEXCOORD_0":ta},
                       "indices":ia,"material":material_indices[name],"mode":4}]})
        nodes.append({"name":name,"mesh":len(meshes)-1})
    doc={"asset":{"version":"2.0","generator":"Watatsumi Hall Generator"},"scene":0,
         "scenes":[{"name":"Watatsumi_Hero_Tank_Hall","nodes":list(range(len(nodes)))}],
         "nodes":nodes,"meshes":meshes,"materials":materials,"accessors":accessors,
         "bufferViews":views,"buffers":[{"byteLength":len(binary)}],
         "extras":{"units":"meters","previewKey":6,"referenceVolumeTonnes":650,
                   "inferredTankPlan":"flat-front semi-ellipse","inferredTankSize":[14.7,12.1,29.0],
                   "inferredGrossVolumeM3":4050.0,"rampWidth":4.2,"rampRise":12.22,
                   "tunnelWallHeight":2.2,"tunnelArchRise":3.0,
                   "rampSequence":["lower-right entry","concealed wall run",
                                   "rear half-helix tunnel","upper-left re-entry",
                                   "deep side platforms"],
                   "futureUpperConnectors":1}}
    json_data=bytearray(json.dumps(doc,separators=(",",":")).encode()); pad4(json_data,0x20); pad4(binary)
    total=12+8+len(json_data)+8+len(binary); OUTPUT.parent.mkdir(parents=True,exist_ok=True)
    with OUTPUT.open("wb") as output:
        output.write(struct.pack("<4sII",b"glTF",2,total)); output.write(struct.pack("<I4s",len(json_data),b"JSON")); output.write(json_data)
        output.write(struct.pack("<I4s",len(binary),b"BIN\0")); output.write(binary)
    return {"meshes":len(meshes),"vertices":sum(len(g.positions)//3 for g in groups.values()),
            "triangles":sum(len(g.indices)//3 for g in groups.values()),"bytes":OUTPUT.stat().st_size}


if __name__ == "__main__":
    build(); stats=write_glb(); print(json.dumps({"glb":str(OUTPUT),**stats}))
