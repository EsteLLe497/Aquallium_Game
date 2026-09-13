"""Door fixtures and lightweight night environment for Route 09."""
import math

def build(g):
    for name,rgba in {'TerraceDoorGlass':(.15,.30,.40,.18),
                      'TerraceDoorFrame':(.16,.21,.25,1),
                      'ExteriorNightSky':(.02,.05,.1,1),
                      'ManagementDoor':(.045,.070,.085,1)}.items():
        g.ensure_material(name,rgba)
    def box(mat,name,p,s,tag=None): g.add_box(mat,name,p,s,tag)
    for side in (-1,1):
        x=side*.75
        box('TerraceDoorGlass','TerracePane'+str(side),(x,7.10,0),(1.36,3.02,.025))
        for xx in (side*.045,side*1.455):
            box('TerraceDoorFrame','TerraceStile',(xx,7.10,0),(.07,3.2,.12))
        for y in (5.56,8.64):
            box('TerraceDoorFrame','TerraceRail',(x,y,0),(1.46,.12,.12))
        for z in (-.14,.14):
            box('TerraceDoorFrame','TerraceHandle',(side*.18,6.8,z),(.035,.65,.05))
        g.add_collider_box('TerraceDoorLeft' if side<0 else 'TerraceDoorRight',
                           (x,7.1,0),(1.5,3.2,.16),'Glass')
    box('ReceptionMetal','TerraceTransom',(0,9.1,0),(3,1,.30),'Solid')
    # Steel door, reinforced ribs, substantial casing and red access reader.
    box('ManagementDoor','ManagementLockedDoor',(21,7.1,-5.5),(.22,3.2,2.86),'Solid')
    box('ReceptionWall','ManagementDoorHeader',(21,9.1,-5.5),(.30,1,3),'Solid')
    for z in (4.04,6.96):
        box('ReceptionMetal','ManagementJamb',(20.82,7.15,-z),(.20,3.3,.14),'Solid')
    for y in (5.9,6.7,7.5,8.3):
        box('ManagementDoor','ManagementReinforcement',(20.855,y,-5.5),(.10,.10,2.6))
    box('ManagementDoor','ManagementLock',(20.80,6.85,-4.45),(.20,.32,.24))
    box('ManagementDoor','ManagementReader',(20.69,7.15,-4.45),(.02,.10,.10))
    # A single static sphere, not a camera-following surface inside the rooms.
    # Opaque building walls naturally occlude it. No light/shadow pass required.
    r=300
    for iy in range(16):
        a,b=-math.pi/2+math.pi*iy/16,-math.pi/2+math.pi*(iy+1)/16
        for ix in range(48):
            c,d=math.tau*ix/48,math.tau*(ix+1)/48
            def p(v,u): return (r*math.cos(v)*math.cos(u),r*math.sin(v),r*math.cos(v)*math.sin(u))
            g.append_quad('ExteriorNightSky',(p(a,c),p(b,c),p(b,d),p(a,d)),(0,1,0))
