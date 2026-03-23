-- PlayerController.lua
-- カメラの視点方向に基づいてWASD移動 + SHIFTダッシュ + Spaceジャンプ

-- 設定をJSONから読み込み
local config = Config and Config.loadJson("assets/config/player_controller.json") or {}

-- public変数（JSONのデフォルト値を使用、Inspectorで上書き可能）
moveSpeed = config.moveSpeed or 10.0
dashSpeedBonus = config.dashSpeedBonus or 4.0
jumpForce = config.jumpForce or 12.0

-- 定数（JSONから読み込み）
local INPUT_THRESHOLD         = config.inputThreshold or 0.1
local NORMALIZATION_EPSILON   = config.normalizationEpsilon or 0.001

-- ローカル変数
local isMoving = false
local jumpWasDown = false

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
    local horizontal = Input.getAxis("Horizontal")
    local vertical = Input.getAxis("Vertical")
    local spaceDown = Input.isKeyDown("Space")
    local shiftDown = Input.isKeyDown("Shift")

    if math.abs(horizontal) > INPUT_THRESHOLD or math.abs(vertical) > INPUT_THRESHOLD then
        isMoving = true
    end

    if shiftDown then
        isDashing = true
    end

    -- Spaceでジャンプ（Rigidbody接地時、held中に着地したら即発動）
    if Rigidbody then
        local grounded = Rigidbody.isGrounded()

        if spaceDown and grounded and not jumpWasDown then
            local vx, vy, vz = Rigidbody.getVelocity()
            Rigidbody.setVelocity(vx, jumpForce, vz)
            jumpWasDown = true
        end

        if not spaceDown then
            jumpWasDown = false
        end
    end

    if isMoving then
        local forwardX, forwardY, forwardZ = Camera.getForward()
        local rightX, rightY, rightZ = Camera.getRight()

        local moveX = forwardX * vertical + rightX * horizontal
        local moveZ = forwardZ * vertical + rightZ * horizontal

        local length = math.sqrt(moveX * moveX + moveZ * moveZ)
        if length > NORMALIZATION_EPSILON then
            moveX = moveX / length
            moveZ = moveZ / length
        end

        local currentSpeed = moveSpeed
        if isDashing then
            currentSpeed = moveSpeed + dashSpeedBonus
        end

        local speed = currentSpeed * deltaTime
        transform.translate(moveX * speed, 0, moveZ * speed)

        if Animator and not wasMoving then
            Animator.play("Walk", true)
        end
    else
        if Animator and wasMoving then
            Animator.play("Idle", true)
        end
    end
end

function OnDestroy()
    Debug.log("PlayerController destroyed")
end
