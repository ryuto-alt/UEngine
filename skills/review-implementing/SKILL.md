---
name: review-implementing
description: Use when processing code review feedback and implementing suggested changes from PR reviews or code audits
---

# Review Feedback Implementation

## Overview

Systematically process code review feedback and implement suggested changes.

## Workflow

### 1. Parse & Plan
- Identify individual feedback items from reviewer notes
- Create actionable todo list with specific, measurable tasks

### 2. Implement
For each task:
- Locate relevant code using search tools
- Make changes following project conventions (C++20/23, PascalCase, etc.)
- Verify correctness
- Mark item as completed immediately

### 3. Feedback Categories

| Type | Approach |
|------|----------|
| Code modifications | Edit in-place, verify compilation intent |
| New features | Follow `!plan` workflow first |
| Refactoring | Ensure backward compatibility or update all references |
| Tests | Add test cases covering the feedback |

### 4. Edge Cases
- **Conflicting feedback**: Ask for clarification
- **Breaking changes**: Flag to reviewer
- **Test failures**: Fix before marking complete
- **Missing code**: Report to reviewer

## Rules
- One `in_progress` todo at a time
- Update status immediately, don't batch
- Ask clarifying questions when feedback is ambiguous
