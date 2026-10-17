# VFS component and deferred integration

`openxops_vfs` is a standalone C++17 static library, linked to `openxops`
without redirecting any game I/O. Its module finds zlib and iconv and registers
CTest tests. No game/main/backend call sites have changed in this component.

## Contract and safety

- Keys are CP932-aware raw bytes: ASCII-only case folding, standalone backslash
  conversion, dot removal, repeated separator collapse. A `..` escaping the
  virtual root fails. A CP932 trail byte equal to backslash or ASCII uppercase
  is never rewritten. Host UTF-8 filenames are converted to CP932 when possible;
  unflagged ZIP names remain raw bytes.
- Directory and ZIP mounts merge into one ordered index, last entry/mount wins.
  Listing is immediate-child-only, deduplicated, sorted by key, using winning
  display case. Saves are intentionally not part of `vfs_list`.
- Stored/deflate ZIP payloads are fully CRC/inflate-validated before publication;
  a damaged mount leaves the previous index unchanged. Local/central metadata,
  bounds, compressed input consumption and output sizes are checked. ZIP64 and
  multi-disk archives are unsupported; encrypted/other compression entries skip
  with a diagnostic. Archive bytes and total declared uncompressed size are each
  capped at 512 MiB; each entry is capped at 256 MiB. Deflated data is temporarily
  expanded during validation, then cached only when opened. Limits are per mount,
  not a global quota over arbitrarily many trusted mount requests.
- Reads check the save root before mounted data. Relative writes (including `+`)
  stay under the save root, creating parents. Descriptor-relative `openat` with
  `O_NOFOLLOW` prevents save-path symlink escapes. The configured save root is
  trusted. Absolute native reads are an explicit host-file escape hatch for CLI
  mission paths; absolute writes fail. This is not a sandbox for untrusted host
  directory mounts: those deliberately follow symlinks with inode loop detection.
- ZIP `FILE*` objects refer into mount-owned memory: close them **before** unmount.
  Mounts, save hook and index are single-threaded. Save hook fires on write-handle
  close, not every write; debounce and IDBFS initialization belong to the web shell.

## Exact future call sites (baseline 0548668)

1. `src/main.cpp`, immediately after argument parsing / `-Dir` chdir (line 162),
   before `OutputLog.MakeLog` (166), `CheckDirectory` (205/213), and config load:
   explicitly call `vfs_init_from_args(NULL)`. Loader-controlled web mounts should
   happen before starting game execution; handle `VFS_ERR_CORRUPT` as fatal there.
2. `src/config.cpp`: replace `fopen` at 84/165 (`Config::LoadFile`/`SaveFile`) and
   529/540 (`CreateExtConfig`). Audit `LoadExtFile`'s stream-based reader as well;
   adapting only fopen will not virtualize that reader.
3. `src/datafile.cpp`: replace fopen at 128, 441, 518, 816, 886, 986, 1445
   (`BlockDataInterface::LoadFiledata`, `PointDataInterface::LoadFiledata`,
   `LoadMSGFiledata`, `MIFInterface::LoadDefaultTextFiledata`,
   `LoadMissionInfoFiledata`, `LoadAddSmallObjectFiledata`,
   `INIFileInterface::LoadINIFile`). Replace the entire platform scan in
   `AddonList::GetMIFlist` (1258–1309) with bounded `vfs_list(dir,".mif",...)`;
   preserve/reset `datas` intentionally. Replace `CheckDirectory` at 1768 with
   `vfs_dir_exists`.
4. `src/debug.cpp`: fopen at 55/100/217 (`DebugLog` constructor, `MakeLog`,
   `WriteLog`). **Do not allow a static/global constructor to trigger lazy VFS
   initialization before main parses `-Dir`**: defer constructor I/O until the
   explicit initialization point.
5. Native backend reads: `platform/stub/d3dgraphics-stub.cpp:21`,
   `platform/stub/sound-stub.cpp:66`. Future GLES/OpenAL loaders must read through
   `vfs_fopen` (including any library decoder that otherwise accepts host paths).
   Retained legacy backends have reads in `d3dgraphics-opengl.cpp` at
   502/888/1057/1238/1322/1413 and `sound-directsound.cpp:243`, and screenshot
   writes at `d3dgraphics-opengl.cpp:3201` / `d3dgraphics-directx.cpp:898`.
6. Disable `OPENXOPS_PATH_DELIMITER_SLASH` when integrating: its current default
   remains ON, and existing `ChangePathDelimiter` calls corrupt CP932 trail 0x5c
   **before** the VFS can see the original bytes. Remove/bypass those calls on VFS
   paths; do not perform blanket byte replacement in the new backends.
7. Web shell: mount/sync IDBFS `/save` before startup, set save hook to schedule
   debounced `FS.syncfs(false)` (500 ms), then unmount only after all streams close.

## Reproducible verification

```
cmake -S . -B build-vfs -DOPENXOPS_SANITIZE=ON
cmake --build build-vfs -j4
ctest --test-dir build-vfs --output-on-failure
```

`vfs` synthesizes its own stored/deflate fixtures and checks CP932 normalization,
layering/listing, CRC/header/truncation failures, limits, wrapper stripping,
save precedence/hooks and traversal/symlink rejection. It uses `$TMPDIR` (or the
working directory), removes successful test trees, and never writes real saves.
`vfs_real_pack` is registered only when local `xopsdata/xops096.zip` exists; it
mounts, CRC-checks, opens and reads every indexed entry. The archive and xopsdata
symlink must remain local and must not be staged. No redistributed game assets
are required for the ordinary test.
