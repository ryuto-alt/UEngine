---
name: git-pushing
description: Use when staging, committing, and pushing changes to the remote branch with conventional commit messages
---

# Git Pushing

## Overview

Stage all changes, create a conventional commit, and push to the remote branch.

## Workflow

1. Stage modifications
2. Generate conventional commit message
3. Push with `-u` flag to set upstream tracking

## Conventional Commit Format

```
<type>(<scope>): <description>

[optional body]

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>
```

### Types
- `feat`: New feature
- `fix`: Bug fix
- `refactor`: Code restructuring
- `perf`: Performance improvement
- `docs`: Documentation
- `build`: Build system changes
- `chore`: Maintenance

### Scopes (UnoEngine)
- `renderer`, `dx12`, `hlsl`, `editor`, `navmesh`, `audio`, `input`, `scripting`, `particle`, `postprocess`, `ui`, `scene`, `animation`, `core`
