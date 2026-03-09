-- PlayerController.lua
-- カメラの視点方向に基づいてWASD移動 + SHIFTダッシュ + Spaceジャンプ

-- public変数（Inspectorに表示される）
moveSpeed = 10.0
dashSpeedBonus = 4.0
jumpForce = 12.0

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

    if math.abs(horizontal) > 0.1 or math.abs(vertical) > 0.1 then
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
        if length > 0.001 then
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
