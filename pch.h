#pragma once

// ==================================================
// UnoEngine Precompiled Header
// ==================================================

// Windows
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>  // ShellExecuteEx用

// DirectX 12
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

// C++ Standard Library
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <map>
#include <functional>
#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <cmath>
#include <cstdint>
#include <array>
#include <optional>

// Engine Core Types (frequently used)
#include "Engine/Core/Types.h"
#include "Engine/Core/NonCopyable.h"
