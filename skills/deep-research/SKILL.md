---
name: deep-research
description: Use when needing in-depth research on DX12 APIs, HLSL specs, C++ standards, GPU architecture, or any technical topic requiring multi-source investigation
---

# Deep Research

## Overview

Perform autonomous multi-step research using web search and documentation fetching to gather comprehensive information on technical topics.

## When to Use

- DX12 API specifications and best practices
- HLSL Shader Model 6.6+ features and syntax
- C++20/23 standard library features
- GPU vendor-specific documentation (AMD, NVIDIA)
- Performance optimization techniques
- Third-party library documentation (ImGui, Recast, Lua, etc.)

## Workflow

1. **Define Research Question** - Be specific about what information is needed
2. **Multi-Source Search** - Use `braveMCP` for web search, `WebFetch` for specific documentation pages
3. **Cross-Reference** - Verify information across multiple authoritative sources
4. **Synthesize** - Compile findings into actionable information
5. **Record** - Use `serenaMCP` to store architectural decisions and findings

## Best Practices

- Always prefer official documentation (Microsoft Docs, Khronos, cppreference)
- Cross-reference vendor-specific advice with standards
- Note version-specific behavior differences
- Record findings in serenaMCP memory for future reference
