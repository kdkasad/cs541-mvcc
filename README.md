# CS 541 MVCC-R project

## Differences from Larson et al.

### Intentional
- Fixed set of keys. The keys and their initial values must be created when the
  database is created. Insert/delete operations are not supported.
  - This means phantom detection is not needed.
  - The hash index is replaced with an array indexed by the keys.
- No persistent log storage
- Uses mutexes instead of bit-packing and atomic CAS.
  - This adds a small amount of overhead, but it's a constant difference (doesn't change time complexity), and it's the same overhead for both optimistic and pessimistic transactions, so it shouldn't significantly affect the comparison.
- Only serializable isolation level (no read committed, repeatable read, or snapshot isolation)
- A given transaction can only write to a given key once
  - This simplifies the cases and avoids the need to implement the case where the transaction updates the version it already created.

### Not implemented yet
- Pessimistic (locking) transactions
- Garbage collection
- Dependency tracking for transactions
  - Currently we just abort in cases where Larson et al. would create a dependency and wait.

## How to build & run

```sh
cmake -S . -B build -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/cs541_mvcc_demo # this also exists but doesn't do much rn
```
