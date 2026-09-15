"""Xiaomi Smart Pet Food Feeder 2 (XMWSQ02) reference model.

Run inside FreeCAD (exec(open(__file__).read()) from the console, or via the
FreeCAD MCP). Rebuilds XiaomiFeeder2.FCStd and XiaomiFeeder2.step next to this
file from the parameters below.

Coordinate system:  X = width (right is +X),  Y = depth (FRONT is -Y, back is +Y),
                    Z = up, Z=0 at the floor (feet included).
                    Origin is the plan centre of the overall footprint bbox.

Dimensions are caliper/ruler measurements on the unit; the overall proportions
were cross-checked against Xiaomi's orthographic marketing renders (not kept in
the repo). The plate outline is footprint.svg, traced from the bottom render's
alpha channel and scaled to the spec'd 220 x 324 footprint.
"""
import math, os
import FreeCAD, Part, importSVG

BASE = "/home/cristian/Source/oddware/xiaomi.feeder.iv2001/accessory/feeder-model"
V = FreeCAD.Vector

P = dict(
    # ---- envelope -------------------------------------------------------
    width=220.0, depth=324.0, height=370.0,          # spec (height verified)
    plate_t=22.0,                                     # (caliper) base plate thickness
    # feet (caliper): cone boss on the plate underside, rubber pad below it
    foot_cone_d0=21.0, foot_cone_d1=19.0, foot_cone_h=1.0,
    foot_rubber_d=15.0, foot_rubber_h=1.25,
    foot_front_inset=32.0,     # front pair: from plate front and from the tongue side edges
    foot_mid_from_front=153.0, # middle pair, same X as the front pair (121 c-c behind the front pair)
    foot_back_x=83.5,          # back pair: 182 outside-to-outside of rubber
    foot_back_from_front=296.5,# back pair: rubber front edge 289 from the plate front
    # ---- body -----------------------------------------------------------
    body_depth=194.0,        # (ruler) back face to front-most point; plan = trace back half mirrored
    seam_gap=108.0,          # (caliper) cave ceiling to hopper/body seam line
    # ---- cave (bowl bay recess in lower front of body) ------------------
    cave_w=186.4,            # (caliper/STEP) +-93.2
    cave_h=84.0,             # (caliper) above plate top
    cave_back_D=142.0,       # (caliper) back wall, from body back face = 4 mm behind the bowl back wall
    cave_plan_r=34.0,        # (caliper) back corners in plan (bowl R30 + 4 mm offset)
    cave_top_r=22.0,         # (caliper) arch top corners + back-top edge
    # food outlet in the cave roof (caliper): pill across X, front edge behind the body front face
    outlet_w=55.0, outlet_l=22.0, outlet_from_front=12.0, outlet_h=40.0,
    # ---- load-cell base: a tray sitting on the plate ----------------------
    lc_size=156.0, lc_h=15.0, lc_r=18.0,              # (caliper) outer
    lc_rim=8.0, lc_recess=12.0,                       # (caliper) rim wall width, recess depth
    # ---- bowl unit (removable holder + stainless bowl) ------------------
    bowl_w=178.4,            # (caliper) +-89.2, same as the plate tongue
    bowl_len=178.0,          # (caliper) front face (flush with plate front) to back wall
    bowl_float=3.0,          # (caliper) skirt bottom above plate top (= base top)
    # underside (caliper + photo): the unit is a shell of shell_t under the lofted bowl with
    # a 2.5 outer skirt; a 1.45 index wall 21 in from the edge and four 25 x 8 pads on its
    # outside (one per side midpoint) hang straight off the shell's underside. The wall drops
    # into the base recess with <1 mm play; the pads bear on the base rim, their bottoms
    # pad_lift above the wall bottom. No other material.
    skirt_t=2.5, rect_t=1.45, rect_inset=21.0,
    pad_w=25.0, pad_t=8.0, pad_lift=3.0,
    shell_t=2.5,             # guess: plastic shell + steel under the bowl
    bowl_top_front=46.0,     # (caliper) top surface above plate top, at the front edge
    bowl_top_back=60.0,      # (caliper) ... at the back wall  (4.5 deg slope)
    bowl_corner_r=30.0,      # (caliper) all four plan corners (front ones fit the trace's R29)
    # stainless bowl (caliper): flat 5 mm lip, smooth bowl below it
    lip_w=5.0,
    bowl_depth=43.0,         # below the lip surface, at the deepest point
    bowl_deep_from_front=80.0,   # deepest point, from the bowl's exterior front edge
    wall_angle_front=70.0, wall_angle_side=70.0, wall_angle_back=50.0,   # at the lip, from horizontal
    # ---- cosmetics ------------------------------------------------------
    # (caliper) all stacked up from the cave ceiling
    led_d=42.0, led_gap=50.0,                         # LED hole; gap = ceiling to hole bottom
    button_d=16.0, button_gap=17.0,                   # feed button; gap = ceiling to button bottom
    window_w=13.0, window_h=88.0, window_gap=50.0,    # hopper window; gap = LED top to window bottom
)

