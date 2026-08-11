# SOTA MICRO-MODELS 2026 — THE SEVEN-HOP RESEARCH

*Seven sequential research hops, each building on the last, to
find the best models that fit the Distiller One CM4 (A72, 7.6GB
RAM, no dotprod) — and the CM5 (A76, 8GB). The v1 currently
ships 2024 models (TinyLlama-140M, stablelm-1.6B, bling-1.3B).
Everything below is the 2026 replacement stack.*

---

## HOP 1 — The small-model leaderboard (2026)

The 2026 SLM field (Turing Post, localaimaster, bentoml,
promptquorum — all current as of Jul-Aug 2026):

| Model | Size | MMLU | Notes |
|---|---|---|---|
| Phi-4-mini | 3.8B | 67.3% | 128K ctx, strong math/code |
| Gemma 4 E2B | ~2.3B eff | — | Pi-class, 128K ctx, ~2GB RAM |
| **Qwen3.5-4B** | 4B | ~70% | multimodal, 256K ctx |
| **Qwen3.5-2B** | 2B | — | multimodal, 256K ctx |
| **Qwen3.5-0.8B** | 0.8B | — | **multimodal**, iPhone-runnable |
| SmolLM3-3B | 3B | — | fully open, 128K ctx |
| Ministral-3-3B | 3B | — | Apache 2.0, edge |

## HOP 2 — Qwen3.5-0.8B deep dive (THE daily-agent pick)

- **2026 refresh of the Qwen3 family** — 0.8B, 2B, 4B, 9B small
  + 35B-A3B MoE, 27B, 122B-A10B, 397B-A17B medium.
- **Multimodal BY DEFAULT**: text + image input (vision!),
  non-thinking mode default, thinking switchable.
- **iPhone 14 Pro verified** (Better Stack, Mar 2026): runs 100%
  offline, agentic coding + vision tests.
- GGUF available (unsloth Dynamic 2.0 quants — SOTA quant
  quality per 150+ KL-divergence benchmarks over 9TB of GGUFs).
- ~500MB (ollama qwen3.5:0.8b) — fits the CM4 comfortably.

## HOP 3 — The sub-500M tier (for the tightest devices)

- **SmolLM2-135M / 360M** (HuggingFace) still the sub-500M
  champs; SmolLM-135M beats MobileLM-125M at 40% fewer tokens.
- The v1 already runs TinyLlama-140M (2024) — SmolLM2-135M is
  the same class, newer, better trained.
- Honest ceiling: 135M-class models do classification,
  extraction, rewriting — not deep reasoning. **The 0.8B class
  is the real daily-agent floor.**

## HOP 4 — Speed reality on the CM4

- Qwen3.5-0.8B: ~403 t/s on API hardware; ollama "runs on
  almost anything", ~500MB.
- No direct CM4 A72 benchmark exists publicly — the CM4 (A72
  1.5GHz) is ~2-3x slower than the CM5 (A76 2.4GHz, PamirAI
  measured Qwen3-0.6B Q8 at 21.5 t/s). Projection for
  Qwen3.5-0.8B Q8 on the CM4: **~8-10 t/s** — usable.
- The v1's current stablelm-1.6B stack runs ~2 t/s. The 0.8B
  is both SMALLER and FASTER than what the device ships.

## HOP 5 — Speech: ASR + TTS 2026 (the ai slots' payload)

- **Moonshine** (Useful Sensors) — 27MB minimum, purpose-built
  for Raspberry Pi edge ASR. The v1 ASR pick (vs the v2 SDK's
  parakeet at ~600MB).
- **Parakeet-TDT-0.6B-v3** (NVIDIA) — now 25 EU languages,
  6.34% WER (top of Open ASR leaderboard), RTFx >2000. The CM5
  pick (v2 already ships parakeet).
