# Root Cause Tracing

## Overview

Bugs often manifest deep in the call stack. Your instinct is to fix where the error appears, but that's treating a symptom.

**Core principle:** Trace backward through the call chain until you find the original trigger, then fix at the source.

## Activation

- `/root-cause-tracing` — Start root cause investigation
- Activate when error appears deep in execution stack

## When to Use

- Error happens deep in execution (not at entry point)
- Stack trace shows long call chain
- Unclear where invalid data originated
- DX12 validation layer errors with unclear origin
- GPU hangs or resource state corruption

## The Tracing Process

### Phase 1: Observe the Symptom
Document the exact error, including any DX12 validation messages, HRESULT codes, or GPU crash dumps.

### Phase 2: Find Immediate Cause
Identify the code that directly causes the error. For DX12, check:
- Resource barrier states
- Descriptor heap indices
- Command list/queue synchronization
- Root signature mismatches

### Phase 3: Trace Backwards
Ask "What called this?" repeatedly. Follow the call chain upward:
```
ResourceBarrier failed → SetResourceState() → BeginRenderPass() → FrameGraph::Execute() → ???
```

### Phase 4: Keep Tracing Up
Track parameter values at each level. Look for:
- Uninitialized values
- Stale pointers/handles
- Wrong enum values passed through layers
- Race conditions between CPU/GPU timelines

### Phase 5: Find Original Trigger
The root cause is where the invalid state was first introduced, not where it was detected.

## Adding Instrumentation

When manual tracing isn't enough:

```cpp
// Before the problematic operation
void TransitionResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
    OutputDebugStringA(std::format(
        "TRACE: Resource {} transition {} -> {} at {}\n",
        reinterpret_cast<uintptr_t>(resource),
        static_cast<int>(before),
        static_cast<int>(after),
        __FUNCTION__
    ).c_str());
}
```

**For DX12:** Enable the Debug Layer and GPU-Based Validation for detailed error context.

## Defense-in-Depth

After finding the root cause:
1. Fix at the source
2. Add validation at each layer the bad data passed through
3. Use `assert()` or `std::expected` for internal invariants
4. Add DX12 debug layer checks in debug builds

## Key Principle

**NEVER fix just where the error appears.** Trace back to find the original trigger, then add validation at every layer to make the bug structurally impossible.
