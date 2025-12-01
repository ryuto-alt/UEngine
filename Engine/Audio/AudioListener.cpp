#include "AudioListener.h"
#include "../Core/GameObject.h"
#include "../Core/Transform.h"

namespace UnoEngine {

Vector3 AudioListener::GetListenerPosition() const {
    if (useEditorOverride_) {
        return editorOverridePosition_;
    }
    
    if (!gameObject_) {
        return Vector3::Zero();
    }
    
    return gameObject_->GetTransform().GetPosition();
}

Vector3 AudioListener::GetListenerForward() const {
    if (useEditorOverride_) {
        return editorOverrideForward_;
    }
    
    if (!gameObject_) {
        return Vector3(0.0f, 0.0f, 1.0f);
    }
    
    // Transformの前方向を取得
    return gameObject_->GetTransform().GetForward();
}

Vector3 AudioListener::GetListenerUp() const {
    if (useEditorOverride_) {
        return editorOverrideUp_;
    }
    
    if (!gameObject_) {
        return Vector3(0.0f, 1.0f, 0.0f);
    }
    
    // Transformの上方向を取得
    return gameObject_->GetTransform().GetUp();
}

} // namespace UnoEngine
