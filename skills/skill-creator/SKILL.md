---
name: skill-creator
description: Guide for creating new skills that extend Claude's capabilities with specialized knowledge, workflows, or tool integrations for UnoEngine
---

# Skill Creator

## Overview

Create effective skills for the UnoEngine project.

## Skill Structure

```
skill-name/
├── SKILL.md          (required - frontmatter + instructions)
├── scripts/          (optional - executable code)
├── references/       (optional - documentation for context)
└── assets/           (optional - templates, files for output)
```

## SKILL.md Format

```yaml
---
name: skill-name
description: When to use this skill (this is the trigger mechanism)
---
```

Body: Instructions for using the skill and its resources.

## Core Principles

1. **Concise** - Only add context Claude doesn't already have
2. **Progressive Disclosure** - Metadata always loaded, body on trigger, resources on demand
3. **Appropriate Freedom** - Match specificity to task fragility

## Creation Process

1. Understand the skill with concrete examples
2. Plan reusable contents (scripts, references, assets)
3. Create directory under `skills/`
4. Write SKILL.md with proper frontmatter
5. Add resources as needed
6. Iterate based on real usage

## Do NOT Include

- README.md, CHANGELOG.md, or auxiliary documentation
- User-facing setup guides
- Files not directly needed for the skill's function
