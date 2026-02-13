# Test Fixing Guide

## Overview

Systematic approach for identifying and resolving test failures through intelligent error categorization.

## Activation

- `/test-fixing` — Start test failure resolution workflow
- Activate when build failures, test suite breakdowns, or CI/CD issues occur

## When to Use

- User reports test/build failures
- CI/CD pipeline breaks
- Multiple compilation errors after refactoring
- DX12 validation layer test failures

## Core Methodology

### Phase 1: Discovery
Execute the build/test suite and catalog all failures by type, affected modules, and underlying causes.

```bash
# Build and capture all errors
"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" UnoEngine.slnx -t:Build -p:Configuration=Debug -p:Platform=x64 -nologo 2>&1
```

### Phase 2: Strategic Grouping
Organize issues by error classification:
- **Linker errors** (LNK2001, LNK2019) — missing symbols, wrong lib paths
- **Compile errors** (C2xxx) — syntax, type mismatches, missing includes
- **HLSL errors** — shader compilation failures
- **Runtime errors** — DX12 validation, resource leaks, GPU hangs

Sequence fixes based on impact scope and logical dependencies.

### Phase 3: Iterative Resolution
For each group (starting with highest impact):
1. Identify root cause
2. Implement fix
3. Verify fix (rebuild affected module)
4. Move to next group

### Phase 4: Prioritization Framework
1. **Infrastructure** — missing includes, broken dependencies, linker setup
2. **API changes** — function signatures, interface changes, renamed symbols
3. **Logic errors** — wrong values, incorrect state transitions, race conditions
4. **Warnings** — treat as errors in strict mode

### Phase 5: Comprehensive Validation
Execute complete build to confirm all issues resolved without regressions.

## Best Practices

- Work through one error category at a time
- Validate each group's resolution before advancing
- Examine recent git changes for context (`git diff`, `git log`)
- Identify common failure patterns (e.g., missing `#include` cascade)
- Maintain focused, minimal changes throughout the process
- Never fix warnings by suppressing them — fix the underlying issue
