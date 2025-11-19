#pragma once

#include "../../Engine/Core/Component.h"

namespace UnoEngine {

// Pure data component
struct Player : public Component {
    float moveSpeed{5.0f};
};

} // namespace UnoEngine
