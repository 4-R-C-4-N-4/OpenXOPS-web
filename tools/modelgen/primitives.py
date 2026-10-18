"""Low-poly solid primitives for modelgen.

All primitives emit quads (and triangle fans for cylinder caps) with outward
winding that satisfies docs/ASSET-CONTRACT.md section 4.2: for every face,
the ordinary cross product ``(v1-v0) x (v2-v0)`` must point away from the
solid's own centre (the same check ``tools/contract/check_x.py --closed``
performs). ``fix_winding`` enforces this automatically so callers can list
corners in any convenient order.

Coordinate convention used throughout this package (matches the contract):
left-handed, +Y up, model "front" is -Z, centred on the Y axis (x=z=0 is the
body axis) unless a part's spec says otherwise.
"""
from __future__ import annotations

from typing import List, Sequence, Tuple

from .xwriter import Mesh, Vec3

Rect = Tuple[float, float, float, float]  # u0, v0, u1, v1 in 0..1


def _sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def _cross(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def _dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def _avg(points: Sequence[Vec3]) -> Vec3:
    n = len(points)
    return (
        sum(p[0] for p in points) / n,
        sum(p[1] for p in points) / n,
        sum(p[2] for p in points) / n,
    )


def fix_winding(points: List[Vec3], center: Vec3) -> List[Vec3]:
    """Reorder a planar quad/tri so it is visible from outside ``center``."""
    p0, p1, p2 = points[0], points[1], points[2]
    n = _cross(_sub(p1, p0), _sub(p2, p0))
    fc = _avg(points)
    if _dot(n, _sub(fc, center)) < 0:
        return list(reversed(points))
    return points


def add_quad(mesh: Mesh, points: List[Vec3], rect: Rect, center: Vec3) -> None:
    """Add a quad whose 4 corners map to the 4 corners of ``rect`` in the
    same order (after any winding fix applies the same permutation)."""
    u0, v0, u1, v1 = rect
    uvs = [(u0, v0), (u1, v0), (u1, v1), (u0, v1)]
    fixed = fix_winding(points, center)
    if fixed != points:
        uvs = list(reversed(uvs))
        # reversed() on a 4-list keeps the "pairing" with reversed(points)
        # rotated by one; realign so corner 0 of fixed still maps near uv0.
        uvs = [uvs[-1]] + uvs[:-1]
    mesh.add_quad_face(fixed[0], uvs[0], fixed[1], uvs[1], fixed[2], uvs[2], fixed[3], uvs[3])


def box(
    center: Vec3,
    size: Vec3,
    uv: dict,
) -> Mesh:
    """Axis-aligned box. ``uv`` maps face name -> Rect for any subset of
    {"front","back","left","right","top","bottom"}; omitted faces are
    skipped (useful for faces that are never seen, saving polygons)."""
    cx, cy, cz = center
    sx, sy, sz = size
    hx, hy, hz = sx / 2, sy / 2, sz / 2
    m = Mesh()

    def c(dx, dy, dz):
        return (cx + dx * hx, cy + dy * hy, cz + dz * hz)

    faces = {
        "front": [c(-1, 1, -1), c(1, 1, -1), c(1, -1, -1), c(-1, -1, -1)],
        "back": [c(1, 1, 1), c(-1, 1, 1), c(-1, -1, 1), c(1, -1, 1)],
        "left": [c(-1, 1, 1), c(-1, 1, -1), c(-1, -1, -1), c(-1, -1, 1)],
        "right": [c(1, 1, -1), c(1, 1, 1), c(1, -1, 1), c(1, -1, -1)],
        "top": [c(-1, 1, 1), c(1, 1, 1), c(1, 1, -1), c(-1, 1, -1)],
        "bottom": [c(-1, -1, -1), c(1, -1, -1), c(1, -1, 1), c(-1, -1, 1)],
    }
    for name, rect in uv.items():
        if name in faces:
            add_quad(m, faces[name], rect, center)
    return m


def tapered_box(
    base_center: Vec3,
    base_size: Tuple[float, float],
    top_center: Vec3,
    top_size: Tuple[float, float],
    uv: dict,
) -> Mesh:
    """A box-like frustum: a rectangular base and a rectangular top of
    (possibly) different width/depth, joined by 4 side quads. Used for
    tapered limbs (e.g. upper arm thicker than forearm)."""
    bx, by, bz = base_center
    bw, bd = base_size
    tx, ty, tz = top_center
    tw, td = top_size
    # Rings span the plane perpendicular to the center-to-center axis. Limb
    # generators pass equal Y coordinates and separated Z coordinates, so
    # choose a Y-oriented cross-section for their longitudinal -Z axis.
    axis = (tx - bx, ty - by, tz - bz)
    longitudinal = max(range(3), key=lambda i: abs(axis[i]))
    transverse = [i for i in range(3) if i != longitudinal]
    m = Mesh()
    center = ((bx + tx) / 2, (by + ty) / 2, (bz + tz) / 2)

    def ring(cx, cy, cz, width, depth):
        points = []
        for s0, s1 in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            p = [cx, cy, cz]
            p[transverse[0]] += s0 * width / 2
            p[transverse[1]] += s1 * depth / 2
            points.append(tuple(p))
        return points

    b_fl, b_fr, b_br, b_bl = ring(bx, by, bz, bw, bd)
    t_fl, t_fr, t_br, t_bl = ring(tx, ty, tz, tw, td)

    faces = {
        "front": [t_fl, t_fr, b_fr, b_fl],
        "back": [t_br, t_bl, b_bl, b_br],
        "left": [t_bl, t_fl, b_fl, b_bl],
        "right": [t_fr, t_br, b_br, b_fr],
        "top": [t_bl, t_br, t_fr, t_fl],
        "bottom": [b_fl, b_fr, b_br, b_bl],
    }
    for name, rect in uv.items():
        if name in faces:
            add_quad(m, faces[name], rect, center)
    return m


def cylinder(
    center: Vec3,
    radius: float,
    height: float,
    segments: int,
    side_uv: Rect,
    cap_uv: Rect = None,
    caps: bool = True,
) -> Mesh:
    """Low-segment vertical cylinder (axis = Y), centred at ``center``.

    Side faces are quads (one UV column per segment inside ``side_uv``),
    caps are triangle fans (the engine only allows tri/quad faces, so an
    n-gon cap must be fanned, contract item 6)."""
    import math

    cx, cy, cz = center
    m = Mesh()
    top_y = cy + height / 2
    bot_y = cy - height / 2
    ring_center = center

    pts_top = []
    pts_bot = []
    for i in range(segments):
        a = 2 * math.pi * i / segments
        x = cx + radius * math.sin(a)
        z = cz + radius * math.cos(a)
        pts_top.append((x, top_y, z))
        pts_bot.append((x, bot_y, z))

    u0, v0, u1, v1 = side_uv
    for i in range(segments):
        j = (i + 1) % segments
        quad = [pts_top[i], pts_top[j], pts_bot[j], pts_bot[i]]
        ua = u0 + (u1 - u0) * i / segments
        ub = u0 + (u1 - u0) * (i + 1) / segments
        uvs = [(ua, v0), (ub, v0), (ub, v1), (ua, v1)]
        fixed = fix_winding(quad, ring_center)
        if fixed != quad:
            uvs = list(reversed(uvs))
            uvs = [uvs[-1]] + uvs[:-1]
        m.add_quad_face(fixed[0], uvs[0], fixed[1], uvs[1], fixed[2], uvs[2], fixed[3], uvs[3])

    if caps:
        cu0, cv0, cu1, cv1 = cap_uv or side_uv
        cuv = ((cu0 + cu1) / 2, (cv0 + cv1) / 2)
        top_center_pt = (cx, top_y, cz)
        bot_center_pt = (cx, bot_y, cz)
        for i in range(segments):
            j = (i + 1) % segments
            tri = [top_center_pt, pts_top[i], pts_top[j]]
            fixed = fix_winding(tri, center)
            m.add_tri_face(fixed[0], cuv, fixed[1], cuv, fixed[2], cuv)
            tri = [bot_center_pt, pts_bot[j], pts_bot[i]]
            fixed = fix_winding(tri, center)
            m.add_tri_face(fixed[0], cuv, fixed[1], cuv, fixed[2], cuv)
    return m


def flip_mesh(mesh: Mesh) -> Mesh:
    """Reverse every face's winding in place (vertices/UVs untouched).

    Used for shapes that must be visible from the inside (the sky box,
    §7: "faces must be visible from inside -- wind them clockwise as seen
    from the origin")."""
    mesh.faces = [tuple(reversed(f)) for f in mesh.faces]
    return mesh


def translate(mesh: Mesh, offset: Vec3) -> Mesh:
    dx, dy, dz = offset
    mesh.verts = [(x + dx, y + dy, z + dz) for (x, y, z) in mesh.verts]
    return mesh


def rotate_x_about(mesh: Mesh, pivot: Vec3, angle_rad: float) -> Mesh:
    """Rotate every vertex about the X axis through ``pivot`` (used to pose
    legs/arms while keeping topology -- and hence morph compatibility --
    identical across keyframes)."""
    import math

    px, py, pz = pivot
    c, s = math.cos(angle_rad), math.sin(angle_rad)
    out = []
    for x, y, z in mesh.verts:
        y0, z0 = y - py, z - pz
        y1 = y0 * c - z0 * s
        z1 = y0 * s + z0 * c
        out.append((x, y1 + py, z1 + pz))
    mesh.verts = out
    return mesh


def merge(*meshes: Mesh) -> Mesh:
    out = Mesh()
    for mesh in meshes:
        out.extend(mesh)
    return out
