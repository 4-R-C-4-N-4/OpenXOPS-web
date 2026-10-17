//! @file vfs.cpp
//! @brief [OpenXOPS-web] Virtual file system (SCOPE-OF-WORK §7). See vfs.h.
//!
//! Memory strategy
//!  - A zip layer keeps ONE copy of the archive bytes (5 MB for xops096.zip).
//!  - Stored entries are served straight from that copy (no extra bytes).
//!  - Every payload is validated at mount; the temporary inflation is discarded.
//!    Deflated entries are inflated again on first open, CRC-checked, and the
//!    result is cached for the life of the mount. FILE* handles from fmemopen()
//!    point into the cache, so it is never evicted while mounted. The cache is
//!    bounded by the uncompressed size of the WINNING entries that the game
//!    actually opens (xops096.zip: at most ~9.6 MB); overridden entries are
//!    dropped from the index and never inflated. Each entry is capped at
//!    VFS_MAX_ENTRY_SIZE so a hostile archive cannot ask for gigabytes.
//!  - Directory layers hold only names; files are opened on the host.

// Copyright (c) 2026, OpenXOPS-web contributors. BSD-3-Clause, same terms as OpenXOPS.

#ifndef _GNU_SOURCE
 #define _GNU_SOURCE	// fmemopen(), fopencookie()
#endif

#include "vfs.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <iconv.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <zlib.h>

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#define VFS_MAX_ARCHIVE_SIZE (512u * 1024u * 1024u)
#define VFS_MAX_ENTRY_SIZE (256u * 1024u * 1024u)	//!< largest uncompressed zip entry accepted
#define VFS_MAX_DIR_FILES 200000					//!< directory-scan cap (mounting "/" by mistake)
#define VFS_MAX_DIR_DEPTH 32
#define SAVE_SYNC_DEBOUNCE_MS 500					//!< §9; the debounce itself lives in the web hook

