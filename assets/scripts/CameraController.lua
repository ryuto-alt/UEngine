-- CameraController.lua
-- FPS/TPSカメラコントローラー
-- Unity/Unreal風の操作: クリックで視点操作開始、Escで解除

-- public変数（Inspectorに表示される）
moveSpeed = 5.0
mouseSensitivity = 1.4

function Awake()
    Debug.log("CameraController initialized")
end

function Start()
    Debug.log("Camera script attached to: " .. gameObject.name)
    Debug.log("Click to enable mouse look, press Escape to release cursor")
end

function Update(deltaTime)
    -- 左クリックでカーソルをロック（視点操作開始）
    if Input.isMouseButtonPressed(0) then
        if not Cursor.isLocked() then
            Cursor.lock()
            Debug.log("Cursor locked - mouse look enabled")
        end
    end

    -- Escapeキーでカーソルをアンロック
    if Input.isKeyPressed("Escape") then
        if Cursor.isLocked() then
            Cursor.unlock()
            Debug.log("Cursor unlocked")
        end
    end

    -- カーソルがロックされていれば視点移動
    if Cursor.isLocked() then
        Cursor.lookAround(mouseSensitivity)
    end

    -- WASD移動（カメラの向きに基づく）
    local horizontal = Input.getAxis("Horizontal")
    local vertical = Input.getAxis("Vertical")

    if horizontal ~= 0 or vertical ~= 0 then
        -- カメラの向きから移動方向を計算
        local fx, fy, fz = Camera.getForward()
        local rx, ry, rz = Camera.getRight()

        local moveX = (fx * vertical + rx * horizontal) * moveSpeed * deltaTime
        local moveZ = (fz * vertical + rz * horizontal) * moveSpeed * deltaTime

        transform.translate(moveX, 0, moveZ)
    end
end

function OnDestroy()
    -- 終了時にカーソルを解放
    if Cursor.isLocked() then
        Cursor.unlock()
    end
    Debug.log("CameraController destroyed")
end
