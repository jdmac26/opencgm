# CGM Engine Test Suite

This directory contains unit and integration tests for the CGM to SVG converter engine.

## Building and Running Tests

### Prerequisites
- CMake 3.15 or later
- C++17 compatible compiler
- Internet connection (for downloading Google Test on first build)

### Build with Tests

```bash
cd engine
mkdir build
cd build
cmake -DBUILD_TESTS=ON ..
cmake --build .
```

### Run Tests

```bash
# Run all tests
ctest

# Or run the test executable directly
./bin/opencgm_tests

# Run with verbose output
ctest --verbose

# Run specific test
./bin/opencgm_tests --gtest_filter=ColorScalingTest.*
```

## Test Organization

- `test_color_scaling.cpp` - Tests for color precision scaling (validates the memory leak fix)
- `test_binary_reader.cpp` - Basic tests for binary CGM parsing
- `test_parameterized_suite.cpp` - Broad sample-corpus smoke coverage (parse, convert, round-trip)
- `test_conversion_artifacts.cpp` - Golden artifact regression checks for SVG output and native conversion reports
- `golden/conversion_cases.json` - Fixture manifest for artifact-based regression cases

## Adding New Tests

1. Create a new `.cpp` file in this directory
2. Include `<gtest/gtest.h>` and relevant CGM headers
3. Write tests using Google Test macros (TEST, EXPECT_EQ, etc.)
4. Add the file to `CMakeLists.txt` in the `cgm_tests` executable sources

For artifact regression cases, prefer adding or updating entries in `golden/conversion_cases.json`
instead of hardcoding more file-specific assertions directly in C++. The fixture manifest is intended
to hold:

- the corpus/sample input to exercise
- expected profile detection where stable
- required SVG fragments
- required report text fragments
- minimum report summary counts for geometry, raster, semantics, and issue totals

Example:
```cpp
#include <gtest/gtest.h>
#include "opencgm/cgm_file.h"

TEST(MyTestSuite, MyTestCase) {
    // Test code here
    EXPECT_EQ(1 + 1, 2);
}
```

## Test Coverage Goals

The test suite should cover:
- [x] Color scaling functions (memory safety)
- [x] C API smoke coverage
- [x] Sample corpus parse/convert smoke coverage
- [x] Golden SVG/report artifact regression harness
- [ ] Binary reader integer parsing (8, 16, 24, 32-bit)
- [ ] Binary reader floating-point parsing
- [ ] VDC coordinate reading
- [ ] Character encoding conversion
- [ ] Command parsing and factory
- [ ] Golden validation JSON regression fixtures
- [ ] Coordinate transformations
- [ ] Expanded golden SVG baselines for more corpora
- [ ] WebCGM 2.1 compliance corpus with profile-specific expectations

## Continuous Integration

To integrate with CI systems:

```yaml
# Example for GitHub Actions
- name: Build and Test
  run: |
    cmake -DBUILD_TESTS=ON -B build
    cmake --build build
    cd build && ctest --output-on-failure
```

## References

- [Google Test Documentation](https://google.github.io/googletest/)
- [CMake Testing](https://cmake.org/cmake/help/latest/manual/ctest.1.html)
