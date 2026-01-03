I read the updated implementation plan, and it’s now **very cohesive and hard to mis-implement**. The “drift prevention” work paid off: required vs optional items are clearly separated, bench/full selection is unambiguous, the mapping-table alignment contract is repeated where it matters, and the scheduler behavior is specified with an exact condition. 

Here’s what I’d still tweak (small) and what I think is *already perfect*.

## What’s excellent

* **Milestone 2 now explicitly requires the generated headers + no runtime JSON parsing in firmware.** That closes a major “it works on host” failure mode. 
* **Generator section (5.0b)** is crystal clear: headers are required behavior, flags are optional, subset support is optional-but-planned. 
* **Bench/full selection mechanism** is consistent: `CHROMANCE_BENCH_MODE` with preprocessor selection, and the build flags are stated in 5.11. 
* **Mapping tables contract** is stated in `PixelsMap` and reiterated near `LedOutput` (good redundancy). 
* **FrameScheduler** is specified as “accumulate + reset” with an exact formula (you still use the word “clamp” once, see below). 
* **WLED kill gate** remains the first acceptance criterion with clear pass/fail implications. 

## Small corrections / last nits

### 1) Replace one remaining “clamp” usage with “reset”

In FrameScheduler 5.3, the algorithm block says “Clamp to avoid death spirals” but the formula is a **reset to now**. Just change that single word so there’s zero ambiguity. 

Suggested edit:

* “Reset to avoid death spirals…”

### 2) `next_frame_ms_` initialization note

Right now, the scheduler API doesn’t specify how `next_frame_ms_` is initialized on first call. In practice you’ll do either:

* `next_frame_ms_ = now_ms` on first render, or
* allow it to be 0 and “render immediately, then accumulate.”

Add one sentence like:

* “On first call, treat `next_frame_ms_==0` as ‘render immediately’ and set `next_frame_ms_=now_ms` before accumulation.”

This prevents different implementations in firmware vs native.

### 3) Milestone 2 acceptance criteria: mention header *selection* as well

You already require the headers exist. Consider adding:

* “Bench build includes `chromance_mapping_bench.h` when `CHROMANCE_BENCH_MODE=1`.”

Not strictly necessary, but it makes bench/full switching a tested deliverable.

### 4) Consistency: “platform owns networking” while networking is out of scope

In the scope section you say `platform/` owns networking, but networking is explicitly deferred. That’s fine, but to reduce confusion:

* “platform owns networking (when/if used).”

## Overall verdict

This plan is **ready to execute**. If you make the one-word “clamp→reset” edit and add a note about first-call scheduler initialization, it becomes extremely difficult for an implementation agent to diverge from your intent. 

