"""Writer for the text DirectX .x subset the OpenXOPS GL loader accepts.

Grammar derived from src/d3dgraphics-opengl.cpp:468-721 (see
docs/ASSET-CONTRACT.md section 4.1). This module intentionally writes exactly
that subset: one vertex/face/UV per line, a single header, a single blank
separator line between vertices and the face count, column-0 ``Mesh`` and a
one-space-indented `` MeshTextureCoords``, plus a trivial single-material
``MeshMaterialList`` (contract item 9) so the D3D backend's per-material draw
path stays safe.

Deterministic formatting: every float is written with ``FLOAT_FMT`` (6 fixed
decimals), so re-running the generator byte-for-byte reproduces its output.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import List, Sequence, Tuple

Vec3 = Tuple[float, float, float]
Vec2 = Tuple[float, float]
Face = Tuple[int, ...]  # 3 or 4 indices

FLOAT_FMT = "{:.6f}"
HEADER = "xof 0302txt 0032"


def fmt(x: float) -> str:
    # Avoid "-0.000000" so output is stable regardless of float sign noise.
    v = float(x)
    if v == 0.0:
        v = 0.0
    s = FLOAT_FMT.format(v)
    if s == "-0.000000":
        s = "0.000000"
    return s


@dataclass
class Mesh:
    """One ``Mesh`` block: parallel vertex/uv arrays plus a face list.

    Faces reference vertex indices directly (no de-duplication is performed
    across primitives on purpose -- each primitive owns its own vertices so
    a per-face UV rect can be assigned without seams between unrelated
    faces). ``verts[i]`` corresponds to ``uvs[i]``.
    """

    verts: List[Vec3] = field(default_factory=list)
    uvs: List[Vec2] = field(default_factory=list)
    faces: List[Face] = field(default_factory=list)

    def add_vertex(self, p: Vec3, uv: Vec2) -> int:
        self.verts.append(tuple(p))
        self.uvs.append(tuple(uv))
        return len(self.verts) - 1

    def add_face(self, indices: Sequence[int]) -> None:
        n = len(indices)
        if n not in (3, 4):
            raise ValueError(f"face must have 3 or 4 indices, got {n}")
        for i in indices:
            if not (0 <= i < len(self.verts)):
                raise ValueError(f"face index {i} out of range 0..{len(self.verts)-1}")
        self.faces.append(tuple(indices))

    def add_quad_face(self, p0, uv0, p1, uv1, p2, uv2, p3, uv3) -> None:
        """Append 4 fresh vertices and the quad face referencing them."""
        i0 = self.add_vertex(p0, uv0)
        i1 = self.add_vertex(p1, uv1)
        i2 = self.add_vertex(p2, uv2)
        i3 = self.add_vertex(p3, uv3)
        self.add_face((i0, i1, i2, i3))

    def add_tri_face(self, p0, uv0, p1, uv1, p2, uv2) -> None:
        i0 = self.add_vertex(p0, uv0)
        i1 = self.add_vertex(p1, uv1)
        i2 = self.add_vertex(p2, uv2)
        self.add_face((i0, i1, i2))

    def extend(self, other: "Mesh") -> None:
        base = len(self.verts)
        self.verts.extend(other.verts)
        self.uvs.extend(other.uvs)
        for f in other.faces:
            self.faces.append(tuple(i + base for i in f))

    @property
    def poly_count(self) -> int:
        return len(self.faces)


def _face_line(face: Face, last: bool) -> str:
    n = len(face)
    idx = ",".join(str(i) for i in face)
    tail = ";;" if last else ";,"
    return f"{n};{idx}{tail}\n"


def _vertex_line(v: Vec3, last: bool) -> str:
    tail = ";;" if last else ";,"
    return f"{fmt(v[0])};{fmt(v[1])};{fmt(v[2])}{tail}\n"


def _uv_line(uv: Vec2, last: bool) -> str:
    tail = ";;" if last else ";,"
    return f"{fmt(uv[0])};{fmt(uv[1])}{tail}\n"


def write_x(path: str, mesh_name: str, meshes: Sequence[Mesh]) -> None:
    """Write one or more ``Mesh`` blocks (contract item 8: multiple meshes
    are allowed and simply concatenated) to ``path``."""
    lines = [HEADER + "\n", "\n"]
    for mi, mesh in enumerate(meshes):
        name = mesh_name if len(meshes) == 1 else f"{mesh_name}{mi}"
        nv = len(mesh.verts)
        nf = len(mesh.faces)
        lines.append(f"Mesh {name} {{\n")
        lines.append(f"{nv};\n")
        for i, v in enumerate(mesh.verts):
            lines.append(_vertex_line(v, i == nv - 1))
        lines.append("\n")  # the single unconditionally-skipped separator line
        lines.append(f"{nf};\n")
        for i, f in enumerate(mesh.faces):
            lines.append(_face_line(f, i == nf - 1))
        lines.append(" MeshTextureCoords {\n")
        lines.append(f"{nv};\n")
        for i, uv in enumerate(mesh.uvs):
            lines.append(_uv_line(uv, i == nv - 1))
        lines.append(" }\n")
        lines.append(" MeshMaterialList {\n")
        lines.append("1;\n")
        lines.append(f"{nf};\n")
        lines.append(",".join(["0"] * nf) + ";;\n" if nf else "0;;\n")
        lines.append(" Material Mat1 {\n")
        lines.append(" 1.000000;1.000000;1.000000;1.000000;;\n")
        lines.append(" 0.000000;\n")
        lines.append(" 0.000000;0.000000;0.000000;;\n")
        lines.append(" 0.000000;0.000000;0.000000;;\n")
        lines.append(" }\n")
        lines.append(" }\n")
        lines.append("}\n")
    with open(path, "w", encoding="ascii", newline="\n") as fh:
        fh.writelines(lines)


def render_x(mesh_name: str, meshes: Sequence[Mesh]) -> str:
    """Same as write_x but returns text (used by tests for determinism)."""
    import io
    import tempfile
    import os

    fd, tmp = tempfile.mkstemp()
    os.close(fd)
    try:
        write_x(tmp, mesh_name, meshes)
        with open(tmp, "r", encoding="ascii") as fh:
            return fh.read()
    finally:
        os.remove(tmp)
