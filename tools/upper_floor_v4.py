"""Route 09 upper rooms and isolated ramp. Units: authored metres, Z mirrored."""
import math

def ramp_points():
    points = [(10.5, 3.0), (15.0, 3.0)]
    for i in range(1, 25):
        a = -math.pi / 2 + i * math.pi / 48
        points.append((15 + 3 * math.cos(a), 6 + 3 * math.sin(a)))
    for i in range(1, 50):
        points.append((18, 6 + 11.3 * i / 49))
    for i in range(1, 25):
        a = i * math.pi / 48
        points.append((15 + 3 * math.cos(a), 17.3 + 3 * math.sin(a)))
    for i in range(1, 97):
        points.append((15 - 23.85 * i / 96, 20.3))
    for i in range(1, 25):
        a = math.pi / 2 + i * math.pi / 48
        points.append((-8.85 + 3 * math.cos(a), 17.3 + 3 * math.sin(a)))
    points += [(-11.85, 17.0)]
    distances = [0.0]
    for a, b in zip(points, points[1:]):
        distances.append(distances[-1] + math.dist(a, b))
    # Both upper bridges cross a level lower corridor. Ease the first/last
    # two metres of the rise, retaining a full flat landing at the exit.
    start = next(d for p, d in zip(points, distances) if p[1] >= 12.5)
    length = distances[-1] - start - 0.3
    def height(s):
        s = max(0, min(length, s))
        ease = 2.0
        integral = s*s/(2*ease) if s < ease else (
            length-ease-(length-s)**2/(2*ease) if s > length-ease else s-ease/2)
        return 5.5 * integral / (length-ease)
    return tuple((x, height(d-start), -z) for (x,z),d in zip(points,distances))

