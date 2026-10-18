# tests/modelgen

See `docs/MODELGEN.md#limitation-no-c-load-harness` for why there is no
`load_harness.cpp` here: `LoadModel` in
`src/d3dgraphics-opengl.cpp` is not factored into a standalone function,
and this task's ownership boundary excludes editing engine source to
extract it. `tools/contract/check_x.py` (a line-accurate Python emulation
of the same loader, already used as this project's `.x` fidelity
reference) stands in for it; `tools/tests/test_modelgen.py` runs every
generated model through that parser.

If `LoadModel` is ever refactored into a free function taking a
`FILE*`/path, a real harness would look like:

```cpp
// tests/modelgen/load_harness.cpp (future)
#include "d3dgraphics-opengl.h"
int main(int argc, char** argv) {
    // call the extracted LoadModel(argv[1]) and report vertex/face counts
}
```
