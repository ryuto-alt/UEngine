---
name: article-extractor
description: Use when needing to extract clean article content from URLs - removes navigation, ads, and clutter to get readable text from technical articles and documentation
---

# Article Extractor

## Overview

Extract clean article content from URLs, removing clutter like navigation, ads, and sidebars.

## Extraction Methods (Priority Order)

1. **WebFetch** - Use Claude's built-in WebFetch tool to retrieve and process page content
2. **reader** (Mozilla Readability) - Excellent clutter removal
3. **trafilatura** (Python) - Effective for blogs and news
4. **Fallback** (curl + basic parsing) - Works without dependencies

## Workflow

1. Fetch URL content using WebFetch
2. Extract main article text
3. Clean formatting (remove nav, ads, sidebars, cookie notices)
4. Preserve: title, author, main text, section headings
5. Present summary and key content to user

## Use Cases for UnoEngine

- Microsoft DirectX 12 documentation
- GPU vendor blog posts (AMD GPUOpen, NVIDIA Developer)
- C++ standards proposals and articles
- Game engine architecture articles
- HLSL shader programming guides
