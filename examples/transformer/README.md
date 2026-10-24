# Transformer model assets

Download Qwen3.5-0.8B GGUF files for the PHP inference experiments.

## Threads and cooperative inference

The inference side demonstrates a cooperative thread pool for a CPU-bound workload where every token depends on the previous one:

1. `Swow\Thread` workers run the whole forward pass. The parent does not schedule matrix tiles per token; it writes one control block into `Swow\SharedBuffer`, wakes the workers once, and waits at the token barrier.
2. `Swow\Futex` carries the wake-up and the command state between the parent and the pool.
3. `Swow\Sync\ThreadBarrier` synchronizes the workers at each token boundary, so a worker never reads activations that another worker has not finished writing.
4. `Swow\SharedBuffer` holds the model weights, the KV cache and the activation buffers. Workers address them directly; nothing is copied per token.
5. The same code path runs single threaded without the extension, so the pool can be compared against a plain sequential forward.

Together with `examples/gemm`, this shows the two shapes of CPU-bound threading in plain PHP: independent row ranges (GEMM) and a barrier-synchronized cooperative step (transformer).

## Sources

The files use the `prithivMLmods/Qwen3.5-0.8B-f32-GGUF` repository, which is mirrored on Hugging Face and ModelScope:

| Quant | File | Size | SHA-256 |
| --- | --- | ---: | --- |
| F32 | `Qwen3.5-0.8B.F32.gguf` | 3,020,533,664 | `53d5c025769385c3c189cba43be43af53c4c8325dcf29497b44a6cadd8437c23` |
| Q8_0 | `Qwen3.5-0.8B.Q8_0.gguf` | 811,843,488 | `367e9bd0383d7161898c996a58f9a36e6063585d92ae2e5be62e3def0ac1375c` |

The model is Apache-2.0 licensed. These are third-party GGUF conversions; check the upstream repository before redistributing them.

## Usage

Show URLs and hashes:

```bash
php examples/transformer/bin/model.php list
```

Download both files from Hugging Face:

```bash
php examples/transformer/bin/model.php download --source=hf
```

Download only Q8_0 from ModelScope:

```bash
php examples/transformer/bin/model.php download --source=modelscope --quant=q8
```

The default output directory is `examples/transformer/models/qwen3.5-0.8b`. The directory is ignored by Git.

Options:

```text
--source=hf|modelscope  default hf
--quant=f32|q8|both     default both
--output=DIR            output directory
--proxy=URL             default HTTPS_PROXY or ALL_PROXY
--token=TOKEN           default HF_TOKEN or MODELSCOPE_API_TOKEN
--force                 remove and download again
--list                  print URLs and hashes only
```

Downloads are resumable. The script writes a `.part` file, checks the final size and SHA-256, then renames it into place. A proxy can also be supplied through the usual `HTTPS_PROXY` or `ALL_PROXY` environment variables.

## GGUF reader

The reader is under `src/Gguf` and is independent of the graph implementation.

```php
use Swow\Examples\Transformer\Gguf\GgufFile;

$file = GgufFile::open('model.gguf');
$info = $file->tensor('blk.0.attn_qkv.weight');
$row = $file->row('blk.0.attn_qkv.weight', 0);
$rows = $file->rows('blk.0.attn_qkv.weight', 0, 8);
$file->close();
```

`GgufFile` implements `WeightStore`. It exposes tensor shape, type, absolute data offset, row size and byte size, so a future mmap or FFI backend can reuse the same tensor directory without copying the whole file.

Inspect metadata and tensor names:

```bash
php examples/transformer/bin/model.php info \
  examples/transformer/models/qwen3.5-0.8b/Qwen3.5-0.8B.Q8_0.gguf

php examples/transformer/bin/model.php info \
  examples/transformer/models/qwen3.5-0.8b/Qwen3.5-0.8B.Q8_0.gguf \
  --tensors
```