namespace {

enum LayerKind { LAYER_DIR, LAYER_ZIP };

struct Layer {
	LayerKind kind;
	std::string label;
	std::string hostroot;				//!< LAYER_DIR: absolute host directory
	std::vector<unsigned char> bytes;	//!< LAYER_ZIP: the archive copy
};

struct Entry {
	int layer;					//!< index into g_layers
	std::string name;			//!< normalised path with original case (display name)
	// LAYER_DIR
	std::string hostpath;
	// LAYER_ZIP
	uint16_t method;
	uint32_t crc;
	uint32_t csize;
	uint32_t usize;
	size_t dataofs;				//!< offset of the entry's data in Layer::bytes
	bool verified;				//!< CRC checked (stored entries) / inflated OK
	std::unique_ptr<unsigned char[]> inflated;
};

std::vector<std::unique_ptr<Layer>> g_layers;
std::map<std::string, Entry> g_index;	//!< key = normalised lower-case bytes; ordered for vfs_list
bool g_initialised = false;
std::string g_save_root;					//!< empty = platform default (resolved lazily)
void (*g_save_hook)(void) = NULL;
int g_trace = -1;							//!< OPENXOPS_VFS_TRACE: log every open to stderr

template<class... Args>
void Log(const char* fmt, Args... args)
{
	fprintf(stderr, "[vfs] ");
	fprintf(stderr, fmt, args...);
	fprintf(stderr, "\n");
}

bool Trace()
{
	if( g_trace < 0 ){
		const char* e = getenv("OPENXOPS_VFS_TRACE");
		g_trace = (e != NULL && e[0] != '\0' && e[0] != '0') ? 1 : 0;
	}
	return g_trace == 1;
}

inline bool IsLead(unsigned char c)
{
	return (0x81 <= c && c <= 0x9F) || (0xE0 <= c && c <= 0xFC);
}

std::string Normalize(const char* path, bool keep_case, bool* valid = NULL)
{
	if( valid ){ *valid = true; }
	std::string flat;
	flat.reserve(strlen(path));
	// 1. CP932 walk: a lone 0x5C becomes '/', single-byte A-Z is lowercased.
	//    A double-byte character (lead + trail) is copied untouched; trail bytes
	//    can be 0x5C ('表' 95 5C) or 0x41-0x5A ('ア' 83 41).
	for(size_t i=0; path[i] != '\0'; i++){
		unsigned char c = (unsigned char)path[i];
		if( IsLead(c) && path[i+1] != '\0' ){
			flat += (char)c;
			flat += path[i+1];
			i++;
			continue;
		}
		if( c == 0x5C ){ c = '/'; }
		else if( !keep_case && 'A' <= c && c <= 'Z' ){ c = (unsigned char)(c - 'A' + 'a'); }
		flat += (char)c;
	}
	// 2. Segments: drop "" (collapses "//", leading/trailing '/') and ".",
	//    ".." pops but never above the root. '/' and '.' are never CP932 trail
	//    bytes (trail range 0x40-0xFC), so splitting on them bytewise is safe.
	std::vector<std::string> segs;
	size_t start = 0;
	while( start <= flat.size() ){
		size_t end = flat.find('/', start);
		if( end == std::string::npos ){ end = flat.size(); }
		std::string seg = flat.substr(start, end - start);
		if( seg.empty() || seg == "." ){
			// skip
		}
		else if( seg == ".." ){
			if( !segs.empty() ){ segs.pop_back(); }
			else { if( valid ){ *valid = false; } return std::string(); }
		}
		else{
			segs.push_back(seg);
		}
		start = end + 1;
	}
	std::string out;
	for(size_t i=0; i<segs.size(); i++){
		if( i ){ out += '/'; }
		out += segs[i];
	}
	return out;
}

std::string LowerAscii(const std::string& s)
{
	// for already-normalised display names: same CP932-aware rule as Normalize
	std::string o = s;
	for(size_t i=0; i<o.size(); i++){
		unsigned char c = (unsigned char)o[i];
		if( IsLead(c) && i+1 < o.size() ){ i++; continue; }
		if( 'A' <= c && c <= 'Z' ){ o[i] = (char)(c - 'A' + 'a'); }
	}
	return o;
}

bool IsAscii(const std::string& s)
{
	for(unsigned char c : s){ if( c >= 0x80 ){ return false; } }
	return true;
}

bool IsValidUtf8(const std::string& s)
{
	size_t i = 0, n = s.size();
	while( i < n ){
		unsigned char c = (unsigned char)s[i];
		int len = (c < 0x80) ? 1 : ((c >> 5) == 0x6) ? 2 : ((c >> 4) == 0xE) ? 3 : ((c >> 3) == 0x1E) ? 4 : 0;
		if( len == 0 || i + len > n ){ return false; }
		for(int k=1; k<len; k++){ if( ((unsigned char)s[i+k] >> 6) != 0x2 ){ return false; } }
		i += len;
	}
	return true;
}

//! UTF-8 → CP932 bytes. false if iconv lacks CP932 or a character is not representable.
bool Utf8ToCp932(const std::string& in, std::string& out)
{
	iconv_t cd = iconv_open("CP932", "UTF-8");
	if( cd == (iconv_t)-1 ){ return false; }
	std::vector<char> buf(in.size() * 2 + 4);
	char* ip = const_cast<char*>(in.data());
	size_t il = in.size();
	char* op = buf.data();
	size_t ol = buf.size();
	size_t r = iconv(cd, &ip, &il, &op, &ol);
	iconv_close(cd);
	if( r == (size_t)-1 || il != 0 ){ return false; }
	out.assign(buf.data(), buf.size() - ol);
	return true;
}

//! Host / UTF-8-flagged names: index by CP932 bytes when possible.
std::string ToGameBytes(const std::string& name, bool utf8_flag, const char* label)
{
	if( IsAscii(name) ){ return name; }
	if( !utf8_flag && !IsValidUtf8(name) ){ return name; }	// already CP932 (or other legacy bytes)
	if( !utf8_flag ){
		// Host names: valid UTF-8 is far more likely UTF-8 than CP932 that
		// happens to validate, so transcode.
	}
	std::string cp;
	if( Utf8ToCp932(name, cp) ){ return cp; }
	Log("%s: name '%s' is UTF-8 and not representable in CP932; kept as UTF-8", label, name.c_str());
	return name;
}

void AddEntry(Entry&& e)
{
	std::string key = LowerAscii(e.name);
	auto it = g_index.find(key);
	if( it != g_index.end() ){
		it->second = std::move(e);	// last mounted wins
	}
	else{
		g_index.emplace(std::move(key), std::move(e));
	}
}

// ---------------------------------------------------------------- directories

void ScanDir(const std::string& host, const std::string& rel, int depth, int layer, std::set<std::pair<dev_t, ino_t>>& seen, int& count)
{
	if( depth > VFS_MAX_DIR_DEPTH ){ return; }
	DIR* d = opendir(host.c_str());
	if( d == NULL ){ return; }
	struct dirent* de;
	while( (de = readdir(d)) != NULL ){
		if( strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0 ){ continue; }
		std::string hp = host + "/" + de->d_name;
		struct stat st;
		if( stat(hp.c_str(), &st) != 0 ){ continue; }	// dangling symlink
		std::string gamename = ToGameBytes(de->d_name, false, g_layers[layer]->label.c_str());
		std::string rp = rel.empty() ? gamename : rel + "/" + gamename;
		if( S_ISDIR(st.st_mode) ){
			if( !seen.insert(std::make_pair(st.st_dev, st.st_ino)).second ){ continue; }	// symlink loop
			ScanDir(hp, rp, depth + 1, layer, seen, count);
		}
		else if( S_ISREG(st.st_mode) ){
			if( count >= VFS_MAX_DIR_FILES ){ continue; }
			Entry e;
			e.layer = layer;
			e.name = Normalize(rp.c_str(), true);
			if( e.name.empty() ){ continue; }
			e.hostpath = hp;
			e.method = 0; e.crc = 0; e.csize = 0; e.usize = (uint32_t)std::min<off_t>(st.st_size, 0xFFFFFFFF);
			e.dataofs = 0; e.verified = true;
			AddEntry(std::move(e));
			count++;
		}
	}
	closedir(d);
}

// ---------------------------------------------------------------- zip

inline uint16_t R16(const unsigned char* p){ return (uint16_t)(p[0] | (p[1] << 8)); }
inline uint32_t R32(const unsigned char* p){ return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

struct ZipItem {
	std::string name;	// after delimiter/'..' normalisation, case kept
	uint16_t method;
	uint32_t crc, csize, usize;
	size_t dataofs;
};

//! Strip a single common top-level folder X when every file lives under X/
//! and X itself wraps the game layout (X/data/ or X/addon/ exists). A zip whose
//! only top folder IS data/ or addon/ (xops096.zip, addon packs) is untouched.
void StripPrefix(std::vector<ZipItem>& items, const char* label)
{
	if( items.empty() ){ return; }
	std::string top;
	bool wraps = false;
	for(const ZipItem& it : items){
		size_t s = it.name.find('/');
		if( s == std::string::npos ){ return; }			// a file at the root
		std::string t = it.name.substr(0, s);
		if( top.empty() ){ top = t; }
		else if( t != top ){ return; }
		std::string rest = LowerAscii(it.name.substr(s + 1));
		if( rest.compare(0, 5, "data/") == 0 || rest.compare(0, 6, "addon/") == 0 ){ wraps = true; }
	}
	std::string lt = LowerAscii(top);
	if( !wraps || lt == "data" || lt == "addon" ){ return; }
	for(ZipItem& it : items){ it.name = it.name.substr(top.size() + 1); }
	Log("%s: stripped common top-level folder '%s/'", label, top.c_str());
}

int ParseZip(const unsigned char* p, size_t len, const char* label, std::vector<ZipItem>& items)
{
	if( len < 22 ){ Log("%s: too small to be a zip", label); return VFS_ERR_CORRUPT; }
	// End of central directory: last 22 bytes + up to 65535 bytes of comment.
	size_t eocd = (size_t)-1;
	size_t lo = (len > 22 + 65535) ? len - 22 - 65535 : 0;
	for(size_t i=len-22; ; i--){
		if( R32(p + i) == 0x06054b50 && i + 22 + R16(p + i + 20) == len ){ eocd = i; break; }
		if( i == lo ){ break; }
	}
	if( eocd == (size_t)-1 ){ Log("%s: no end-of-central-directory record (not a zip?)", label); return VFS_ERR_CORRUPT; }
	uint16_t disk = R16(p + eocd + 4), cddisk = R16(p + eocd + 6);
	uint16_t n_here = R16(p + eocd + 8), n_total = R16(p + eocd + 10);
	uint32_t cdsize = R32(p + eocd + 12), cdofs = R32(p + eocd + 16);
	if( n_total == 0xFFFF || cdsize == 0xFFFFFFFF || cdofs == 0xFFFFFFFF ){
		Log("%s: zip64 archives are not supported", label); return VFS_ERR_UNSUPPORTED;
	}
	if( disk != 0 || cddisk != 0 || n_here != n_total ){
		Log("%s: multi-disk archives are not supported", label); return VFS_ERR_UNSUPPORTED;
	}
	if( (uint64_t)cdofs + cdsize > eocd ){ Log("%s: central directory out of range", label); return VFS_ERR_CORRUPT; }
	// Data prepended to the archive (self-extractor stub) shifts every offset.
	size_t bias = eocd - ((size_t)cdofs + cdsize);

	const unsigned char* c = p + bias + cdofs;
	const unsigned char* cend = c + cdsize;
	uint64_t total_size = 0;
	for(unsigned n=0; n<n_total; n++){
		if( (size_t)(cend - c) < 46 || R32(c) != 0x02014b50 ){ Log("%s: central directory entry %ld damaged", label, (long)n); return VFS_ERR_CORRUPT; }
		uint16_t flags = R16(c + 8), method = R16(c + 10);
		uint32_t crc = R32(c + 16), csize = R32(c + 20), usize = R32(c + 24);
		uint16_t nlen = R16(c + 28), xlen = R16(c + 30), klen = R16(c + 32);
		uint32_t lofs = R32(c + 42);
		if( (size_t)(cend - c) < (size_t)46 + nlen + xlen + klen ){ Log("%s: central directory entry %ld truncated", label, (long)n); return VFS_ERR_CORRUPT; }
		std::string raw((const char*)c + 46, nlen);
		c += 46 + nlen + xlen + klen;

		if( raw.find('\0') != std::string::npos || (!raw.empty() && (raw[0] == '/' || raw[0] == '\\')) || raw.find(':') != std::string::npos ){ return VFS_ERR_CORRUPT; }
		if( raw.empty() ){ continue; }
		bool lastIsDelim = false;
		for(size_t i=0; i<raw.size(); ++i){
			if( IsLead((unsigned char)raw[i]) && i+1<raw.size() ){ ++i; lastIsDelim=false; }
			else { lastIsDelim = raw[i] == '/' || raw[i] == '\\'; }
		}
		if( lastIsDelim ){
			std::string dirName = raw.substr(0, raw.size()-1);
			if( !dirName.empty() && Normalize(dirName.c_str(), true).empty() ){ return VFS_ERR_CORRUPT; }
			continue;	// directory entry
		}
		if( csize == 0xFFFFFFFF || usize == 0xFFFFFFFF || lofs == 0xFFFFFFFF ){
			Log("%s: zip64 entry '%s' is not supported", label, raw.c_str()); return VFS_ERR_UNSUPPORTED;
		}
		// Local header: validate now so damage is reported at mount time.
		size_t l = bias + (size_t)lofs;
		if( l > (size_t)(cend - p) - cdsize || (size_t)(cend - p) - cdsize - l < 30 || R32(p + l) != 0x04034b50 ){ Log("%s: local header of '%s' damaged", label, raw.c_str()); return VFS_ERR_CORRUPT; }
		size_t dataofs = l + 30 + R16(p + l + 26) + R16(p + l + 28);
		if( dataofs > bias + cdofs || csize > bias + cdofs - dataofs ){ Log("%s: data of '%s' runs past the end of the archive", label, raw.c_str()); return VFS_ERR_CORRUPT; }

		if( R16(p+l+8) != method || R16(p+l+6) != flags || R16(p+l+26) != nlen || memcmp(p+l+30, raw.data(), nlen) != 0 ){ return VFS_ERR_CORRUPT; }
		if( !(flags & 8) && (R32(p+l+14) != crc || R32(p+l+18) != csize || R32(p+l+22) != usize) ){ return VFS_ERR_CORRUPT; }
		if( flags & 0x0001 ){ Log("%s: '%s' is encrypted; skipped", label, raw.c_str()); continue; }
		if( method != 0 && method != 8 ){ Log("%s: '%s' uses unsupported compression method %ld (only stored/deflate); skipped", label, raw.c_str(), (long)method); continue; }
		total_size += usize;
		if( usize > VFS_MAX_ENTRY_SIZE || total_size > VFS_MAX_ARCHIVE_SIZE ){ return VFS_ERR_CORRUPT; }
		if( method == 0 && csize != usize ){ Log("%s: stored entry '%s' has csize != usize", label, raw.c_str()); return VFS_ERR_CORRUPT; }

		std::string gamename = (flags & 0x0800) ? ToGameBytes(raw, true, label) : raw;
		ZipItem it;
		it.name = Normalize(gamename.c_str(), true);
		if( it.name.empty() ){ return VFS_ERR_CORRUPT; }
		it.method = method; it.crc = crc; it.csize = csize; it.usize = usize; it.dataofs = dataofs;
		items.push_back(std::move(it));
	}
	if( c != cend ){ return VFS_ERR_CORRUPT; }
	StripPrefix(items, label);
	return VFS_OK;
}

//! Make sure a zip entry's bytes are available and correct. Returns the data pointer or NULL.
const unsigned char* ZipData(Entry& e, Layer& L)
{
	const unsigned char* src = L.bytes.data() + e.dataofs;
	if( e.method == 0 ){
		if( !e.verified ){
			if( (uint32_t)crc32(0L, src, e.usize) != e.crc ){
				Log("%s: CRC mismatch in '%s'", L.label.c_str(), e.name.c_str()); return NULL;
			}
			e.verified = true;
		}
		return src;
	}
	if( e.inflated ){ return e.inflated.get(); }
	std::unique_ptr<unsigned char[]> out(new (std::nothrow) unsigned char[e.usize ? e.usize : 1]);
	if( !out ){ Log("%s: out of memory inflating '%s'", L.label.c_str(), e.name.c_str()); return NULL; }
	z_stream zs;
	memset(&zs, 0, sizeof(zs));
	if( inflateInit2(&zs, -MAX_WBITS) != Z_OK ){ return NULL; }
	zs.next_in = const_cast<Bytef*>(src);
	zs.avail_in = e.csize;
	zs.next_out = out.get();
	zs.avail_out = e.usize ? e.usize : 1;
	int r = inflate(&zs, Z_FINISH);
	uLong total = zs.total_out;
	uLong consumed = zs.total_in;
	inflateEnd(&zs);
	if( r != Z_STREAM_END || total != e.usize || consumed != e.csize ){
		Log("%s: '%s' does not inflate (zlib %ld)", L.label.c_str(), e.name.c_str(), (long)r); return NULL;
	}
	if( (uint32_t)crc32(0L, out.get(), e.usize) != e.crc ){
		Log("%s: CRC mismatch in '%s'", L.label.c_str(), e.name.c_str()); return NULL;
	}
	e.verified = true;
	e.inflated = std::move(out);
	return e.inflated.get();
}

// ---------------------------------------------------------------- cookies

ssize_t NullRead(void*, char*, size_t){ return 0; }
ssize_t NullWrite(void*, const char*, size_t){ return 0; }
int NullSeek(void*, off64_t* pos, int){ *pos = 0; return 0; }
int NullClose(void*){ return 0; }

FILE* EmptyFile()
{
	cookie_io_functions_t io = { NullRead, NullWrite, NullSeek, NullClose };
	return fopencookie(NULL, "rb", io);
}

ssize_t SaveRead(void* c, char* b, size_t n){ size_t r = fread(b, 1, n, (FILE*)c); return (r == 0 && ferror((FILE*)c)) ? -1 : (ssize_t)r; }
ssize_t SaveWrite(void* c, const char* b, size_t n){ size_t r = fwrite(b, 1, n, (FILE*)c); return (r == 0 && n != 0) ? -1 : (ssize_t)r; }
int SaveSeek(void* c, off64_t* pos, int whence)
{
	if( fseeko((FILE*)c, (off_t)*pos, whence) != 0 ){ return -1; }
	*pos = (off64_t)ftello((FILE*)c);
	return 0;
}
int SaveClose(void* c)
{
	int r = fclose((FILE*)c);
	if( g_save_hook ){ g_save_hook(); }
	return r;
}

// ---------------------------------------------------------------- save root

const std::string& SaveRoot()
{
	if( g_save_root.empty() ){
#ifdef __EMSCRIPTEN__
		g_save_root = "/save";
#else
		const char* x = getenv("XDG_DATA_HOME");
		if( x != NULL && x[0] == '/' ){ g_save_root = std::string(x) + "/openxops-web"; }
		else{
			const char* h = getenv("HOME");
			g_save_root = std::string((h != NULL && h[0] != '\0') ? h : ".") + "/.local/share/openxops-web";
		}
#endif
	}
	return g_save_root;
}

bool MakeParents(const std::string& path)
{
	// create every directory component of `path` except the last
	for(size_t i=1; i<path.size(); i++){
		if( path[i] != '/' ){ continue; }
		std::string d = path.substr(0, i);
		if( mkdir(d.c_str(), 0755) != 0 && errno != EEXIST ){ return false; }
	}
	return true;
}

// Walk below the trusted save root using descriptors, never following a
// symlink in a virtual path (including the final file).
FILE* OpenSave(const std::string& key, const char* mode, bool write)
{
 if( write && !MakeParents(SaveRoot() + "/.create") ){ return NULL; }
 int dir = open(SaveRoot().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
 if( dir < 0 ){ return NULL; }
 size_t start = 0, slash;
 while( (slash = key.find('/', start)) != std::string::npos ){
  std::string part = key.substr(start, slash-start);
  if( write && mkdirat(dir, part.c_str(), 0755) != 0 && errno != EEXIST ){ close(dir); return NULL; }
  int next = openat(dir, part.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
  close(dir); if( next < 0 ){ return NULL; } dir = next; start = slash+1;
 }
 int flags = O_NOFOLLOW | O_CLOEXEC;
 flags |= strchr(mode, '+') ? O_RDWR : write ? O_WRONLY : O_RDONLY;
 if( mode[0] == 'w' ){ flags |= O_CREAT | O_TRUNC; }
 if( mode[0] == 'a' ){ flags |= O_CREAT | O_APPEND; }
 if( strchr(mode, 'x') ){ flags |= O_EXCL; }
 int fd = openat(dir, key.substr(start).c_str(), flags, 0644); close(dir);
 if( fd < 0 ){ return NULL; }
 struct stat st;
 if( fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) ){ close(fd); return NULL; }
 FILE* fp = fdopen(fd, mode);
 if( !fp ){ close(fd); }
 return fp;
}

void EnsureInit()
{
	if( !g_initialised ){ vfs_init_from_args(NULL); }
}

}	// namespace

// ================================================================ public API

bool vfs_normalize(const char* path, char* out, size_t outsize, bool keep_case)
{
	if( path == NULL || out == NULL || outsize == 0 ){ return false; }
	bool valid;
	std::string s = Normalize(path, keep_case, &valid);
	if( !valid ){ out[0] = '\0'; return false; }
	if( s.size() + 1 > outsize ){ out[0] = '\0'; return false; }
	memcpy(out, s.c_str(), s.size() + 1);
	return true;
}

bool vfs_mount_dir(const char* hostdir, const char* label)
{
	if( hostdir == NULL || hostdir[0] == '\0' ){ return false; }
	g_initialised = true;
	char abs[PATH_MAX];
	if( realpath(hostdir, abs) == NULL ){ Log("mount_dir: '%s' not found", hostdir); return false; }
	struct stat st;
	if( stat(abs, &st) != 0 || !S_ISDIR(st.st_mode) ){ Log("mount_dir: '%s' is not a directory", hostdir); return false; }

	DIR* readable = opendir(abs);
	if( !readable ){ return false; }
	closedir(readable);
	std::unique_ptr<Layer> L(new Layer);
	L->kind = LAYER_DIR;
	L->label = (label != NULL) ? label : hostdir;
	L->hostroot = abs;
	g_layers.push_back(std::move(L));
	int layer = (int)g_layers.size() - 1;

	std::set<std::pair<dev_t, ino_t>> seen;
	seen.insert(std::make_pair(st.st_dev, st.st_ino));
	int count = 0;
	ScanDir(abs, "", 0, layer, seen, count);
	if( count >= VFS_MAX_DIR_FILES ){ Log("mount_dir: '%s' has more than %ld files; the rest are ignored", hostdir, (long)VFS_MAX_DIR_FILES); }
	if( Trace() ){ Log("mounted dir '%s' (%s), %ld files", g_layers[layer]->label.c_str(), abs, (long)count); }
	return true;
}

int vfs_mount_zip_memory(const unsigned char* data, size_t len, const char* label)
try {
	if( data == NULL ){ return VFS_ERR_ARG; }
	if( len > VFS_MAX_ARCHIVE_SIZE ){ return VFS_ERR_CORRUPT; }
	g_initialised = true;
	const char* lb = (label != NULL) ? label : "zip";
	std::vector<ZipItem> items;
	int r = ParseZip(data, len, lb, items);
	if( r != VFS_OK ){ return r; }

	std::unique_ptr<Layer> L(new (std::nothrow) Layer);
	if( !L ){ return VFS_ERR_NOMEM; }
	L->kind = LAYER_ZIP;
	L->label = lb;
	try{ L->bytes.assign(data, data + len); }
	catch(...){ return VFS_ERR_NOMEM; }
	// Validate all payloads before publishing any part of the mount.
	for(const ZipItem& it : items){
		Entry e{}; e.name=it.name; e.method=it.method; e.crc=it.crc;
		e.csize=it.csize; e.usize=it.usize; e.dataofs=it.dataofs;
		if( !ZipData(e, *L) ){ return VFS_ERR_CORRUPT; }
	}
	int layer = (int)g_layers.size();
	std::map<std::string, Entry> staged;
	for(ZipItem& it : items){
		Entry e;
		e.layer = layer;
		e.name = std::move(it.name);
		e.method = it.method; e.crc = it.crc; e.csize = it.csize; e.usize = it.usize;
		e.dataofs = it.dataofs; e.verified = false;
		std::string key = LowerAscii(e.name);
		staged.insert_or_assign(std::move(key), std::move(e));
	}
	g_layers.push_back(std::move(L));
	for(auto& item : staged){
		auto old = g_index.find(item.first);
		if( old != g_index.end() ){ old->second = std::move(item.second); }
	}
	g_index.merge(staged);
	if( Trace() ){ Log("mounted zip '%s', %ld files", lb, (long)items.size()); }
	return VFS_OK;
} catch(const std::bad_alloc&) { return VFS_ERR_NOMEM; }

int vfs_mount_zip_file(const char* hostpath, const char* label)
try {
	if( hostpath == NULL || hostpath[0] == '\0' ){ return VFS_ERR_ARG; }
	FILE* fp = fopen(hostpath, "rb");
	if( fp == NULL ){ Log("mount_zip: cannot open '%s'", hostpath); return VFS_ERR_IO; }
	auto close_file = [](FILE* f){ fclose(f); };
	std::unique_ptr<FILE, decltype(close_file)> owned(fp, close_file);
	std::vector<unsigned char> buf;
	unsigned char tmp[65536];
	size_t n;
	while( (n = fread(tmp, 1, sizeof(tmp), fp)) > 0 ){
		if( buf.size() + n > VFS_MAX_ARCHIVE_SIZE ){ return VFS_ERR_CORRUPT; }
		buf.insert(buf.end(), tmp, tmp + n);
	}
	bool err = ferror(fp) != 0;
	owned.reset();
	if( err ){ Log("mount_zip: read error on '%s'", hostpath); return VFS_ERR_IO; }
	if( label == NULL ){
		const char* s = strrchr(hostpath, '/');
		label = s ? s + 1 : hostpath;
	}
	if( buf.empty() ){ return VFS_ERR_CORRUPT; }
	return vfs_mount_zip_memory(buf.data(), buf.size(), label);
} catch(const std::bad_alloc&) { return VFS_ERR_NOMEM; }

void vfs_unmount_all()
{
	g_index.clear();
	g_layers.clear();
}

void vfs_set_save_root(const char* hostdir)
{
	g_save_root = (hostdir != NULL) ? hostdir : "";
	while( g_save_root.size() > 1 && g_save_root[g_save_root.size() - 1] == '/' ){ g_save_root.erase(g_save_root.size() - 1); }
}

void vfs_set_save_hook(void (*after_close)(void))
{
	g_save_hook = after_close;
}

void vfs_init_from_args(const char* datadir)
{
	if( g_initialised ){ return; }
	g_initialised = true;
	const char* sd = getenv("OPENXOPS_SAVE_DIR");
	if( sd != NULL && sd[0] != '\0' && g_save_root.empty() ){ vfs_set_save_root(sd); }

	vfs_mount_dir((datadir != NULL && datadir[0] != '\0') ? datadir : ".", "dir");

	const char* z = getenv("OPENXOPS_DATA_ZIP");
	if( z != NULL && z[0] != '\0' ){
		std::string list = z;
		size_t start = 0;
		while( start <= list.size() ){
			size_t end = list.find(':', start);
			if( end == std::string::npos ){ end = list.size(); }
			std::string one = list.substr(start, end - start);
			if( !one.empty() ){
				int r = vfs_mount_zip_file(one.c_str(), NULL);
				if( r != VFS_OK ){ Log("OPENXOPS_DATA_ZIP: '%s' not mounted (error %ld)", one.c_str(), (long)r); }
			}
			start = end + 1;
		}
	}
}

FILE* vfs_fopen(const char* path, const char* mode)
{
	if( path == NULL || mode == NULL || path[0] == '\0' ){ return NULL; }
	EnsureInit();
	bool write = strchr(mode, 'w') || strchr(mode, 'a') || strchr(mode, '+');

	if( path[0] == '/' ){
		if( write ){ return NULL; }
		// absolute host path (e.g. a .mif given on the command line): not virtualised
		FILE* fp = fopen(path, write ? mode : "rb");
		if( Trace() ){ Log("open %s '%s' -> host %s", write ? "w" : "r", path, fp ? "ok" : "MISSING"); }
		return fp;
	}

	bool valid = false;
	std::string key = Normalize(path, false, &valid);
	if( !valid ){ return NULL; }
	if( key.find(':') != std::string::npos ){ return NULL; }
	if( key.empty() ){ return NULL; }
	std::string savepath = SaveRoot() + "/" + key;

	if( write ){
		FILE* fp = OpenSave(key, mode, true);
		if( Trace() ){ Log("open w '%s' -> %s %s", path, savepath.c_str(), fp ? "ok" : "FAILED"); }
		if( fp == NULL || g_save_hook == NULL ){ return fp; }
		cookie_io_functions_t io = { SaveRead, SaveWrite, SaveSeek, SaveClose };
		FILE* wrapped = fopencookie(fp, mode, io);
		if( wrapped == NULL ){ fclose(fp); }
		return wrapped;
	}

	// reads: save root first
	if( FILE* saved = OpenSave(key, "rb", false) ){
		if( Trace() ){ Log("open r '%s' -> save %s", path, savepath.c_str()); }
		return saved;
	}
	auto it = g_index.find(key);
	if( it == g_index.end() ){
		if( Trace() ){ Log("open r '%s' -> MISSING (key '%s')", path, key.c_str()); }
		return NULL;
	}
	Entry& e = it->second;
	Layer& L = *g_layers[e.layer];
	if( Trace() ){ Log("open r '%s' -> %s %s", path, L.label.c_str(), e.name.c_str()); }
	if( L.kind == LAYER_DIR ){ return fopen(e.hostpath.c_str(), "rb"); }
	const unsigned char* data = ZipData(e, L);
	if( data == NULL ){ return NULL; }
	if( e.usize == 0 ){ return EmptyFile(); }	// fmemopen() rejects size 0
	return fmemopen(const_cast<unsigned char*>(data), e.usize, "rb");
}

int vfs_list(const char* dir, const char* ext, char out[][MAX_PATH], int max)
{
	if( out == NULL || max <= 0 ){ return 0; }
	EnsureInit();
	bool valid;
	std::string prefix = Normalize(dir != NULL ? dir : "", false, &valid);
	if( !valid ){ return 0; }
	if( !prefix.empty() ){ prefix += '/'; }
	std::string lext = (ext != NULL) ? LowerAscii(ext) : "";
	int n = 0;
	for(auto it = g_index.lower_bound(prefix); it != g_index.end() && n < max; ++it){
		const std::string& key = it->first;
		if( key.compare(0, prefix.size(), prefix) != 0 ){ break; }
		if( key.find('/', prefix.size()) != std::string::npos ){ continue; }	// in a subdirectory
		if( !lext.empty() && (key.size() < lext.size() || key.compare(key.size() - lext.size(), lext.size(), lext) != 0) ){ continue; }
		const std::string& name = it->second.name;
		std::string base = name.substr(name.size() - (key.size() - prefix.size()));
		if( base.size() + 1 > MAX_PATH ){ continue; }
		memcpy(out[n], base.c_str(), base.size() + 1);
		n++;
	}
	return n;
}

bool vfs_dir_exists(const char* dir)
{
	if( dir == NULL ){ return false; }
	EnsureInit();
	bool valid;
	std::string key = Normalize(dir, false, &valid);
	if( !valid ){ return false; }
	if( key.empty() ){ return true; }	// the root
	std::string prefix = key + "/";
	auto it = g_index.lower_bound(prefix);
	if( it != g_index.end() && it->first.compare(0, prefix.size(), prefix) == 0 ){ return true; }
	struct stat st;
	std::string host = Normalize(dir, true);
	if( stat(host.c_str(), &st) == 0 && S_ISDIR(st.st_mode) ){ return true; }
	std::string sp = SaveRoot() + "/" + key;
	if( stat(sp.c_str(), &st) == 0 && S_ISDIR(st.st_mode) ){ return true; }
	return false;
}

const char* vfs_debug_resolve(const char* path)
{
	static std::string buf;
	if( path == NULL ){ return NULL; }
	EnsureInit();
	if( path[0] == '/' ){ buf = std::string("host:") + path; return buf.c_str(); }
	std::string key = Normalize(path, false);
	std::string savepath = SaveRoot() + "/" + key;
	if( !key.empty() ){
		if( FILE* saved = OpenSave(key, "rb", false) ){ fclose(saved); buf = "save:" + savepath; return buf.c_str(); }
	}
	auto it = g_index.find(key);
	if( it == g_index.end() ){ return NULL; }
	const Layer& L = *g_layers[it->second.layer];
	if( L.kind == LAYER_DIR ){ buf = "dir:" + L.label + ":" + it->second.hostpath; }
	else{ buf = "zip:" + L.label + ":" + it->second.name; }
	return buf.c_str();
}

int vfs_enumerate(void (*fn)(const char* key, const char* name, const char* layer, size_t size, void* user), void* user)
{
	EnsureInit();
	int n = 0;
	for(auto& kv : g_index){
		if( fn ){ fn(kv.first.c_str(), kv.second.name.c_str(), g_layers[kv.second.layer]->label.c_str(), kv.second.usize, user); }
		n++;
	}
	return n;
}
