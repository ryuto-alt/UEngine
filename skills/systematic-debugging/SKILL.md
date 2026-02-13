---
name: systematic-debugging
description: Use when encountering any bug, test failure, or unexpected behavior, before proposing fixes
---

# Systematic Debugging

## Overview

Random fixes waste time and create new bugs. Quick patches mask underlying issues.

**Core principle:** ALWAYS find root cause before attempting fixes. Symptom fixes are failure.

## The Iron Law

```
NO FIXES WITHOUT ROOT CAUSE INVESTIGATION FIRST
```

If you haven't completed Phase 1, you cannot propose fixes.

## When to Use

Use for ANY technical issue:
- Test failures, bugs, unexpected behavior
- Performance problems, build failures
- DX12 validation errors, GPU crashes
- Integration issues

## The Four Phases

### Phase 1: Root Cause Investigation

**BEFORE attempting ANY fix:**

1. **Read Error Messages Carefully** - Don't skip past errors. Read stack traces completely. Note line numbers, file paths, error codes.
2. **Reproduce Consistently** - Can you trigger it reliably? What are the exact steps?
3. **Check Recent Changes** - Git diff, recent commits, new dependencies, config changes.
4. **Gather Evidence in Multi-Component Systems** - Log what data enters/exits each component boundary. Run once to gather evidence showing WHERE it breaks.
5. **Trace Data Flow** - Where does the bad value originate? Keep tracing up until you find the source. Fix at source, not at symptom.

### Phase 2: Pattern Analysis

1. **Find Working Examples** - Locate similar working code in same codebase.
2. **Compare Against References** - Read reference implementation COMPLETELY.
3. **Identify Differences** - List every difference, however small.
4. **Understand Dependencies** - What other components, settings, config, environment?

### Phase 3: Hypothesis and Testing

1. **Form Single Hypothesis** - "I think X is the root cause because Y"
2. **Test Minimally** - SMALLEST possible change. One variable at a time.
3. **Verify Before Continuing** - Didn't work? Form NEW hypothesis. DON'T add more fixes on top.

### Phase 4: Implementation

1. **Create Failing Test Case** - Simplest possible reproduction.
2. **Implement Single Fix** - ONE change at a time. No "while I'm here" improvements.
3. **Verify Fix** - No other tests broken? Issue actually resolved?
4. **If 3+ Fixes Failed: Question Architecture** - Each fix reveals new problem in different place = wrong architecture. Discuss before attempting more fixes.

## Red Flags - STOP and Follow Process

- "Quick fix for now, investigate later"
- "Just try changing X and see if it works"
- "I don't fully understand but this might work"
- Proposing solutions before tracing data flow
- "One more fix attempt" (when already tried 2+)

**ALL of these mean: STOP. Return to Phase 1.**

## Quick Reference

| Phase | Key Activities | Success Criteria |
|-------|---------------|------------------|
| **1. Root Cause** | Read errors, reproduce, check changes, gather evidence | Understand WHAT and WHY |
| **2. Pattern** | Find working examples, compare | Identify differences |
| **3. Hypothesis** | Form theory, test minimally | Confirmed or new hypothesis |
| **4. Implementation** | Create test, fix, verify | Bug resolved, tests pass |