Large tokenizer metadata arrays are skipped during parsing and exposed as `GgufArrayRef`. Call `toArray()` only when the tokenizer needs them.

Run the reader tests:

```bash
php vendor/bin/phpunit \
  --bootstrap examples/transformer/autoload.php \
  examples/transformer/tests/GgufFileTest.php
```

Supported tensor types: F32, F16, BF16, Q4_0, Q8_0. BF16 is readable but has no kernel yet.

## Dynamic graph

`src/Graph` is a define-by-run graph. Every operation executes immediately and records an `Op` with static shape/dtype metadata.

```php
use Swow\Examples\Transformer\Graph\Graph;
use Swow\Examples\Transformer\Graph\Shape;
use Swow\Examples\Transformer\Graph\Weight;

$graph = new Graph();
$x = $graph->input('x', new Shape(1024), $input);
$gate = $graph->linear($x, Weight::fromGguf($file, 'blk.0.ffn_gate.weight'));
$up = $graph->linear($x, Weight::fromGguf($file, 'blk.0.ffn_up.weight'));
$out = $graph->linear($graph->swiGlu($gate, $up), Weight::fromGguf($file, 'blk.0.ffn_down.weight'));
echo $graph->dump();
```

Kernels are separate classes registered in `KernelRegistry`. The trace contains input/output ids, shapes, dtypes, weight names and attrs, so a future JIT planner can partition the trace without changing graph semantics.

## Full decode model

`src/Model/Qwen35Model.php` implements the text-only decode path:

- token embedding row
- 24 blocks
- 18 Gated DeltaNet layers
- 6 full attention layers
- FFN and final RMSNorm
- tied token embedding logits

Run token ids directly:

```bash
USE_ZEND_ALLOC=0 php -d memory_limit=-1 \
  examples/transformer/bin/generate.php \
  examples/transformer/models/qwen3.5-0.8b/Qwen3.5-0.8B.F32.gguf \
  --ids=100,200 --tokens=0 --output=build/php-f32
```

`USE_ZEND_ALLOC=0` is recommended during development; the model weights are large.

The F32 model matches llama.cpp on tokens 100 and 200:

```text
token 100: max_abs 8.7e-06, top1/top5 match
token 200: max_abs 5.5e-06, top1/top5 match
```

Q8_0 is supported by the same code path. Use a Q8 llama.cpp run as the reference; do not compare Q8 logits with F32 logits.

Current Q8 comparison for token 100, Q8 model:

```text
PHP vs llama.cpp: top1 match, top5 differs, max_abs 4.76e-01
```

For token 200:

```text
PHP vs llama.cpp: top1 differs (126 vs 14618)
```

Early Q8 layers agree closely (layer 0 and 1 within 1e-7), then quantization flips accumulate. The remaining Q8 work is float32 accumulation and activation-quantization rounding alignment, not graph structure. The F32 path already matches llama.cpp.

Run the slow full-model test:

```bash
RUN_QWEN35_SLOW=1 USE_ZEND_ALLOC=0 php -d memory_limit=-1 \
  vendor/bin/phpunit \
  --bootstrap examples/transformer/autoload.php \
  examples/transformer/tests/Qwen35ModelTest.php
```

## Sampler

`src/Sampler` implements greedy, temperature, top-k, top-p, min-p and token penalties. The random source is seeded and does not use PHP's global RNG.

```php
use Swow\Examples\Transformer\Sampler\Sampler;
use Swow\Examples\Transformer\Sampler\SamplingConfig;

$sampler = new Sampler(new SamplingConfig(
    topK: 40,
    topP: 0.95,
    minP: 0.05,
    temperature: 0.8,
    seed: 123,
));
$token = $sampler->sample($logits);
```

Command-line check from raw logits:

```bash
php examples/transformer/bin/inspect.php sample logits_00.bin --greedy
```

## Tokenizer

`src/Tokenizer` implements Qwen byte-level BPE from GGUF metadata.

