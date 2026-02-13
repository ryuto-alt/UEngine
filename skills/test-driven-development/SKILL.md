---
name: test-driven-development
description: Use when implementing any feature or bugfix, before writing implementation code
---

# Test-Driven Development (TDD)

## Overview

Write the test first. Watch it fail. Write minimal code to pass.

**Core principle:** If you didn't watch the test fail, you don't know if it tests the right thing.

## The Iron Law

```
NO PRODUCTION CODE WITHOUT A FAILING TEST FIRST
```

Write code before the test? Delete it. Start over.

## When to Use

**Always:** New features, bug fixes, refactoring, behavior changes.

**Exceptions (ask user):** Throwaway prototypes, generated code, configuration files.

## Red-Green-Refactor

### RED - Write Failing Test
- One behavior per test
- Clear name describing the behavior
- Real code (no mocks unless unavoidable)

### Verify RED - Watch It Fail (MANDATORY)
- Test fails (not errors)
- Failure message is expected
- Fails because feature missing (not typos)

### GREEN - Minimal Code
- Simplest code to pass the test
- Don't add features, refactor other code, or "improve" beyond the test

### Verify GREEN - Watch It Pass (MANDATORY)
- Test passes
- Other tests still pass
- Output pristine (no errors, warnings)

### REFACTOR - Clean Up
- Remove duplication, improve names, extract helpers
- Keep tests green. Don't add behavior.

## Red Flags - STOP and Start Over

- Code before test
- Test after implementation
- Test passes immediately
- "I'll write tests after"
- Rationalizing "just this once"

**All of these mean: Delete code. Start over with TDD.**

## Verification Checklist

- [ ] Every new function/method has a test
- [ ] Watched each test fail before implementing
- [ ] Each test failed for expected reason
- [ ] Wrote minimal code to pass each test
- [ ] All tests pass
- [ ] Edge cases and errors covered
