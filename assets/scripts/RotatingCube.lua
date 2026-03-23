-- RotatingCube.lua
-- オブジェクトを回転させるサンプルスクリプト

-- 設定をJSONから読み込み
local config = Config and Config.loadJson("assets/config/rotating_cube.json") or {}

-- public変数（JSONのデフォルト値を使用、Inspectorで上書き可能）
rotationSpeed = config.rotationSpeed or 45.0
axis = config.axis or "y"

-- 軸ごとの回転適用テーブル（データ駆動）
local axisApply = {
    x = function(rx, ry, rz, rot) transform.setRotation(rx + rot, ry, rz) end,
    y = function(rx, ry, rz, rot) transform.setRotation(rx, ry + rot, rz) end,
    z = function(rx, ry, rz, rot) transform.setRotation(rx, ry, rz + rot) end,
}

-- ローカル変数
local totalRotation = 0

function Awake()
    print("RotatingCube script initialized")
end

function Start()
    Debug.log("RotatingCube attached to: " .. gameObject.name)
end

function Update(deltaTime)
    local rotation = rotationSpeed * deltaTime
    totalRotation = totalRotation + rotation

    local rx, ry, rz = transform.getRotation()

    local applyFn = axisApply[axis]
    if applyFn then
        applyFn(rx, ry, rz, rotation)
    end
end

function OnDestroy()
    Debug.log(string.format("RotatingCube destroyed. Total rotation: %.2f degrees", totalRotation))
end
