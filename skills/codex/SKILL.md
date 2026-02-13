---
name: codex
description: Execute coding tasks using Claude Code (Codex) in the UnoEngine project
user-invocable: true
---

# Codex Skill for UnoEngine

This skill allows you to execute coding tasks using Claude Code (Codex) specifically for the UnoEngine project.

## Usage

When you need to perform coding tasks in the UnoEngine project:
- Implementing new game engine features
- Fixing bugs in the engine
- Refactoring C++ code
- Building and testing the engine
- Analyzing the codebase

You can use the `codex` tool with the following parameters:

### Parameters

- **prompt** (required): The task description for Codex to execute
- **cwd** (optional): Defaults to the UnoEngine workspace directory
- **model** (optional): The model to use (e.g., "sonnet", "opus", "haiku")
- **approval-policy** (optional): Approval policy for shell commands: `untrusted`, `on-failure`, `on-request`, `never`

### Example

```
Use the codex tool to implement a new rendering pipeline feature in the Engine.
```

## Tool Call

When you want to use Codex, call the `mcp__codex__codex` tool with appropriate parameters.

The working directory will automatically be set to:
```
C:\Users\Unoryuto\Documents\GitHub\UnoEngine
```

For continuing an existing Codex conversation, use `mcp__codex__codex-reply` with the threadId.

## Important Notes

- This agent's workspace is already set to the UnoEngine directory
- Codex has full access to the UnoEngine codebase
- Provide clear, specific task descriptions for best results
- Codex can handle complex multi-step C++ development tasks
- Results will be returned when the Codex session completes