```php
use Swow\Examples\Transformer\Tokenizer\QwenTokenizer;

$tokenizer = QwenTokenizer::fromGguf($file);
$ids = $tokenizer->encode('Hello 你好');
$text = $tokenizer->decode($ids);
```

Command-line check:

```bash
USE_ZEND_ALLOC=0 php -d memory_limit=-1 \
  examples/transformer/bin/inspect.php tokenize \
  examples/transformer/models/qwen3.5-0.8b/Qwen3.5-0.8B.Q8_0.gguf \
  'Hello 你好'
```

Known IDs match llama-tokenize:

```text
Hello                  [9419]
Hello world            [9419, 1814]
你好                   [109266]
Hello 你好 😀          [9419, 220, 109266, 87209]
a\nb                   [64, 198, 65]
```

Run tests:

```bash
USE_ZEND_ALLOC=0 php -d memory_limit=-1 \
  vendor/bin/phpunit \
  --bootstrap examples/transformer/autoload.php \
  examples/transformer/tests/SamplerTest.php \
  examples/transformer/tests/TokenizerTest.php
```

## ChatML and generation

`src/Chat/QwenChatML.php` renders the text part of the Qwen chat template:

- system must be first
- user and assistant messages
- assistant reasoning content
- `enable_thinking` chooses `<think>` or an empty thinking block
- generation prompt is added after the messages

`bin/chat.php` runs an interactive TTY session:

```bash
USE_ZEND_ALLOC=0 php -d memory_limit=-1 \
  examples/transformer/bin/chat.php \
  examples/transformer/models/qwen3.5-0.8b/Qwen3.5-0.8B.F32.gguf \
  --context=2048 --tokens=32 --system='You are a helpful assistant'
```

Options:

```text
--context=N     context token limit, default 2048
--tokens=N      maximum generated tokens per turn, default 32
--system=TEXT   system message, default "You are a helpful assistant"
--no-system     disable the system message
--thinking      use <think> generation prompt
--greedy        greedy sampling
--temp=N        temperature
--top-k=N       top-k
--top-p=N       top-p
--min-p=N       min-p
--seed=N        sampler seed
```

The terminal interface is implemented in `src/Terminal/XtermTerminal.php`. It puts the TTY in raw mode, handles UTF-8 backspace, and restores the saved stty settings on exit. Ctrl-C cancels the current input line, Ctrl-D closes the session.

Each user turn is appended to the existing model state using `QwenChatML::nextUserTurn()`. The previous generated tokens stay in the KV and GDN state; the next turn feeds only the closing `<|im_end|>`, the new user message, and the next assistant generation prompt. Batching is still a later prefill optimization.

`Conversation` depends on the `Inferencer` interface, not on `Qwen35Model` directly. `CooperativePool` is the real backend; tests inject a fake backend that returns fixed logits. This separates session, ChatML, sampler, and TUI tests from slow model inference.

## Inference demos

```bash
php examples/transformer/bin/generate.php <model.gguf> --prompt=hi --tokens=9 --threads=8
php examples/transformer/bin/chat.php <model.gguf> --threads=8
```

`generate.php` is the forward demo and prints per-token timings. `chat.php` is the interactive TUI.

Workers run the whole forward and synchronize through shared memory, a futex, and barriers. The parent only submits a token and waits for the result.

Tools:

- `generate.php --ids=... --tokens=0 --output=PREFIX` dumps logits for given token ids
- `model.php list|download|info` manages model files and prints metadata
- `inspect.php tokenize|sample|compare` checks the tokenizer, sampler and logits


### Chat smoke test

```text
prompt: Hi
chatml prompt tokens: 13
generated tokens: [9419, 0]
generated text: "Hello!"
```

The same prompt text passed to `llama-completion --no-conversation` produces `Hello!` as well, so ChatML rendering and the greedy path agree for this case.

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
