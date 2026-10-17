#include "vfs.h"
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>
#include <zlib.h>
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      std::cerr << __LINE__ << ": " #x "\n";                                   \
      std::exit(1);                                                            \
    }                                                                          \
  } while (0)
using Bytes = std::vector<unsigned char>;
static void put(Bytes &b, unsigned v, int n) {
  while (n--) {
    b.push_back(v & 255);
    v >>= 8;
  }
}
static void set(Bytes &b, size_t p, unsigned v, int n) {
  while (n--) {
    b[p++] = v & 255;
    v >>= 8;
  }
}
static Bytes zip(const std::string &name, const std::string &text,
                 bool deflate = false) {
  Bytes data(text.begin(), text.end());
  if (deflate) {
    z_stream s{};
    CHECK(deflateInit2(&s, 9, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) == Z_OK);
    data.resize(compressBound(text.size()));
    s.next_in = (Bytef *)text.data();
    s.avail_in = text.size();
    s.next_out = data.data();
    s.avail_out = data.size();
    CHECK(::deflate(&s, Z_FINISH) == Z_STREAM_END);
    data.resize(s.total_out);
    deflateEnd(&s);
  }
  Bytes b;
  unsigned crc = crc32(0, (const Bytef *)text.data(), text.size());
  put(b, 0x04034b50, 4);
  put(b, 20, 2);
  put(b, 0, 2);
  put(b, deflate ? 8 : 0, 2);
  put(b, 0, 4);
  put(b, crc, 4);
  put(b, data.size(), 4);
  put(b, text.size(), 4);
  put(b, name.size(), 2);
  put(b, 0, 2);
  b.insert(b.end(), name.begin(), name.end());
  b.insert(b.end(), data.begin(), data.end());
  size_t cd = b.size();
  put(b, 0x02014b50, 4);
  put(b, 20, 2);
  put(b, 20, 2);
  put(b, 0, 2);
  put(b, deflate ? 8 : 0, 2);
  put(b, 0, 4);
  put(b, crc, 4);
  put(b, data.size(), 4);
  put(b, text.size(), 4);
  put(b, name.size(), 2);
  put(b, 0, 2);
  put(b, 0, 2);
  put(b, 0, 2);
  put(b, 0, 2);
  put(b, 0, 4);
  put(b, 0, 4);
  b.insert(b.end(), name.begin(), name.end());
  size_t sz = b.size() - cd;
  put(b, 0x06054b50, 4);
  put(b, 0, 2);
  put(b, 0, 2);
  put(b, 1, 2);
  put(b, 1, 2);
  put(b, sz, 4);
  put(b, cd, 4);
  put(b, 0, 2);
  return b;
}
static int mount(const Bytes &b) {
  return vfs_mount_zip_memory(b.data(), b.size(), "test");
}
static std::string read(const char *p) {
  FILE *f = vfs_fopen(p, "rt");
  CHECK(f);
  std::string s;
  char b[4096];
  size_t n;
  while ((n = fread(b, 1, sizeof b, f)))
    s.append(b, n);
  CHECK(!ferror(f));
  CHECK(fclose(f) == 0);
  return s;
}
static int hooks = 0;
static void hook() { ++hooks; }
static void real_entry(const char *key, const char *, const char *, size_t size,
                       void *user) {
  CHECK(read(key).size() == size);
  ++*(int *)user;
}
int main(int argc, char **argv) {
  char norm[260];
  CHECK(vfs_normalize(".\\DATA//X/../A.TXT", norm, sizeof norm, false));
  CHECK(std::string(norm) == "data/a.txt");
  std::string cp = "DATA/";
  cp += "\x95\x5c\x83\x41";
  cp += "\\A.X";
  CHECK(vfs_normalize(cp.c_str(), norm, sizeof norm, false));
  CHECK(std::string(norm) == std::string("data/\x95\x5c\x83\x41/a.x"));
  CHECK(!vfs_normalize("abc", norm, 3, false));
  CHECK(!vfs_normalize("../../escape", norm, sizeof norm, false));
  const char *tmp = std::getenv("TMPDIR");
  std::string pattern = std::string(tmp ? tmp : ".") + "/vfs-test-XXXXXX";
  std::vector<char> buf(pattern.begin(), pattern.end());
  buf.push_back(0);
  CHECK(mkdtemp(buf.data()));
  std::filesystem::path root(buf.data());
  vfs_set_save_root((root / "save").c_str());
  if (argc > 1) {
    CHECK(vfs_mount_zip_file(argv[1], "real") == VFS_OK);
    int count = 0;
    int total = vfs_enumerate(real_entry, &count);
    CHECK(total > 100 && total == count);
    std::cout << "real ZIP: " << count << " files CRC-checked and read\n";
  } else {
    std::filesystem::create_directories(root / "base" / "Data");
    std::ofstream(root / "base" / "Data" / "A.MIF") << "base";
    std::ofstream(root / "base" / "Data" / "z.mif") << "z";
    CHECK(vfs_mount_dir((root / "base").c_str(), "base"));
    CHECK(read("data/a.mif") == "base");
    CHECK(mount(zip("DATA/a.mif", "override", true)) == VFS_OK);
    CHECK(read("data/A.MIF") == "override");
    CHECK(mount(zip("data/B.MIF", "stored")) == VFS_OK);
    CHECK(read("data/b.mif") == "stored");
    char names[8][MAX_PATH];
    CHECK(vfs_list("DATA", ".MIF", names, 8) == 3);
    CHECK(std::string(names[0]) == "a.mif" &&
          std::string(names[1]) == "B.MIF" && std::string(names[2]) == "z.mif");
    CHECK(mount(zip(cp, "cp932")) == VFS_OK);
    CHECK(read(cp.c_str()) == "cp932");
    CHECK(mount(zip("empty", "", true)) == VFS_OK);
    CHECK(read("empty").empty());
    vfs_set_save_hook(hook);
    FILE *f = vfs_fopen("DATA/A.MIF", "wt");
    CHECK(f);
    CHECK(fputs("saved", f) >= 0);
    CHECK(fclose(f) == 0);
    CHECK(hooks == 1);
    CHECK(read("data/a.mif") == "saved");
    CHECK(std::string(vfs_debug_resolve("data/a.mif")).find("save:") == 0);
    CHECK(vfs_dir_exists("DATA"));
    CHECK(!vfs_fopen("../../escape", "wb"));
    std::filesystem::create_directory_symlink(root / "base",
                                              root / "save" / "link");
    CHECK(!vfs_fopen("link/escape", "wb"));
    Bytes b = zip("bad", "payload");
    b[30 + 3] ^= 1;
    CHECK(mount(b) == VFS_ERR_CORRUPT);
    b = zip("bad", "payload", true);
    b.resize(b.size() - 10);
    CHECK(mount(b) == VFS_ERR_CORRUPT);
    b = zip("bad", "payload");
    set(b, b.size() - 22 + 16, 0xfffffff0, 4);
    CHECK(mount(b) == VFS_ERR_CORRUPT);
    b = zip("bad", "payload");
    set(b, 8, 8, 2);
    CHECK(mount(b) == VFS_ERR_CORRUPT);
    b = zip("../escape", "payload");
    CHECK(mount(b) == VFS_ERR_CORRUPT);
    b = zip("huge", "x", true);
    size_t cd = 30 + 4 + 3;
    set(b, cd + 24, 0x40000000, 4);
    CHECK(mount(b) != VFS_OK);
    CHECK(read("data/b.mif") == "stored");
    b = zip("data/b.mif", "corrupt");
    b[30 + 10] ^= 1;
    CHECK(mount(b) == VFS_ERR_CORRUPT);
    CHECK(read("data/b.mif") == "stored");
    CHECK(mount(zip("wrapper/data/new", "wrapped")) == VFS_OK);
    CHECK(read("data/new") == "wrapped");
    std::ofstream(root / "base" / "Data" / u8"表.txt") << "utf8";
    CHECK(vfs_mount_dir((root / "base").c_str(), "last-dir"));
    CHECK(read("data/\x95\x5c.txt") == "utf8");
    CHECK(read("data/a.mif") == "saved");
    f = vfs_fopen("data/b.mif", "rb");
    CHECK(f);
    CHECK(fseek(f, -3, SEEK_END) == 0);
    CHECK(ftell(f) == 3);
    CHECK(fgetc(f) == 'r');
    CHECK(fclose(f) == 0);
    std::filesystem::create_symlink(root / "base" / "Data" / "A.MIF",
                                    root / "save" / "alias");
    CHECK(!vfs_fopen("alias", "wb"));
    CHECK(!vfs_fopen((root / "absolute-write").c_str(), "wb"));
    b = zip("unsupported", "x");
    set(b, 8, 99, 2);
    set(b, b.size() - 22 - 11 - 46 + 10, 99, 2);
    CHECK(mount(b) == VFS_OK);
    CHECK(!vfs_fopen("unsupported", "rb"));
    b = zip("huge", "x", true);
    cd = 30 + 4 + 3;
    set(b, 22, 0x40000000, 4);
    set(b, cd + 24, 0x40000000, 4);
    CHECK(mount(b) == VFS_ERR_CORRUPT);
    // Every truncated prefix must fail without reading outside its input
    // buffer.
    b = zip("truncated", "hello", true);
    for (size_t len = 1; len < b.size(); ++len) {
      CHECK(vfs_mount_zip_memory(b.data(), len, "truncated") != VFS_OK);
    }
    // Memory mounts own their input and do not retain caller storage.
    b = zip("owned", "copy");
    CHECK(mount(b) == VFS_OK);
    std::fill(b.begin(), b.end(), 0);
    CHECK(read("owned") == "copy");
    // CP932 second byte may itself look like a lead; delimiter detection must
    // walk.
    int before = vfs_enumerate(nullptr, nullptr);
    CHECK(mount(zip("\x81\x81\\", "")) == VFS_OK);
    CHECK(vfs_enumerate(nullptr, nullptr) == before);
  }
  vfs_set_save_hook(nullptr);
  vfs_unmount_all();
  std::filesystem::remove_all(root);
  std::cout << "VFS checks passed\n";
}
