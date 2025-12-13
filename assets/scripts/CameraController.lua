-- CameraController.lua
-- FPS/TPSカメラコントローラー
-- Play開始時に自動で視点操作、TABで解除、クリックで再開

-- public変数（Inspectorに表示される）
moveSpeed = 5.0
mouseSensitivity = 1.4

local initialized = false

function Awake()
    Debug.log("CameraController initialized")
end

function Start()
    Debug.log("Camera script attached to: " .. gameObject.name)
    Debug.log("Press TAB to release cursor, click to re-enable mouse look")
    initialized = true
end

function Update(deltaTime)
    -- 初期化後の最初のフレームでカーソルロック
    if initialized and not Cursor.isLocked() then
        Cursor.lock()
        Debug.log("Cursor locked - mouse look enabled")
        initialized = false
    end

    -- TABキーでカーソルをアンロック
    if Input.isKeyPressed("Tab") then
        if Cursor.isLocked() then
            Cursor.unlock()
            Debug.log("Cursor unlocked (TAB)")
        end
    end

    -- 左クリックでカーソルを再ロック
    if Input.isMouseButtonPressed(0) then
        if not Cursor.isLocked() then
            Cursor.lock()
            Debug.log("Cursor locked - mouse look enabled")
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
