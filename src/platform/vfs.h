//! @file vfs.h
//! @brief [OpenXOPS-web] Virtual file system: every game-data read and every
//!        save-file write goes through here (SCOPE-OF-WORK §7).
//!
//! Model
//!  - Layers ("mounts") are host directories or zip archives. Each layer adds
//!    its files to one index keyed by the NORMALISED PATH (raw bytes, see
//!    vfs_normalize()). A later mount overrides an earlier one per key
//!    (free pack → xops096.zip → addons → my edits; GOALS.md G3).
//!  - Reads (mode without 'w', 'a', '+') look in the SAVE ROOT first, then in
//!    the index. Writes always go to the save root (§7.2).
//!  - Files come back as ordinary FILE*, so upstream fread/fgets/fseek code is
//!    unchanged. Zip entries are served by fmemopen(); directory entries are
//!    opened on the host with fopen().
//!
//! Single-threaded, like the game. Not reentrant.

// Copyright (c) 2026, OpenXOPS-web contributors. BSD-3-Clause, same terms as OpenXOPS.

#ifndef OPENXOPS_PLATFORM_VFS_H
#define OPENXOPS_PLATFORM_VFS_H

#include <stddef.h>
#include <stdio.h>

#ifndef MAX_PATH
 #define MAX_PATH 260
#endif

//! Return codes of vfs_mount_zip_*().
enum {
	VFS_OK = 0,
	VFS_ERR_IO = -1,			//!< host file could not be opened/read
	VFS_ERR_CORRUPT = -2,		//!< not a zip, or its directory/headers are damaged. Nothing was mounted.
	VFS_ERR_UNSUPPORTED = -3,	//!< zip64 / multi-disk archive. Nothing was mounted.
	VFS_ERR_NOMEM = -4,			//!< allocation failed
	VFS_ERR_ARG = -5			//!< NULL/empty argument
};

//! @brief Mount a host directory tree as a layer.
//! The tree is scanned once now (files added later are not seen; the save
//! root is the exception, it is checked on every read). Symlinks are followed,
//! loops are skipped. Host names that are valid non-ASCII UTF-8 are indexed by
//! their CP932 bytes when representable (a Linux unzip usually writes UTF-8).
//! @param hostdir directory (relative paths are resolved with realpath() now)
//! @param label   name used in logs and vfs_debug_resolve(); may be NULL
//! @return true if the directory was readable and mounted (even when empty)
bool vfs_mount_dir(const char* hostdir, const char* label);

//! @brief Mount a zip archive held in memory as a layer.
//! The VFS COPIES the bytes: the caller may free `data` as soon as this returns
//! (on web the loader can drop its JS/heap copy). Supported: stored (0) and
//! deflate (8) entries. Entries with any other method, or encrypted ones, are
//! skipped with a log line ("unsupported compression method N") and are not
//! indexed. Directory entries are not files. A single common top-level folder
//! is stripped when it wraps the game layout (see vfs.cpp: StripPrefix).
//! Payloads are CRC/inflate-checked before publication. Limits per archive:
//! 256 MiB per entry, 512 MiB total uncompressed and archive bytes.
//! @return VFS_OK, or VFS_ERR_* (on error nothing is mounted)
int vfs_mount_zip_memory(const unsigned char* data, size_t len, const char* label);

//! @brief Read a host zip file into memory and mount it (see vfs_mount_zip_memory()).
int vfs_mount_zip_file(const char* hostpath, const char* label);

//! @brief Drop every layer and the index. Does NOT reset the save root or hook.
//! FILE* handles from earlier vfs_fopen() calls on zip entries must be closed first.
void vfs_unmount_all();

//! @brief fopen() replacement.
//! - mode contains 'w', 'a' or '+': opens save_root/<normalised path> on the
//!   host, creating parent directories (§7.2).
//! - otherwise (read): save_root/<normalised path> if it exists (so config.dat
//!   written by the game round-trips), else the index (last mounted layer
//!   wins), else NULL. A 't' in the mode is ignored; reads are always binary.
//! - an absolute READ path ('/'...) bypasses the VFS and is opened on the host
//!   (native command-line .mif paths). Absolute writes are rejected. Save-path
//!   components never follow symlinks; attempts to escape the root fail.
//! Initialises the VFS with defaults on first use (vfs_init_from_args(NULL)).
FILE* vfs_fopen(const char* path, const char* mode);

//! @brief List files directly inside `dir` whose name ends with `ext`
//!        (ASCII case-insensitive; NULL or "" = all). Replaces the
//!        FindFirstFile("dir\\*.mif") scan.
//! Layers are merged (one name per key, case of the winning layer), sorted by
//! key, and at most `max` names are written. Names that do not fit MAX_PATH
//! are skipped. Only the index is listed, not the save root.
//! @return number of names written to out
int vfs_list(const char* dir, const char* ext, char out[][MAX_PATH], int max);

//! @brief Set the host directory for writes and for read-precedence.
//! Native default: $XDG_DATA_HOME/openxops-web (or ~/.local/share/openxops-web).
//! Web default: /save (IDBFS, mounted by the shell). Created on first write.
void vfs_set_save_root(const char* hostdir);

//! @brief Callback run after every fclose() of a file opened for writing
//!        through vfs_fopen(). Web plugs the debounced FS.syncfs(false) in here.
//!        NULL removes the hook. With a hook set, write handles are
//!        fopencookie() wrappers around the host FILE.
void vfs_set_save_hook(void (*after_close)(void));

//! @brief Which layer would serve `path` for reading, for logs.
//! @return e.g. "save:/home/u/.local/share/openxops-web/config.dat",
//!         "zip:xops096.zip:data/model/soldier0.bmp", "dir:data:/srv/x/data/..";
//!         NULL if not found. Points to a static buffer (next call overwrites).
const char* vfs_debug_resolve(const char* path);

// ---- extensions beyond the §7 API ----

//! @brief Set up the default layers once. Later calls are no-ops.
//! 1. vfs_mount_dir(datadir ? datadir : ".")   ("." = the cwd after -Dir chdir)
//! 2. every zip in $OPENXOPS_DATA_ZIP (':'-separated, mounted in order)
//! 3. save root = $OPENXOPS_SAVE_DIR if set, else the platform default
//! Called lazily by vfs_fopen()/vfs_list()/vfs_dir_exists(); main.cpp should
//! call it explicitly right after argument parsing (after the -Dir chdir).
void vfs_init_from_args(const char* datadir);

//! @brief True if any indexed file lives under `dir`, or `dir` exists on the
//!        host (relative to the cwd) or in the save root. Used by CheckDirectory().
bool vfs_dir_exists(const char* dir);

//! @brief Normalise a game path into an index key (§7):
//!  strip "./" ".\", walk as CP932 (lead 0x81–0x9F/0xE0–0xFC take the next
//!  byte) turning a lone 0x5C into '/' and lowercasing single-byte 'A'..'Z'
//!  only, collapse "//", drop "." segments, resolve ".." without escaping the
//!  root (escape attempts fail), drop leading/trailing '/'.
//! @param keep_case true = skip the lowercasing (gives the display name)
//! @return false for invalid arguments, traversal above root, or if the result
//!         does not fit (outsize includes the NUL)
bool vfs_normalize(const char* path, char* out, size_t outsize, bool keep_case);

//! @brief Call fn(key, display_name, layer_label, size, user) for every indexed
//!        file (winning layer only), in key order. For tests and pack export.
//! @return number of files
int vfs_enumerate(void (*fn)(const char* key, const char* name, const char* layer, size_t size, void* user), void* user);

#endif	// OPENXOPS_PLATFORM_VFS_H
