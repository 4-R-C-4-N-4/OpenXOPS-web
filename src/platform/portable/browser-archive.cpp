// Browser startup preflight uses the same validated VFS as the engine.
#include "../vfs.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define EXPORT
#endif
static char archive_error[512];
extern "C" EXPORT const char* openxops_archive_error() { return archive_error; }
// Layering: the bundled free pack (/openxops-data.zip) is mounted first and
// the optional original archive (/xops096.zip) over it, so original files
// override free ones. main() mounts the same list via OPENXOPS_DATA_ZIP.
static bool host_file_exists(const char* path) {
    FILE* fp = std::fopen(path, "rb");
    if (!fp) return false;
    std::fclose(fp);
    return true;
}
static int prepare_layers(const char* free_pack, const char* original) {
    archive_error[0] = '\0';
    vfs_unmount_all();
    std::string env;
    if (free_pack && host_file_exists(free_pack)) {
        const int r = vfs_mount_zip_file(free_pack, "free data pack");
        if (r != VFS_OK) {
            std::snprintf(archive_error, sizeof(archive_error),
                "The bundled free data pack could not be read (archive error %d). Reload the page.", r);
            return 3;
        }
        env = free_pack;
    }
    if (original && host_file_exists(original)) {
        const int r = vfs_mount_zip_file(original, "selected archive");
        if (r != VFS_OK) {
            std::snprintf(archive_error, sizeof(archive_error),
                "Cannot read this ZIP (archive error %d). It may be damaged, encrypted, or unsupported. Select the original xops096.zip.", r);
            vfs_unmount_all();
            return 1;
        }
        if (!env.empty()) env += ":";
        env += original;
    }
    const char* required[] = {"data/char.dds", "data/model/up0.x",
        "data/map10/temp.bd1", "data/sound/bang.wav"};
    for (const char* filename : required) {
        FILE* fp = vfs_fopen(filename, "rb");
        if (!fp) {
            std::snprintf(archive_error, sizeof(archive_error),
                "Game data is missing %s. If you selected classic data, choose xops096.zip (the original X operations 0.96 game), not 1.080.zip (OpenXOPS engine) or 1.080-src.zip (source code).", filename);
            vfs_unmount_all();
            return 2;
        }
        std::fclose(fp);
    }
    setenv("OPENXOPS_DATA_ZIP", env.c_str(), 1);
    return 0;
}
int openxops_prepare_archive_path(const char* path) {
    return prepare_layers(nullptr, path);
}
extern "C" EXPORT int openxops_prepare_archive() {
    return prepare_layers("/openxops-data.zip", "/xops096.zip");
}
