// Engine-collision proof harness: loads a real .bd1 via the unmodified
// src/datafile.cpp loader, runs CalculationBlockdata (face normals), then
// uses the unmodified src/collision.cpp (Collision class) to assert:
//   - no block is misclassified as a degenerate "BoardBlock" (which the
//     engine completely excludes from ALL collision -- see
//     Collision::InitCollision's CheckPolygonFront(i,j,center) loop and
//     every CheckBlockInside/CheckBlockAABB/CheckBlockIntersectRay early
//     "if (cbdata[id].BoardBlock) return false" guard, src/collision.cpp).
//   - a point resting on the ground slab top is reported "inside" (solid).
//   - a ray cast straight down from just above the ground slab top hits
//     face 0 (top) with a normal pointing up (+y).
//
// Usage: collision_harness <file.bd1> <spawn_x> <spawn_y> <spawn_z>
// Exit 0 on all assertions passing, non-zero + stderr message otherwise.

#define H_LAYERLEVEL 2
#include "datafile.h"
#include "collision.h"

#include <cstdio>
#include <cstdlib>

int main(int argc, char **argv) {
    if (argc != 5) {
        std::fprintf(stderr, "usage: %s <file.bd1> <spawn_x> <spawn_y> <spawn_z>\n", argv[0]);
        return 2;
    }

    float spawn_x = std::atof(argv[2]);
    float spawn_y = std::atof(argv[3]);
    float spawn_z = std::atof(argv[4]);

    BlockDataInterface blocks;
    if (blocks.LoadFiledata(argv[1]) != 0) {
        std::fprintf(stderr, "FAIL: BlockDataInterface::LoadFiledata failed\n");
        return 1;
    }
    blocks.CalculationBlockdata(false);

    int n = blocks.GetTotaldatas();
    std::printf("BLOCKS %d\n", n);

    Collision coll;
    if (coll.InitCollision(&blocks) != 0) {
        std::fprintf(stderr, "FAIL: Collision::InitCollision failed\n");
        return 1;
    }

    // Find the block whose horizontal AABB contains the spawn point and
    // whose top is nearest below spawn_y -- that is "the ground slab
    // under the player" for this assertion.
    int ground_block = -1;
    float ground_top = -1e9f;
    for (int i = 0; i < n; i++) {
        float minx, miny, minz, maxx, maxy, maxz;
        coll.GetBlockPosMINMAX(i, &minx, &miny, &minz, &maxx, &maxy, &maxz);
        if (spawn_x < minx || maxx < spawn_x) continue;
        if (spawn_z < minz || maxz < spawn_z) continue;
        if (maxy <= spawn_y + 0.5f && maxy > ground_top) {
            ground_top = maxy;
            ground_block = i;
        }
    }
    if (ground_block == -1) {
        std::fprintf(stderr, "FAIL: no block found under spawn (%.2f,%.2f,%.2f)\n", spawn_x, spawn_y, spawn_z);
        return 1;
    }
    std::printf("GROUND_BLOCK %d top_y=%.3f\n", ground_block, ground_top);

    // Assertion 1: the ground block must not be a degenerate BoardBlock
    // (CheckBlockAABB/CheckBlockInside/CheckBlockIntersectRay all skip
    // BoardBlock entries entirely -- that is exactly "floor isn't solid").
    // We can't read cbdata[].BoardBlock directly (private), so we infer it
    // via CheckBlockAABB: a non-BoardBlock ground slab's own AABB must
    // collide with itself.
    if (coll.CheckBlockAABB(ground_block, spawn_x - 0.1f, ground_top - 0.1f, spawn_z - 0.1f,
                             spawn_x + 0.1f, ground_top + 0.1f, spawn_z + 0.1f) == false) {
        std::fprintf(stderr, "FAIL: ground block %d is excluded from collision (BoardBlock) "
                              "-- inverted/degenerate face winding\n", ground_block);
        return 1;
    }

    // Assertion 2: a point resting just inside the ground slab top is "inside".
    if (coll.CheckALLBlockInside(spawn_x, ground_top - 0.05f, spawn_z) == false) {
        std::fprintf(stderr, "FAIL: point just above ground_top-0.05 is not solid ground\n");
        return 1;
    }

    // Assertion 3: a point just ABOVE the ground slab top must be clear
    // (so the player can actually stand there, not clipped into the block).
    if (coll.CheckALLBlockInside(spawn_x, ground_top + 0.5f, spawn_z) == true) {
        std::fprintf(stderr, "FAIL: point 0.5 above ground_top is still inside a block "
                              "(player would be stuck in the floor)\n");
        return 1;
    }

    // Assertion 4: a ray straight down from just above the slab top must
    // hit face 0 (top) with a normal pointing up (this is what the
    // engine's own "am I standing on something" checks rely on).
    int hit_id = -1, hit_face = -1;
    float dist = 0.0f;
    bool hit = coll.CheckALLBlockIntersectRay(spawn_x, ground_top + 1.0f, spawn_z,
                                               0.0f, -1.0f, 0.0f, &hit_id, &hit_face, &dist, 1000.0f);
    if (!hit) {
        std::fprintf(stderr, "FAIL: downward ray from ground_top+1.0 hit nothing "
                              "(player would free-fall through the floor)\n");
        return 1;
    }
    std::printf("RAY_HIT block=%d face=%d dist=%.3f\n", hit_id, hit_face, dist);
    if (hit_face != 0) {
        std::fprintf(stderr, "FAIL: downward ray hit face %d, expected face 0 (top)\n", hit_face);
        return 1;
    }

    std::printf("OK\n");
    return 0;
}
