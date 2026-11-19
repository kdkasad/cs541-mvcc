# CS 541 MVCC-R project

## Differences from Larson et al.

- Fixed set of keys. The keys and their initial values must be created when the
  database is created. Insert/delete operations are not supported.

## How to build & run

```sh
cmake -S . -B build -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/cs541_mvcc_demo # this also exists but doesn't do much rn
```
