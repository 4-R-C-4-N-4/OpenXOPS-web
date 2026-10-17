# VFS integration

`openxops_vfs` is a C++17 static library linked to `openxops`. Game data/config/log reads and writes now use its `FILE*` wrappers; ZIP mounts therefore require no extracted `data` tree. The module finds zlib and iconv and registers CTest tests.

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
  trusted; archive paths (including directory entries) are untrusted and rejected
  if they traverse above the virtual root. Absolute native reads are an explicit host-file escape hatch for CLI
  mission paths; absolute writes fail. This is not a sandbox for untrusted host
  directory mounts: those deliberately follow symlinks with inode loop detection.
- ZIP `FILE*` objects refer into mount-owned memory: close them **before** unmount.
  Mounts, save hook and index are single-threaded. Save hook fires on write-handle
  close, not every write; debounce and IDBFS initialization belong to the web shell.

## Integrated game call sites and remaining backend work

- `src/main.cpp` calls `vfs_init_from_args(NULL)` after parsing arguments and applying `-Dir`, before logging, data checks, or config reads. `OPENXOPS_DATA_ZIP` supplies colon-separated ZIP paths; the default mount is the post-`-Dir` working directory. ZIP mount failures are logged by the VFS; web-shell mounts must be ready before startup.
- `src/config.cpp` uses `vfs_fopen` for config/INI reads and save writes. `src/datafile.cpp` uses it for block, point, message, mission, addon-info, and INI reads; addon enumeration uses bounded `vfs_list`, and `CheckDirectory` uses `vfs_dir_exists`. `src/debug.cpp` routes log files through `vfs_fopen`; its global constructor performs no I/O.
- Do not enable `OPENXOPS_PATH_DELIMITER_SLASH` transformation: blanket conversion can corrupt CP932 trail byte 0x5c. The CMake option is retained for compatibility but no longer defines that transformation for the VFS-connected target.
- Renderer/audio backend loaders still need to use `vfs_fopen` (including decoder libraries currently passed host paths). Legacy backend reads and screenshot writes are not integrated. The web shell owns `/save` IDBFS setup and the debounced save hook.

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