def build_upper(g):
    from upper_doors import build
    build(g)
    floor = 5.5
    def solid(name, center, size, material='ReceptionWall'):
        g.add_box(material, 'V4_'+name, center, size, 'Solid')
    def wall(name, axis, fixed, lo, hi, openings=(), y=floor, h=5.85):
        cursor=lo
        def piece(suffix,a,b,base,height):
            if b-a <= .001 or height <= .001: return
            center=(fixed,base+height/2,-(a+b)/2) if axis=='x' else ((a+b)/2,base+height/2,-fixed)
            size=(.3,height,b-a) if axis=='x' else (b-a,height,.3)
            solid(name+suffix,center,size)
        for i,(a,b) in enumerate(sorted(openings)):
            piece(str(i),cursor,a,y,h)
            piece('Header'+str(i),a,b,y+4.2,h-4.2)
            cursor=b
        piece('End',cursor,hi,y,h)
    def slab(name,x0,z0,x1,z1):
        g.add_floor('V4_'+name,(x0+x1)/2,-(z0+z1)/2,x1-x0,z1-z0,floor,'HeroRampFloor')
    # Replace the obsolete west-facing exit wall only below the 2F floor.
    wall('FormerExitSeal','x',-11.5,.9,5.7,y=0,h=5.30)
    wall('EastFacade','x',14.25,0,17,[(4,7),(9,12)])
    wall('WestFacade','x',-14.25,0,17,[(4,7)])
    wall('NorthFacade','z',17,-14.25,14.25,[(-14.05,-9.65)])
    # Close the south facade without blocking the central terrace entrance.
    wall('SouthFacade','z',0,-14.25,14.25,[(-1.5,1.5)])
    # The old cross-bridge guard must have a genuine central opening.
    # Its colliders/mesh are filtered at generation, rebuilt below.
    for x in (-5.55,5.55):
        solid('TerraceGuard'+str(x),(x,6.025,-.9),(8.10,1.05,.18),'WatatsumiArchitecture')
    slab('TerraceThreshold',-1.5,0,1.5,.9)
    def room(name,x0,z0,x1,z1,side,door):
        slab(name+'Floor',x0,z0,x1,z1)
        g.add_box('ReceptionWall','V4_'+name+'Ceiling',((x0+x1)/2,11.5,-(z0+z1)/2),(x1-x0,.3,z1-z0))
        wall(name+'West','x',x0,z0,z1,[door] if side=='west' else [])
        if name != 'Dolphin':
            wall(name+'East','x',x1,z0,z1,[door] if side=='east' else [])
        wall(name+'North','z',z1,x0,x1)
        wall(name+'South','z',z0,x0,x1)
    room('Dolphin',-24.25,0,-14.25,10,'east',(4,7))
    room('Management',21,1,31,8,'west',(4,7))
    room('Reef',21,8.3,31,18.3,'west',(9,12))
    # Dolphin shares the hall facade: never duplicate coplanar wall faces.
    for name,z0,z1 in [('Management',4,7),('Reef',9,12)]:
        slab(name+'Bridge',14.25,z0,21,z1)
        wall(name+'BridgeNear','z',z0,14.25,21)
        wall(name+'BridgeFar','z',z1,14.25,21)
        g.add_box('ReceptionWall','V4_'+name+'BridgeRoof',(17.625,11.5,-(z0+z1)/2),(6.75,.3,z1-z0))
    slab('Terrace',-5,-9,5,0)
    wall('TerraceLeft','x',-5,-9,0,h=1.2)
    wall('TerraceRight','x',5,-9,0,h=1.2)
    wall('TerraceEnd','z',-9,-5,5,h=1.2)
    # Display tanks have a dedicated cheap water family; the giant hero-tank
    # lighting coordinates are deliberately not reused in these rooms.
    def display(name,x,z,width,depth):
        solid(name+'Back',(x,7.6,-(z+depth/2)),(width+.35,4.2,.20),'TankShell')
        for side in (-1,1):
            solid(name+'Side'+str(side),(x+side*(width/2+.10),7.6,-z),(.20,4.2,depth),'TankShell')
        for y in (5.6,9.6):
            solid(name+'Cap'+str(y),(x,y,-z),(width,.20,depth),'TankShell')
        # Recessed tank on rear wall, front viewing pane on the public side.
        g.add_box('TankWaterDisplayBox','V4_'+name+'Water',(x,7.7,-(z-depth/2-.03)),(width,3.4,.04),'Glass')
        solid(name+'Sill',(x,5.85,-(z-depth/2-.08)),(width+.3,.7,.25),'ReceptionMetal')
    display('DolphinExhibit',-19.25,8,8,2)
    display('ReefExhibit',26,16.2,8,2.4)
    # The optional-clue room is deliberately blue. These fixtures reuse one
    # emissive batch; the shader supplies a cheap room-wide blue bounce.
    for i,x in enumerate((23.0,26.0,29.0)):
        g.add_box('EmissiveCyan','V4_ReefBlueCeiling'+str(i),(x,10.95,-13.2),(1.25,.045,.18))
    for i,z in enumerate((9.0,12.0,15.0)):
        g.add_box('EmissiveCyan','V4_ReefBlueWall'+str(i),(30.82,6.15,-z),(.035,.18,1.15))
    for i,x in enumerate((23,26,29)):
        solid('Desk'+str(i),(x,6.25,-6.6),(2.2,.12,1.0),'ReceptionCounter')
        solid('DeskBase'+str(i),(x,5.86,-6.6),(1.8,.72,.65),'ReceptionMetal')
        solid('Monitor'+str(i),(x,6.65,-6.8),(.8,.55,.10),'ReceptionMetal')
        g.add_box('EmissiveCyan','V4_Screen'+str(i),(x,6.65,-6.735),(.7,.43,.015))
    for name,x,z in [('Dolphin',-18,3),('Reef',26,10),('Terrace',-3,-5)]:
        solid(name+'Bench',(x,6,-z),(2.4,.2,.65),'ReceptionCounter')
        for side in (-1,1):
            solid(name+'BenchLeg'+str(side),(x+side*.85,5.7,-z),(.15,.4,.5),'ReceptionMetal')
    # A thin luminous clue card rests on the exhibition bench. It has no
    # collision so it cannot snag the player; selection uses a dedicated AABB.
    g.ensure_material('CluePaperGlow',(.72,.87,1.0,1.0))
    g.add_box('CluePaperGlow','V4_ReefCluePaper',(26.0,6.13,-10.0),(.58,.025,.40))
    # Fill the entrance's rectangular cut around the actual curved shell.
    # Front and back spandrels share the shell profile (10 segments).
    for x,half in ((10.72,2.4),(11.72,2.2)):
        for i in range(10):
            a,b=math.pi*i/10,math.pi*(i+1)/10
            z0,z1=-3+half*math.cos(a),-3+half*math.cos(b)
            y0,y1=1.55+2.45*math.sin(a),1.55+2.45*math.sin(b)
            v=((x,y0,z0),(x,y1,z1),(x,5.3,z1),(x,5.3,z0))
            g.append_quad('ReceptionWall',v,(-1,0,0))
    g.features.append({'type':'upper_v4','rooms':4,'ramp_exit':[-11.85,5.5,17],
        'bridge_clearance':1.32,'arch_start_z':25,'arch_water_start_z':23,
        'ramp_max_z':22.5})
