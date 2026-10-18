// Engine-loader proof harness for Phase B of docs/OPEN-ASSETS-SOW.md.
//
// Links directly against the real src/datafile.cpp (BSD engine source,
// unmodified) and calls BlockDataInterface::LoadFiledata /
// PointDataInterface::LoadFiledata on the files passed on argv, then
// prints what the engine itself parsed. This is the independent check
// that tools/mapgen's .bd1/.pd1 writers produce bytes the real loader
// accepts -- not just bytes our own reader accepts.
//
// Usage: load_harness <file.bd1> <file.pd1>
// Output (stdout), one line each:
//   BLOCKS <n>
//   POINTS <n>

#define H_LAYERLEVEL 1
#include "datafile.h"

#include <cstdio>

int main(int argc, char **argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: %s <file.bd1> <file.pd1>\n", argv[0]);
        return 2;
    }

    BlockDataInterface blocks;
    int rc = blocks.LoadFiledata(argv[1]);
    if (rc != 0) {
        std::fprintf(stderr, "BlockDataInterface::LoadFiledata failed: %d\n", rc);
        return 1;
    }
    std::printf("BLOCKS %d\n", blocks.GetTotaldatas());

    PointDataInterface points;
    rc = points.LoadFiledata(argv[2]);
    if (rc != 0) {
        std::fprintf(stderr, "PointDataInterface::LoadFiledata failed: %d\n", rc);
        return 1;
    }
    std::printf("POINTS %d\n", points.GetTotaldatas());

    return 0;
}
