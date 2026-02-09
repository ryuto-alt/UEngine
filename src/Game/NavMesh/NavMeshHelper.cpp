#include "NavMeshHelper.h"
#include "NavMesh.h"
#include "NavMeshBuilder.h"
#include "DetourNavMeshQuery.h"
#include <cmath>
#include <algorithm>

Vector3 NavMeshHelper::ClampToNavMesh(
	const Vector3& position,
	NavMesh* navMesh,
	const Vector3& extents
) {
	if (!navMesh || !navMesh->IsValid()) {
		return position;
	}

	// Use NavMesh's cached query instead of allocating a new one every frame
	return navMesh->FindNearestPoint(position, extents);
}

void NavMeshHelper::FollowPath(
	Vector3& position,
	float& currentRotationY,
	float& currentSpeed,
	std::vector<Vector3>& currentPath,
	int& currentWaypointIndex,
	float moveSpeed,
	float deltaTime,
	NavMesh* navMesh,
	bool* isAtCorner,
	float* cornerSlowdownFactor
) {
	if (currentPath.empty() || currentWaypointIndex >= static_cast<int>(currentPath.size())) {
		return;
	}

	// Advance past any waypoints already within reach (don't skip a frame per waypoint)
	const float WAYPOINT_REACH_THRESHOLD = 0.5f;
	while (currentWaypointIndex < static_cast<int>(currentPath.size())) {
		const Vector3& wp = currentPath[currentWaypointIndex];
		float dx = wp.x - position.x;
		float dz = wp.z - position.z;
		if (std::sqrt(dx * dx + dz * dz) >= WAYPOINT_REACH_THRESHOLD) break;
		currentWaypointIndex++;
	}

	if (currentWaypointIndex >= static_cast<int>(currentPath.size())) {
		currentPath.clear();
		currentWaypointIndex = 0;
		return;
	}

	const Vector3& targetWaypoint = currentPath[currentWaypointIndex];

	// 目標ウェイポイントへのベクトル
	Vector3 toWaypoint = {
		targetWaypoint.x - position.x,
		0.0f,
		targetWaypoint.z - position.z
	};

	float distanceToWaypoint = std::sqrt(toWaypoint.x * toWaypoint.x + toWaypoint.z * toWaypoint.z);

	// 先読み：次のウェイポイントがある場合、そちらにも少し引き寄せられる
	Vector3 targetDirection = toWaypoint;
	bool atCorner = false;
	float slowdownFactor = 1.0f;

	if (currentWaypointIndex + 1 < static_cast<int>(currentPath.size())) {
		const Vector3& nextWaypoint = currentPath[currentWaypointIndex + 1];
		Vector3 toNextWaypoint = {
			nextWaypoint.x - position.x,
			0.0f,
			nextWaypoint.z - position.z
		};

		float distToNext = std::sqrt(toNextWaypoint.x * toNextWaypoint.x + toNextWaypoint.z * toNextWaypoint.z);
		if (distToNext > 0.001f) {
			toNextWaypoint.x /= distToNext;
			toNextWaypoint.z /= distToNext;

			// 正規化された方向ベクトル
			float normalizedToWaypointX = toWaypoint.x / distanceToWaypoint;
			float normalizedToWaypointZ = toWaypoint.z / distanceToWaypoint;

			// 角度を計算（内積）
			float dotProduct = normalizedToWaypointX * toNextWaypoint.x + normalizedToWaypointZ * toNextWaypoint.z;
			float angle = std::acos(std::clamp(dotProduct, -1.0f, 1.0f));

			// 角度が大きい（急カーブ）場合は軽く減速（最低0.55倍速）
			const float SHARP_TURN_THRESHOLD = 1.0f;
			if (angle > SHARP_TURN_THRESHOLD) {
				atCorner = true;
				slowdownFactor = 0.55f + (1.0f - angle / 3.14159f) * 0.45f;
			}

			// 先読みブレンド：2m以内で効き始め、角度が急でも一定量ブレンド
			const float LOOKAHEAD_DISTANCE = 2.0f;
			float baseBlendFactor = 1.0f - std::clamp(distanceToWaypoint / LOOKAHEAD_DISTANCE, 0.0f, 1.0f);

			// 角度が急なほどブレンドを弱くする（ただし完全無効化はしない）
			float angleInfluence = std::clamp(1.0f - (angle / 2.5f), 0.15f, 1.0f);
			float blendFactor = baseBlendFactor * angleInfluence * 0.5f;

			targetDirection.x = normalizedToWaypointX * (1.0f - blendFactor) + toNextWaypoint.x * blendFactor;
			targetDirection.z = normalizedToWaypointZ * (1.0f - blendFactor) + toNextWaypoint.z * blendFactor;
		}
	}

	// 正規化
	float targetLength = std::sqrt(targetDirection.x * targetDirection.x + targetDirection.z * targetDirection.z);
	if (targetLength > 0.001f) {
		targetDirection.x /= targetLength;
		targetDirection.z /= targetLength;
	}

	// 目標回転角を計算
	float targetRotationY = std::atan2(targetDirection.x, targetDirection.z);

	// 回転の補間（デルタタイムを考慮）
	const float ROTATION_SPEED = 12.0f;  // 1秒あたりの回転速度（ラジアン/秒）
	float angleDiff = targetRotationY - currentRotationY;
	while (angleDiff > 3.14159f) angleDiff -= 2.0f * 3.14159f;
	while (angleDiff < -3.14159f) angleDiff += 2.0f * 3.14159f;

	// 回転速度を制限（最大速度で回転）
	float rotationStep = ROTATION_SPEED * deltaTime;
	if (std::abs(angleDiff) > rotationStep) {
		// 角度差が大きい場合は一定速度で回転
		currentRotationY += (angleDiff > 0 ? rotationStep : -rotationStep);
	} else {
		// 角度差が小さい場合は即座に目標角度に
		currentRotationY = targetRotationY;
	}

	// 速度の補間（デルタタイムを考慮）
	const float SPEED_LERP_RATE = 10.0f;  // 1秒あたりの補間速度
	float targetSpeed = moveSpeed * slowdownFactor;
	float speedLerpFactor = 1.0f - std::exp(-SPEED_LERP_RATE * deltaTime);
	currentSpeed += (targetSpeed - currentSpeed) * speedLerpFactor;

	// 移動（デルタタイムを適用）
	Vector3 newPosition = position;
	newPosition.x += targetDirection.x * currentSpeed * deltaTime;
	newPosition.z += targetDirection.z * currentSpeed * deltaTime;

	// NavMesh上の有効な位置に補正
	newPosition = ClampToNavMesh(newPosition, navMesh);

	position = newPosition;

	// 出力パラメータを設定
	if (isAtCorner) *isAtCorner = atCorner;
	if (cornerSlowdownFactor) *cornerSlowdownFactor = slowdownFactor;
}
