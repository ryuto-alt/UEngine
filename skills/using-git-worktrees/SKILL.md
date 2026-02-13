---
name: using-git-worktrees
description: Use when starting feature work that needs isolation from current workspace - creates isolated git worktrees with smart directory selection and safety verification
---

# Using Git Worktrees

## Overview

Git worktrees create isolated workspaces sharing the same repository, allowing work on multiple branches simultaneously without switching.

**Core principle:** Systematic directory selection + safety verification = reliable isolation.

## Directory Selection Process

### 1. Check Existing Directories
```bash
ls -d .worktrees 2>/dev/null
ls -d worktrees 2>/dev/null
```
If found, use that directory. If both exist, `.worktrees/` wins.

### 2. Check CLAUDE.md
```bash
grep -i "worktree.*director" CLAUDE.md 2>/dev/null
```
If preference specified, use it.

### 3. Ask User
If no directory exists and no preference found.

## Safety Verification

**MUST verify directory is ignored before creating worktree:**
```bash
git check-ignore -q .worktrees 2>/dev/null
```
If NOT ignored: Add to .gitignore, commit, then proceed.

## Creation Steps

1. Detect project name: `project=$(basename "$(git rev-parse --show-toplevel)")`
2. Create worktree: `git worktree add "$path" -b "$BRANCH_NAME"`
3. Run project setup (auto-detect from project files)
4. Verify clean baseline (run tests)
5. Report location and status

## Integration

- **Pairs with:** `finishing-a-development-branch` for cleanup after work complete
