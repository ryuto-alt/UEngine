# UnoEngine - Project Context

## Tech Stack
- **Language**: C++20/23
- **Graphics API**: DirectX 12 Ultimate
- **Shaders**: HLSL (Shader Model 6.6+)
- **Math Library**: DirectXMath (wrapped)
- **Build System**: MSBuild (VS2022, v145 toolset)
- **Platform**: Windows 10/11 (x64)
- **External Dependencies**:
  - DirectXTex (NuGet)
  - Recast/Detour (`external/recast/`)
  - ImGui (`external/imgui/`)

## Architecture Philosophy
- **Abstraction Level**: Medium - DX12を隠蔽せず、ラップして使いやすくする
- **Memory Management**:
  - `ComPtr<T>` for DX12 COM objects
  - `std::unique_ptr` / `std::shared_ptr` for engine objects
  - RAII-based resource lifetime
- **Synchronization**: Explicit barriers, no "magic" fixes
- **Error Handling**: `HRESULT` checking, `std::expected`/`std::optional` (no exceptions)

## Codebase Structure
```
UnoEngine/
├── Engine/
│   ├── Core/           - Application, Camera, Input, Types
│   ├── Graphics/       - DX12 wrapper (Device, Pipeline, Mesh, Texture)
│   ├── Math/           - Vector, Matrix, Quaternion, MathUtils
│   ├── Navigation/     - NavMesh (Recast/Detour integration)
│   ├── Scene/          - GameObject, Component, SceneManager
│   ├── Window/         - Win32 window management
│   └── Utils/          - Utilities
├── Game/
│   └── UI/             - EditorUI (ImGui-based)
├── Shaders/            - HLSL files
├── assets/             - Scenes, textures, models
└── external/           - Third-party libraries
```

## Key Systems
| System | Description |
|--------|-------------|
| Graphics | DX12 Device, SwapChain, Pipeline, Descriptor Heaps |
| Rendering | Forward rendering, PBR materials, Shadow mapping |
| Navigation | Recast-based NavMesh baking, A* pathfinding |
| Scene | ECS-like GameObject/Component architecture |
| Editor | ImGui-based scene editor, inspector, gizmos |
