# Mission list (data-driven)

The menu's standard-mission list is built once at first use
(`ParameterInfo::GetOfficialMissionCount`, src/parameter.cpp):

1. Each of the 58 built-in official entries is kept only if its
   `<directory>temp.bd1` and `<directory><txt>.pd1` exist in the mounted data.
2. Then every valid line of `data/missions.txt` (if present) is appended, with
   the same file-existence check.

So with only the classic xops096.zip the list is exactly the 58 originals; with
only the free pack it is the free missions; with both it is originals followed
by free missions.

`data/missions.txt` format (UTF-8/ASCII, `#` or `;` comments):

    name|fullname|directory|txtbase|collision(0/1)|screen(0/1)
    UFO FIELD|UFO FIELD - alien wave defence|data\ufofield\|ufo.mif|0|0
    STARTER|STARTER compound|data\starter\|start|0|0

- name ≤ 30 bytes (menu label), fullname ≤ 60, txtbase ≤ 30.
- Map: `<directory>temp.bd1`; points `<directory><txtbase>.pd1`; briefing
  `<directory><txtbase>.txt`.
- If txtbase ends in `.mif`, the entry is a full MIF mission shown in the main
  list: `<directory><txtbase>` must exist, and the MIF supplies the map, points,
  sky, flags, briefing pictures and optional addon prop. This keeps free
  missions out of the ADD-ON list, which remains for player-installed addons.

Data layering in the browser (src/platform/portable/browser-archive.cpp):
bundled free pack `/openxops-data.zip` first, optional classic `/xops096.zip`
over it (classic files override free ones), exported to the engine via
`OPENXOPS_DATA_ZIP`.

Menu background: the main menu loads a random classic demo
(`data\map{2,4,5,7,8,16}\temp.bd1` + `demo.pd1`). If none is present it falls
back to `data\menu\temp.bd1` + `data\menu\demo.pd1`, which the free pack
provides (currently a copy of the map10 opening scene).
