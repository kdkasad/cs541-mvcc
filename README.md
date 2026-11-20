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

## How to benchmark

Use a Release build (Debug is much slower and prints debug messages). The benchmark prints an
aligned table by default; pass `--csv` for CSV.

```sh
make -S . -B build-release -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
./build-release/cs541_mvcc_bench
./build-release/cs541_mvcc_bench --csv > results.csv
```

The sweep is defined in `bench/main.cpp`. Each run does a fixed number of transactions per
thread rather than running for a fixed time, since there's no GC and memory grows with every
write.

### Known behaviors
- **Throughput drops as threads increase.** Every transaction takes the single
  `TransactionManager` mutex 3 times (begin, commit timestamp, removal). Under contention
  most CPU time goes to the OS putting waiting threads to sleep. Larson et al. use atomic
  counters instead.
- **The 10-key contention run looks better than 100 keys.** It has fewer aborts and higher
  throughput. Threads rarely sleep in the OS there, so transactions finish faster and have
  less time to be invalidated. This reflects mutex/OS scheduling, not MVCC; use 10000 → 100
  keys as the contention curve.
- **Some aborts are due to missing commit dependencies.** `read_miss` aborts, and some
  `validation` aborts, happen when a transaction runs into another one that is preparing.
  Larson et al. would wait instead of aborting.
