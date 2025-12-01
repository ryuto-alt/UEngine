-- CameraController.lua
-- FPS/TPSカメラコントローラーサンプル

-- public変数（Inspectorに表示される）
moveSpeed = 5.0
mouseSensitivity = 1.4

-- ローカル変数
local yaw = 0
local pitch = 0
local isMouseLocked = false

function Awake()
    print("CameraController initialized")
    Debug.log("Camera script attached to: " .. gameObject.name)
end

function Start()
    -- 初期位置を設定
    local x, y, z = transform.getPosition()
    Debug.log(string.format("Initial position: (%.2f, %.2f, %.2f)", x, y, z))
end

function Update(deltaTime)
    -- 移動処理
    local moveX = 0
    local moveZ = 0
    
    -- WASDキー入力はInputシステム経由で取得（Phase3で実装）
    -- 今はテスト用に自動移動
    
    -- Transformを使った移動のデモ
    -- transform.translate(moveX * moveSpeed * deltaTime, 0, moveZ * moveSpeed * deltaTime)
end

function OnDestroy()
    print("CameraController destroyed")
end
