# UnoEngine - System Patterns

## Naming Conventions

### Files
| Type | Convention | Example |
|------|------------|---------|
| Header | PascalCase | `GraphicsDevice.h` |
| Source | PascalCase | `GraphicsDevice.cpp` |
| Shader | PascalCase + Stage | `BasicVS.hlsl`, `ShadowPS.hlsl` |

### Code
| Type | Convention | Example |
|------|------------|---------|
| Class/Struct | PascalCase | `GraphicsDevice`, `TransformCB` |
| Function/Method | PascalCase | `Initialize()`, `GetDevice()` |
| Local Variable | camelCase | `device`, `cmdList` |
| Member Variable | m_camelCase | `m_pipeline`, `m_vertexBuffer` |
| Constant | UPPER_SNAKE | `BACK_BUFFER_COUNT`, `PI` |
| Namespace | PascalCase | `UnoEngine::Math` |

## Type Aliases
```cpp
// Fundamental types
using uint8 = std::uint8_t;
using uint32 = std::uint32_t;
// ... etc

// Smart pointers
template<typename T> using UniquePtr = std::unique_ptr<T>;
template<typename T> using SharedPtr = std::shared_ptr<T>;
```

## Class Structure Template
```cpp
class ClassName {
public:
    ClassName() = default;
    ~ClassName() = default;

    // No copy, allow move
    ClassName(const ClassName&) = delete;
    ClassName& operator=(const ClassName&) = delete;
    ClassName(ClassName&&) = default;
    ClassName& operator=(ClassName&&) = default;

    void PublicMethod();
    Type GetValue() const { return m_value; }

private:
    void PrivateMethod();

    Type m_value;
    ComPtr<ID3D12Device> m_device;
};
```

## Include Order
1. Corresponding header (for .cpp)
2. Engine headers (`"Engine/Core/Types.h"`)
3. System headers (`<d3d12.h>`)
4. Standard library (`<vector>`)

## Comments
- **Language**: 日本語OK
- **Style**: `// 単行コメント`のみ
- **Philosophy**:
  - 自明なコードにはコメント不要
  - "Why"を説明する（"What"ではない）
  - 削除したコードはコメントアウトせずGitで管理

## DX12 Patterns

### Descriptor Heaps
- Ring-buffer allocation for dynamic descriptors
- Shader-visible heaps for bindless rendering

### Resource Barriers
```cpp
// 必ず明示的に状態遷移
CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
    resource.Get(),
    D3D12_RESOURCE_STATE_RENDER_TARGET,
    D3D12_RESOURCE_STATE_PRESENT
);
cmdList->ResourceBarrier(1, &barrier);
```

### Constant Buffer Alignment
- CBV: 256 bytes alignment
- Structs must match HLSL packing rules

## Shader Conventions

### HLSL Structure Matching
```hlsl
// HLSL
cbuffer TransformCB : register(b0) {
    float4x4 World;      // 64 bytes
    float4x4 View;       // 64 bytes
    float4x4 Projection; // 64 bytes
}; // Total: 192 bytes → padded to 256
```

```cpp
// C++ (must match)
struct alignas(256) TransformCB {
    DirectX::XMFLOAT4X4 World;
    DirectX::XMFLOAT4X4 View;
    DirectX::XMFLOAT4X4 Projection;
};
```

## Forbidden Patterns
- `new`/`delete` (use smart pointers)
- `NULL` (use `nullptr`)
- C-style arrays (use `std::array`, `std::span`)
- `typedef` (use `using`)
- `try-catch` (use `HRESULT`, `std::expected`)
- Raw owning pointers

---
*Reference: CLAUDE.md for full directives*
