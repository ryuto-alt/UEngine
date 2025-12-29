-- CameraController.lua
-- FPS/TPSカメラコントローラー
-- F1キーでマウスロック、TABまたはESCで解除

-- public変数（Inspectorに表示される）
moveSpeed = 5.0
mouseSensitivity = 1.4

local wasLocked = false

function Awake()
    Debug.log("CameraController initialized")
end

function Start()
    Debug.log("Camera script attached to: " .. gameObject.name)
    Debug.log("Press F1 to enable mouse look, TAB/ESC to release")
    -- 自動ロックしない
end

function Update(deltaTime)
    -- F1キーでカーソルをロック
    if Input.isKeyPressed("F") then
        if not Cursor.isLocked() then
            Cursor.lock()
            Debug.log("Cursor locked - mouse look enabled (Press TAB to unlock)")
        end
    end

    -- TABキーまたはESCキーでカーソルをアンロック
    if Input.isKeyPressed("Tab") or Input.isKeyPressed("Escape") then
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