# --------------------------------------------------------------------------
def rounded_box(x0, x1, y0, y1, z0, z1, r_vertical=0.0):
    b = Part.makeBox(x1 - x0, y1 - y0, z1 - z0, V(x0, y0, z0))
    if r_vertical > 0:
        edges = [e for e in b.Edges if abs(e.tangentAt(e.FirstParameter).z) > 0.99]
        b = b.makeFillet(r_vertical, edges)
    return b

def stadium_yz_cut(xc, zc0, zc1, w, y_face, depth):
    """Vertical slot with round ends, cut into a face normal to -Y."""
    r = w / 2
    c0 = Part.makeCylinder(r, depth, V(xc, y_face - 1, zc0), V(0, 1, 0))
    c1 = Part.makeCylinder(r, depth, V(xc, y_face - 1, zc1), V(0, 1, 0))
    b = Part.makeBox(w, depth, zc1 - zc0, V(xc - r, y_face - 1, zc0))
    return c0.fuse([c1, b])

def rrect_wire(x0, x1, y0, y1, r, z):
    """Closed rounded-rectangle wire in the plane z."""
    r = max(min(r, (x1 - x0) / 2 - 1e-3, (y1 - y0) / 2 - 1e-3), 1e-3)
    c = [V(x1 - r, y1 - r, z), V(x0 + r, y1 - r, z), V(x0 + r, y0 + r, z), V(x1 - r, y0 + r, z)]
    segs = []
    for i in range(4):
        a0 = math.radians(90 * i); a1 = a0 + math.radians(90); am = (a0 + a1) / 2
        arc = Part.Arc(c[i] + V(r * math.cos(a0), r * math.sin(a0), 0),
                       c[i] + V(r * math.cos(am), r * math.sin(am), 0),
                       c[i] + V(r * math.cos(a1), r * math.sin(a1), 0))
        segs.append(arc.toShape())
        nxt = c[(i + 1) % 4] + V(r * math.cos(a1), r * math.sin(a1), 0)
        segs.append(Part.LineSegment(arc.EndPoint, nxt).toShape())
    return Part.Wire(Part.__sortEdges__(segs))

def bowl_inset(angle_deg, run, depth_total):
    """Wall profile: cubic Bezier in (inset, depth) leaving the lip at angle_deg from
    horizontal and arriving at (run, depth_total) horizontally. Returns inset(depth)."""
    th = math.radians(angle_deg)
    k1 = 0.55 * depth_total / math.sin(th)
    P = [(0.0, 0.0), (k1 * math.cos(th), k1 * math.sin(th)), (run * 0.55, depth_total), (run, depth_total)]
    def bez(u):
        w = [(1 - u) ** 3, 3 * u * (1 - u) ** 2, 3 * u * u * (1 - u), u ** 3]
        return sum(w[i] * P[i][0] for i in range(4)), sum(w[i] * P[i][1] for i in range(4))
    def inset(depth):
        lo, hi = 0.0, 1.0
        for _ in range(60):
            mid = (lo + hi) / 2
            if bez(mid)[1] < depth: lo = mid
            else: hi = mid
        return bez((lo + hi) / 2)[0]
    return inset

def try_fillet(shape, r, edge_filter, label):
    edges = [e for e in shape.Edges if edge_filter(e)]
    if not edges:
        return shape
    try:
        return shape.makeFillet(r, edges)
    except Exception as ex:  # noqa
        print(f"  fillet skipped on {label}: {ex}")
        return shape

