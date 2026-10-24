# GEMV and GEMM PHP kernels

Plain PHP matrix kernels for dense f32, Q4_0 and Q8_0 data, run on `Swow\Thread` workers. Weights and results move through `Swow\SharedBuffer`, so a benchmark run touches no temporary file.

## Threads and shared memory

This example demonstrates how far plain PHP can be pushed on a CPU-bound kernel while keeping the thread layer explicit:

1. `Swow\Thread` workers each take a row range of the output, so one matvec or one square GEMM is split without copying the weight matrix.
2. `Swow\SharedBuffer` holds the weights, the input vector and the result matrix. Workers read and write it directly, so nothing is serialized per job and no temporary file is written.
3. The driver owns the timing: it starts the workers, collects their reports, and computes GMAC/s from the measured span.
4. CPU pinning is optional and best effort through FFI: `sched_setaffinity` on Linux, `thread_policy_set` on macOS, `SetThreadAffinityMask` on Windows. When FFI or topology discovery is unavailable the benchmark warns and runs unpinned.
5. `bin/verify.php` checks every kernel against stored vectors, so threaded results are compared numerically, not only timed.

The kernels are ordinary PHP loops, optionally compiled by the tracing JIT, and the benchmark reports the JIT state so the numbers can be read in context.

## Layout

```text
examples/gemm/
  autoload.php
  bin/gemv.php            matvec benchmark
  bin/gemm.php            square GEMM benchmark
  bin/verify.php          vector runner
  src/Format.php          weight format interface
  src/Format/             f32, q4_0 and q8_0 formats
  src/Kernel/             dense f32 matvec and square GEMM
  src/Quantization/       Q4_0 and Q8_0 block codecs and matvec
  src/Bench/              benchmark driver and worker
  src/Support/            thread helper, matrix data, CPU topology
  src/Verification/       vector comparison
```

## Formats

A format answers three questions: how many bytes one row takes, how to generate a matrix, and how to compute a row range of `y = W x`.

| Format | Storage | Kernel |
| --- | --- | --- |
| `f32` | 4 bytes per weight | `Kernel\F32Gemv` |
| `q8_0` | 34 bytes per 32 weights | `Quantization\Q8_0::matvec` |
| `q4_0` | 18 bytes per 32 weights | `Quantization\Q4_0::matvec` |

Q8_0 quantizes the input once, then runs an integer dot per row. The row header and the input scales are parsed once per row, not once per element.

## Usage

```bash
php examples/gemm/bin/gemv.php f32  [k] [n] [threads] [--pin=auto|sysfs|compact]
php examples/gemm/bin/gemv.php q4_0 [k] [n] [threads] [--pin=...]
php examples/gemm/bin/gemv.php q8_0 [k] [n] [threads] [--pin=...]
php examples/gemm/bin/gemv.php q8_0 both [threads]
php examples/gemm/bin/gemm.php [size] [threads] [--efficiency]
php examples/gemm/bin/verify.php <vectors.txt>
```

`gemv.php both` runs the decode shapes `k=1024 n=4096` and `k=1536 n=6144`. The defaults for a single run are `k=1024`, `n=4096` and the number of physical cores. `--warmup=N` and `--iterations=N` control the measurement.

Pinning is best effort and uses FFI:

- Linux: `sched_setaffinity`
- macOS: `thread_policy_set` affinity hint
- Windows: `SetThreadAffinityMask`

If FFI or topology discovery is unavailable, the benchmark prints a warning and runs unpinned. On a virtual machine the guest vCPUs are not bound to host cores, so pinning gives no measurable speedup.

## Method

GEMV:

1. The parent generates the weight matrix once and publishes it in a `SharedBuffer`.
2. The parent starts `threads` workers. Worker `i` reads rows `[i*n/threads, (i+1)*n/threads)` from the shared buffer, warms up, and times each iteration.
3. Each worker writes its median and its result rows into shared buffers.
4. The parent reads the results, checks a few rows against the independent `referenceRow()` path, and reports GMAC/s and GB/s.

GEMM:

1. The parent publishes both f32 matrices in shared buffers.
2. Worker `i` computes a row range with an `ikj` loop and writes the rows back.
3. The parent reports the wall time, each worker's time, and a sampled check.

The GEMV report uses the slowest worker's median. The GEMM report uses the wall time around spawn and join. Both print the PHP, JIT and CPU configuration.

## Verification

```bash
php examples/gemm/bin/verify.php <vectors.txt>
```

The verifier covers `fp16`, `q4_0_dequant`, `q4_0_dot_f32`, `q4_0_dot_q8_0`, `matvec_q4_0`, `q8_0_quant`, `q8_0_dequant`, `q8_0_dot_q8_0`, `matvec_q8_0`, `gemm_q8_0`, and the f32 vector sections.

One MAC is one multiply-accumulate. GEMV uses `k * n` MACs; GEMM uses `size^3`. GFLOP/s is twice GMAC/s. These are plain PHP loops, not BLAS numbers.

## License

Copyright (C) 2026 Yun Dou <dixyes@gmail.com>

You may use, modify and redistribute this demo without attribution.

```text
            DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
                    Version 2, December 2004

 Copyright (C) 2026 Yun Dou <dixyes@gmail.com>

 Everyone is permitted to copy and distribute verbatim or modified
 copies of this license document, and changing it is allowed as long
 as the name is changed.

            DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
   TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION

  0. You just DO WHAT THE FUCK YOU WANT TO.
```