- **Qwen3-ASR-0.6B** — Apache 2.0 alternative.
- **TTS tiny tier**: KittenTTS nano/micro (edge-friendly),
  MOSS-TTS-Nano-100M (multilingual voice-cloning), Piper
  (10-100MB, "surprisingly good", the v2's pick).

## HOP 6 — Vision: the surprise (multimodal 0.8B)

- **Qwen3.5-0.8B is a VLM** — MMMU 49, MathVista 62.2, OCRBench
  74.5, AI2D 69.9. The Distiller's camera gets vision from the
  SAME model that does chat — no separate VLM needed.
- Qwen3.5-2B: MMMU 64.2, OCRBench 84.5, MMLongBench-Doc 45.4 —
  the CM5's vision tier.
- SmolVLM2-256M exists for the truly tiny tier.
- Penguin-VL (arxiv 2603.06569) shows the efficiency frontier.

## HOP 7 — Quantization science for the A72 (no dotprod)

- Q8_0: ~99.5% of FP16, under 0.5% loss — the quality ceiling.
- Q4_K_M: ~96.5%, 1-3% loss, ~half the size — best balance.
- K_M variants matter MOST for models <8B (Kaitchup) — always
  prefer Q4_K_M over Q4_0 at the same bits.
- imatrix IQ variants (IQ4_XS) can match Q4_K_M quality smaller
  when a good imatrix exists.
- ARM mobile guidance (Arm blog): 8-bit ~no perplexity impact,
  4-bit minor — quantization is safe on edge CPUs.
- **For the A72 (no dotprod): Q8_0 and Q4_K_M are the two
  formats. Both are pure-NEON friendly (no SDOT required).**

---

## THE DECISION — the 2026 stack for BOTH devices

### v1 (CM4, 7.6GB RAM, A72) — REVISED (LFM2.5 added)
| Role | Model | Quant | RAM | Notes |
|---|---|---|---|---|
| **AGI agent (NEW)** | **LFM2.5-2.6B** | Q4_K_M | ~1.7GB | agentic: planning + tool calling + multi-step; beats Gemma-4-E2B/E4B and Qwen3.5-4B on instruction-following + tool use; 128K ctx, 16 languages; Agentic RL trained inside Hermes Agent + OpenClaw harnesses; day-one llama.cpp GGUF |
| Daily agent (fast tier) | Qwen3.5-0.8B | Q8_0 | ~0.9GB | multimodal: chat + VISION + audio in one; ~500MB model |
| Reasoner | Qwen3.5-2B | Q4_K_M | ~1.5GB | 256K ctx, vision |
| ASR | **Moonshine** | — | ~27MB | Pi-class edge ASR (vs parakeet 600MB) |
| TTS | Piper or KittenTTS-micro | — | <100MB | |
| **Total** | | | **~4.2GB of 7.6GB** | agent + vision + speech, leaves room |

### CM5 (A76, 8GB) — REVISED
| Role | Model | Quant | Notes |
|---|---|---|---|
| AGI agent | LFM2.5-2.6B | Q8_0 | ~2.9GB, max quality |
| Daily agent | Qwen3.5-2B | Q8_0 | ~2GB, multimodal |
| Reasoner | Qwen3.5-4B | Q4_K_M | ~2.5GB, 256K ctx |
| Big model | Qwen3.5-9B | Q4_K_M | ~5GB, optional |
| ASR | Parakeet-TDT-0.6B-v3 | — | 25 languages, 6.34% WER |
| TTS | Piper / KittenTTS | — | |

### Why LFM2.5-2.6B is the AGI pick
- **It was trained in our world**: Agentic RL ran inside Hermes
  Agent and OpenClaw harnesses — the exact agent ecosystem we
  run. It is literally an agent for this environment.
- **Powerhouse for its size** (user's words, confirmed): beats
  models 2-3x its size on IFBench (59.17 vs Gemma E2B 34.08),
  Multi-IF (80.07), IFStruct (85.49), ToolSandbox (77.83),
  Claw-Eval (62.85) — and trails only Qwen3.5-9B on BFCLv4.
- **Fast on CPU**: 30 t/s on a phone; under 2.5GB; Q4_K_M
  1.67GB fits the CM4 with room for Qwen3.5-0.8B (vision) +
  Moonshine (ASR) + Piper (TTS).
- **The v1 becomes a real AGENT device**: plans, calls tools,
  multi-step tasks, on-device, private, free inference.

### Next actions
- [x] Swap the CM4 model download to the research winners
- [ ] Download LFM2.5-2.6B Q4_K_M (1.67GB) + Qwen3.5-0.8B Q8_0
- [ ] Deploy via the distiller-c11 ai slots (registry + gui)
- [ ] llama-bench on the CM4 when the build finishes