def build():
    W, D, H = P["width"], P["depth"], P["height"]
    plate_top = P["foot_cone_h"] + P["foot_rubber_h"] + P["plate_t"]     # Z=0 is the rubber pad bottom
    yF, yB = -D / 2, D / 2                       # front / back plane

    # ---- footprint from the traced SVG (front is +y in the file -> mirror) ----
    tmp = FreeCAD.newDocument("_trace")
    importSVG.insert(os.path.join(BASE, "footprint.svg"), "_trace")
    tmp.recompute()
    wire = [o for o in tmp.Objects if hasattr(o, "Shape")][0].Shape.Wires[0]
    FreeCAD.closeDocument("_trace")
    foot = Part.Face(wire).mirror(V(0, 0, 0), V(0, 1, 0))
    bb = foot.BoundBox
    foot.translate(V(-(bb.XMin + bb.XMax) / 2, -(bb.YMin + bb.YMax) / 2, 0))
    print(f"footprint {foot.BoundBox.XLength:.1f} x {foot.BoundBox.YLength:.1f}")

    # ---- base plate ----------------------------------------------------------
    plate = foot.extrude(V(0, 0, P["plate_t"]))
    plate.translate(V(0, 0, plate_top - P["plate_t"]))
    by0 = yF                                   # bowl front face (flush with plate front)
    by1 = by0 + P["bowl_len"]                  # bowl back wall
    hw = P["bowl_w"] / 2

    # ---- feet ----------------------------------------------------------------
    xf = 89.2 - P["foot_front_inset"]
    yf_ = -D / 2
    spots = [(-xf, yf_ + P["foot_front_inset"]), (xf, yf_ + P["foot_front_inset"]),
             (-xf, yf_ + P["foot_mid_from_front"]), (xf, yf_ + P["foot_mid_from_front"]),
             (-P["foot_back_x"], yf_ + P["foot_back_from_front"]), (P["foot_back_x"], yf_ + P["foot_back_from_front"])]
    feet = []
    for x, y in spots:
        cone = Part.makeCone(P["foot_cone_d1"] / 2, P["foot_cone_d0"] / 2, P["foot_cone_h"],
                             V(x, y, P["foot_rubber_h"]))
        pad = Part.makeCylinder(P["foot_rubber_d"] / 2, P["foot_rubber_h"], V(x, y, 0))
        feet.append(cone.fuse(pad))
    feet = Part.makeCompound(feet)

    # ---- body: extrusion of the plate trace's back half, mirrored ----------------
    # The hopper sits flush on the plate, and its front corners measure as the back
    # corners mirrored (within 0.5 mm on the top render), so the plan is the trace
    # clipped at the body's mid-depth and reflected about it.
    ymid = yB - P["body_depth"] / 2
    back = foot.common(Part.makeBox(300, 400, 1, V(-150, ymid, -0.5))).Faces[0]
    on_cut = lambda e: abs(e.BoundBox.YMin - ymid) < 1e-3 and abs(e.BoundBox.YMax - ymid) < 1e-3
    edges = [e for e in back.OuterWire.Edges if not on_cut(e)]
    edges += [e.mirror(V(0, ymid, 0), V(0, 1, 0)) for e in edges]
    body_plan = Part.Face(Part.Wire(Part.__sortEdges__(edges)))
    print(f"body plan {body_plan.BoundBox.XLength:.1f} x {body_plan.BoundBox.YLength:.1f}, front Y={body_plan.BoundBox.YMin:.1f}")
    y_body_front = body_plan.BoundBox.YMin
    body = body_plan.extrude(V(0, 0, H - plate_top))
    body.translate(V(0, 0, plate_top))

    # cave: back wall at Y = yB - cave_back_D, R34 plan corners, R22 on the ceiling edges
    cy_back = yB - P["cave_back_D"]
    cz_top = plate_top + P["cave_h"]
    cave = Part.makeBox(P["cave_w"], cy_back - (yF - 20), P["cave_h"],
                        V(-P["cave_w"] / 2, yF - 20, plate_top))
    cave = try_fillet(cave, P["cave_plan_r"],
                      lambda e: e.BoundBox.ZLength > 1 and abs(e.BoundBox.YMin - cy_back) < 1e-3,
                      "cave plan corners")
    cave = try_fillet(cave, P["cave_top_r"],
                      lambda e: abs(e.BoundBox.ZMin - cz_top) < 1e-3 and e.BoundBox.ZLength < 1e-3
                      and e.BoundBox.YMin > yF - 19,          # all ceiling edges except the (virtual) front one
                      "cave ceiling edges")
    body = body.cut(cave)

    # food outlet: stadium in the cave roof, chute cut outlet_h up into the body
    ow, ol = P["outlet_w"], P["outlet_l"]
    oy0 = y_body_front + P["outlet_from_front"]
    r = ol / 2
    c0 = Part.makeCylinder(r, P["outlet_h"] + 2, V(-ow / 2 + r, oy0 + r, cz_top - 1))
    c1 = Part.makeCylinder(r, P["outlet_h"] + 2, V(ow / 2 - r, oy0 + r, cz_top - 1))
    outlet = c0.fuse([c1, Part.makeBox(ow - ol, ol, P["outlet_h"] + 2, V(-ow / 2 + r, oy0, cz_top - 1))])
    body = body.cut(outlet)
    print(f"food outlet: X +-{ow/2:.1f}, Y {oy0:.1f}..{oy0+ol:.1f} (cave roof back wall at Y={cy_back:.1f})")

    # lid seam (0.6 mm groove, 0.5 mm deep) — cosmetic
    seam_z = cz_top + P["seam_gap"]
    seam = body_plan.extrude(V(0, 0, 0.6)); seam.translate(V(0, 0, seam_z - 0.3))
    m = FreeCAD.Matrix(); m.scale(V(1 - 1.0 / W, 1 - 1.0 / body_plan.BoundBox.YLength, 1))
    inner = seam.transformGeometry(m)
    inner.translate(V(0, (y_body_front + yB) / 2 * (1.0 / body_plan.BoundBox.YLength), 0))
    body = body.cut(seam.cut(inner))

    # front cosmetics (0.5 mm recesses); the front face is curved so cut 3 mm deep from ahead of it
    yf = y_body_front
    btn_z = cz_top + P["button_gap"] + P["button_d"] / 2
    led_z = cz_top + P["led_gap"] + P["led_d"] / 2
    win_z0 = led_z + P["led_d"] / 2 + P["window_gap"] + P["window_w"] / 2      # lower arc centre
    win_z1 = win_z0 + P["window_h"] - P["window_w"]                            # upper arc centre
    disp = Part.makeCylinder(P["led_d"] / 2, 3.5, V(0, yf - 3, led_z), V(0, 1, 0))
    slot = stadium_yz_cut(0, win_z0, win_z1, P["window_w"], yf - 2, 3.5)
    btn = Part.makeCylinder(P["button_d"] / 2, 3.3, V(0, yf - 3, btn_z), V(0, 1, 0))
    body = body.cut(disp).cut(slot).cut(btn)
    print(f"front features: button z={btn_z:.1f}, LED z={led_z:.1f}, window z[{win_z0 - P['window_w']/2:.1f},{win_z1 + P['window_w']/2:.1f}]")

    # ---- load-cell base ------------------------------------------------------
    lc_cy = (by0 + by1) / 2
    hs, rim = P["lc_size"] / 2, P["lc_rim"]
    lc = rounded_box(-hs, hs, lc_cy - hs, lc_cy + hs, plate_top, plate_top + P["lc_h"], P["lc_r"])
    recess = rounded_box(-hs + rim, hs - rim, lc_cy - hs + rim, lc_cy + hs - rim,
                         plate_top + P["lc_h"] - P["lc_recess"], plate_top + P["lc_h"] + 1, max(P["lc_r"] - rim, 1))
    lc = lc.cut(recess)

    # ---- bowl unit -----------------------------------------------------------
    z_lo = plate_top + P["bowl_float"]
    zt0 = plate_top + P["bowl_top_front"]
    zt1 = plate_top + P["bowl_top_back"]
    bowl = Part.Face(rrect_wire(-hw, hw, by0, by1, P["bowl_corner_r"], 0)).extrude(V(0, 0, 200))
    bowl.translate(V(0, 0, z_lo))
    # sloped top: keep everything below the plane through (by0, zt0)-(by1, zt1)
    slope = (zt1 - zt0) / (by1 - by0)
    keep = Part.Face(Part.makePolygon([V(0, by0 - 10, z_lo - 1), V(0, by1 + 10, z_lo - 1),
                                       V(0, by1 + 10, zt1 + slope * 10), V(0, by0 - 10, zt0 - slope * 10),
                                       V(0, by0 - 10, z_lo - 1)]))
    keep = keep.extrude(V(P["bowl_w"] + 2, 0, 0)); keep.translate(V(-hw - 1, 0, 0))
    bowl = bowl.common(keep)

    # stainless bowl cavity: lofted in a frame where the lip is horizontal at z=zt0,
    # then tilted onto the sloped top about the lip's front edge.
    lw, dep = P["lip_w"], P["bowl_depth"]
    lx, ly0, ly1 = hw - lw, by0 + lw, by1 - lw                 # lip inner edge
    y_deep = by0 + P["bowl_deep_from_front"]
    ins_f = bowl_inset(P["wall_angle_front"], y_deep - ly0, dep)
    ins_b = bowl_inset(P["wall_angle_back"], ly1 - y_deep, dep)
    ins_s = bowl_inset(P["wall_angle_side"], lx, dep)
    def cavity(off):
        """Lofted bowl cavity, grown outward by `off` (0 = steel inner surface)."""
        sections = [rrect_wire(-lx - off, lx + off, ly0 - off, ly1 + off, P["bowl_corner_r"] - lw + off, zt0 + 5)]
        for t in [0, 2, 5, 9, 14, 20, 26, 32, 37, 40.5, 42.4]:
            f, b, sd = ins_f(t), ins_b(t), ins_s(t)
            x1, ya, yb = lx - sd + off, ly0 + f - off, ly1 - b + off
            r = min(P["bowl_corner_r"] - lw + off, 0.9 * x1, 0.9 * (yb - ya) / 2)
            sections.append(rrect_wire(-x1, x1, ya, yb, r, zt0 - t - off))
        c = Part.makeLoft(sections, True, False)
        c.rotate(V(0, by0, zt0), V(1, 0, 0), math.degrees(math.atan(slope)))
        return c
    bowl = bowl.cut(cavity(0))

    # underside: hollow inside the outer skirt up to the pocket ceiling, leaving the
    # rectangular wall (stands on the base) and the steel bulge that hangs below the ceiling
    st, rt, ri = P["skirt_t"], P["rect_t"], P["rect_inset"]
    z_rim = plate_top + P["lc_h"]                      # base rim top = pad bottoms
    z_wall = z_rim - P["pad_lift"]                     # index wall bottom, inside the recess
    z_top = zt1 + 10                                   # above everything: wall/pads run up to the shell
    xo, yo0, yo1 = hw - ri + rt, by0 + ri - rt, by1 - ri + rt    # wall outer faces
    print(f"underside (above plate top): skirt bottom {z_lo - plate_top:.1f}, wall bottom {z_wall - plate_top:.1f}, "
          f"pads on rim at {z_rim - plate_top:.1f}; wall/recess play {(P['lc_size'] / 2 - P['lc_rim']) - xo:.2f} mm per side")
    hollow = Part.Face(rrect_wire(-hw + st, hw - st, by0 + st, by1 - st, P["bowl_corner_r"] - st, 0)).extrude(V(0, 0, z_top - z_lo + 1))
    hollow.translate(V(0, 0, z_lo - 1))
    rect_out = Part.Face(rrect_wire(-xo, xo, yo0, yo1, max(P["bowl_corner_r"] - ri + rt, 1), 0))
    rect_in = Part.Face(rrect_wire(-hw + ri, hw - ri, by0 + ri, by1 - ri, max(P["bowl_corner_r"] - ri, 1), 0))
    wall = rect_out.cut(rect_in).extrude(V(0, 0, z_top - z_wall)); wall.translate(V(0, 0, z_wall))
    pw, pt, ph = P["pad_w"] / 2, P["pad_t"], z_top - z_rim
    ym = (by0 + by1) / 2
    pads = [Part.makeBox(2 * pw, pt, ph, V(-pw, yo0 - pt, z_rim)), Part.makeBox(2 * pw, pt, ph, V(-pw, yo1, z_rim)),
            Part.makeBox(pt, 2 * pw, ph, V(xo, ym - pw, z_rim)), Part.makeBox(pt, 2 * pw, ph, V(-xo - pt, ym - pw, z_rim))]
    hollow = hollow.cut(wall).cut(Part.makeCompound(pads)).cut(cavity(P["shell_t"]))
    bowl = bowl.cut(hollow)

    return dict(BasePlate=plate, Feet=feet, Body=body, LoadCellBase=lc, BowlUnit=bowl)


def main():
    name = "XiaomiFeeder2"
    if name in [d.Name for d in FreeCAD.listDocuments().values()]:
        FreeCAD.closeDocument(name)
    doc = FreeCAD.newDocument(name)
    parts = build()
    colors = dict(BasePlate=(0.92, 0.92, 0.92), Feet=(0.5, 0.5, 0.5), Body=(0.96, 0.96, 0.96),
                  LoadCellBase=(0.6, 0.6, 0.65), BowlUnit=(0.75, 0.78, 0.8))
    for k, s in parts.items():
        o = doc.addObject("Part::Feature", k)
        o.Shape = s
        o.ViewObject.ShapeColor = colors[k]
        bb = s.BoundBox
        print(f"{k:10s} x[{bb.XMin:7.1f},{bb.XMax:7.1f}] y[{bb.YMin:7.1f},{bb.YMax:7.1f}] "
              f"z[{bb.ZMin:6.1f},{bb.ZMax:6.1f}] valid={s.isValid()}")

    doc.recompute()
    doc.saveAs(os.path.join(BASE, name + ".FCStd"))
    Part.export([doc.getObject(k) for k in parts], os.path.join(BASE, name + ".step"))
    print("saved", name)
    return doc

doc = main()
