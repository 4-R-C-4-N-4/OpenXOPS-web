// Browser startup preflight uses the same validated VFS as the engine.
#include "../vfs.h"
#include <cstdio>
#include <cstring>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define EXPORT
#endif
static char archive_error[512];
extern "C" EXPORT const char* openxops_archive_error() { return archive_error; }
int openxops_prepare_archive_path(const char* path) {
    archive_error[0] = '\0';
    vfs_unmount_all();
    const int result = vfs_mount_zip_file(path, "selected archive");
    if (result != VFS_OK) {
        std::snprintf(archive_error, sizeof(archive_error),
            "Cannot read this ZIP (archive error %d). It may be damaged, encrypted, or unsupported. Select the original xops096.zip.", result);
        return 1;
    }
    const char* required[] = {"data/char.dds", "data/model/up0.x",
        "data/map10/temp.bd1", "data/sound/bang.wav"};
    for (const char* filename : required) {
        FILE* fp = vfs_fopen(filename, "rb");
        if (!fp) {
            std::snprintf(archive_error, sizeof(archive_error),
                "This ZIP is missing %s. Choose xops096.zip (the original X operations 0.96 game), not 1.080.zip (OpenXOPS engine) or 1.080-src.zip (source code).", filename);
            vfs_unmount_all();
            return 2;
        }
        std::fclose(fp);
    }
    return 0;
}
extern "C" EXPORT int openxops_prepare_archive() {
    return openxops_prepare_archive_path("/xops096.zip");
}
