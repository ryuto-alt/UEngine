-- PlayerController.lua
-- カメラの視点方向に基づいてWASD移動 + SHIFTダッシュ

-- public変数（Inspectorに表示される）
moveSpeed = 5.0
dashSpeedBonus = 4.0

-- ローカル変数
local isMoving = false

function Awake()
    Debug.log("PlayerController initialized on: " .. gameObject.name)
end

function Start()
    local x, y, z = transform.getPosition()
    Debug.log(string.format("Player position: (%.2f, %.2f, %.2f)", x, y, z))
end

function Update(deltaTime)
    if not Input or not Camera then
        return
    end

    local wasMoving = isMoving
    isMoving = false
    local isDashing = false

    -- 入力取得
    local horizontal = Input.getAxis("Horizontal")  -- A/D
    local vertical = Input.getAxis("Vertical")      -- W/S

    if math.abs(horizontal) > 0.1 or math.abs(vertical) > 0.1 then
        isMoving = true
    end

    -- SHIFTキーでダッシュ
    if Input.isKeyDown("Shift") then
        isDashing = true
    end

    if isMoving then
        -- カメラの向きを取得
        local forwardX, forwardY, forwardZ = Camera.getForward()
        local rightX, rightY, rightZ = Camera.getRight()

        -- 移動方向を計算（カメラ基準）
        local moveX = forwardX * vertical + rightX * horizontal
        local moveZ = forwardZ * vertical + rightZ * horizontal

        -- 正規化
        local length = math.sqrt(moveX * moveX + moveZ * moveZ)
        if length > 0.001 then
            moveX = moveX / length
            moveZ = moveZ / length
        end

        -- 速度計算（ダッシュ時は+4）
        local currentSpeed = moveSpeed
        if isDashing then
            currentSpeed = moveSpeed + dashSpeedBonus
        end

        local speed = currentSpeed * deltaTime
        transform.translate(moveX * speed, 0, moveZ * speed)

        -- 移動アニメーション
        if Animator and not wasMoving then
            Animator.play("Walk", true)
        end
    else
        -- アイドルアニメーション
        if Animator and wasMoving then
            Animator.play("Idle", true)
        end
    end
end

function OnDestroy()
    Debug.log("PlayerController destroyed")
end
