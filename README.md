# SugRingBuffer

An experiment in lock-free single-producer/single-consumer (SPSC) ring buffers,
measuring the effect of low-latency optimizations on throughput.

Two experiments are run:

1. **General optimizations** — a naive implementation vs. an optimized one that
   applies several optimizations at once (buffer representation, index-wrap
   arithmetic, cache-line alignment). This shows the *combined* effect of more
   than just cache alignment.
2. **Cache alignment only** — two implementations that are identical except for
   `alignas` on the read/write indices, to isolate the effect of false sharing
   alone.

## Implementations

Both derive from a common `SPSCInterface<T>` and hold `capacity - 1` items
(one slot is wasted to distinguish full from empty).

The two implementations in **Experiment 1** differ as follows:

| Optimization | `SPSC` (naive) | `AlignedSPSC` (optimized) |
|---|---|---|
| Buffer | `std::vector<std::optional<T>>` | `std::unique_ptr<T[]>` |
| Index wrap | `% capacity` (integer division) | `& (capacity - 1)` (bitmask, power-of-2) |
| Cache-line alignment | none | `alignas(std::hardware_destructive_interference_size)` |

In **Experiment 2**, `SPSC` is changed to match `AlignedSPSC` in every way
except the `alignas`, so the two differ *only* in cache-line alignment.

A separate `size_` atomic counter was originally maintained on every
`push`/`pop` but was removed: it is *true* sharing (written by both threads),
which serializes producer and consumer and dominated the results. `size()` is
now derived from the two indices.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Tests: `ctest --test-dir build`. Benchmark: `build/benchmarks/sug_ringbuffer_bench`.

## Benchmark

```
./build/benchmarks/sug_ringbuffer_bench <aligned|nonaligned> <# items> <capacity> <pin 0|1>
```

With `pin=1`, the producer is pinned to CPU 0 and the consumer to CPU 1 on
Linux (`pthread_setaffinity_np`). Pinning is required for cross-core cache
effects to manifest; it is a no-op on macOS, which has no reliable pinning API.

```bash
perf stat -e cycles,instructions,cache-misses,cache-references \
    ./build/benchmarks/sug_ringbuffer_bench aligned 10000000 4096 1

perf stat -e l2d_cache_inval,l2d_cache_refill,l2d_cache_wb_clean,bus_access \
    ./build/benchmarks/sug_ringbuffer_bench nonaligned 10000000 4096 1
```

---

## Experiment 1: General optimizations

Compares `SPSC` (naive) against `AlignedSPSC` (optimized), so the measured
difference is the *combined* effect of the buffer, index-wrap, and alignment
changes.

Hardware: Raspberry Pi 5 (4× Cortex-A76, private L1/L2, shared L3),
10M items, capacity 4096, pinned.

### Throughput

| Implementation | Time | Throughput |
|---|---|---|
| `AlignedSPSC` | 0.830s | **12.06M items/s** |
| `SPSC` | 1.144s | 8.75M items/s |

### CPU counters

| Counter | Aligned | Nonaligned |
|---|---|---|
| cycles | 3,795,224,371 | 5,336,718,195 |
| instructions | 925,558,205 | 1,652,195,618 |
| insn/cycle (IPC) | 0.24 | 0.31 |
| cache-misses | 42,132,160 (3.17%) | 39,589,553 (2.10%) |
| cache-references | 1,327,251,162 | 1,883,162,596 |

### Coherence counters

| Counter | Aligned | Nonaligned |
|---|---|---|
| l2d_cache_inval | 44,317,041 | 41,145,309 |
| l2d_cache_refill | 43,259,150 | 38,782,108 |
| l2d_cache_wb_clean | 44,319,422 | 41,139,828 |
| bus_access | 354,659,009 | 329,169,545 |

### Findings

1. **The instruction count, not cache alignment, drives the gap.** The optimized
   implementation executes ~0.93B instructions vs ~1.65B for the naive one — a
   1.79× difference from the `%`→`&` and `optional`→raw-array changes. This
   accounts for essentially the entire ~34% time difference.

2. **`cache-misses` does not reveal false sharing.** The generic counter
   measures *local* cache misses, not coherence/invalidation traffic. The
   coherence counters above are essentially equal (slightly higher for aligned),
   showing no false-sharing penalty in the naive version.

3. **True sharing (`size_`) can be worse than false sharing.** Before the
   `size_` counter was removed, the naive implementation was *faster*: its
   `size_` + indices all sat on one cache line, coalescing coherence traffic,
   while the aligned version spread them across three lines. A variable written
   by both threads is a serialization point that `alignas` cannot fix.

---

## Experiment 2: Cache alignment only

Isolates false sharing by making the two implementations identical except for
`alignas` on the read/write indices: `SPSC` uses the same
`std::unique_ptr<T[]>` buffer, `& (capacity - 1)` index wrap (power-of-2), and
`size_t` indices as `AlignedSPSC`. The only remaining difference is whether
`readIdx_`/`writeIdx_` share a cache line.

### Throughput

| Implementation | Time | Throughput |
|---|---|---|
| `AlignedSPSC` | 0.829s | **12.07M items/s** |
| `SPSC` | 1.044s | 9.58M items/s |

Aligned is ~1.26x faster (~26%), or ~21.5 ns/item — the pure false-sharing cost,
since the implementations differ only by `alignas`.

### Coherence counters

| Counter | Aligned | Nonaligned |
|---|---|---|
| l2d_cache_inval | 43,362,384 | 42,005,136 |
| l2d_cache_refill | 42,495,081 | 40,784,385 |
| l2d_cache_wb_clean | 43,369,874 | 41,993,274 |
| bus_access | 347,018,833 | 335,992,525 |

(`remote_access` is 0 for both — it counts cross-cluster traffic, and the Pi 5
is a single cluster.)

### L1/L2 refills (load vs store misses)

| Counter | Aligned | Nonaligned |
|---|---|---|
| l1d_cache_refill_rd (L1 load misses) | 21,877,725 | 14,485,941 |
| l1d_cache_refill_wr (L1 store misses) | 22,931,137 | 27,739,407 |
| l2d_cache_refill_rd (L2 load misses) | 44,814,421 | 41,263,590 |
| l2d_cache_refill_wr (L2 store misses) | 0 | 0 |

### Findings

1. **Avoiding false sharing recovers ~26% throughput** on this hardware. The
   effect is real but smaller than the ~34% gap in Experiment 1, which was
   dominated by instruction-count differences.

2. **False sharing costs latency, not coherence traffic.** The
   invalidation/refill/write-back counts are essentially equal for both
   versions, yet aligned is ~26% faster. The penalty appears as stall time,
   not as extra cache operations.

3. **False sharing shows up as store misses.** Nonaligned has ~21% more L1
   store misses (27.7M vs 22.9M): because `writeIdx_` and `readIdx_` share one
   line, each index store invalidates the other core's copy and the next store
   misses, ping-ponging the line between cores. Load misses shift the other
   way (aligned higher) as a secondary effect, so the *total* miss count is
   nearly equal — only the *kind* of miss changes.

4. `l2d_cache_refill_wr = 0` for both: a write-back cache allocates a line on
   a store miss without refilling it (the whole line is about to be
   overwritten), so store misses appear at L1 (write-allocate) but not as L2
   refills.
