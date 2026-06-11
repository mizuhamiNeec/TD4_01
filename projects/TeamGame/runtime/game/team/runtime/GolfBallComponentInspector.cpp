#include "GolfBallComponent.h"

#include "GolfBallEndPosComponent.h"
#include "GolfBallStartPosComponent.h"

#include <algorithm>
#include <vector>

#include <engine/ImGui/ImGuiWidgets.h>
#include <engine/scene/Scene.h>
#include <engine/unnamed/framework/entity/Entity.h>
#include <engine/world/World.h>

#ifdef _DEBUG
#include <imgui.h>

namespace MyGame {
	void GolfBallComponent::DrawInspectorImGui() {
		ImGui::Text("=== ゴルフボール設定 ===");
		if (ImGui::DragFloat("半径##golf_radius", &_radius, 0.25f, 0.1f, 100.0f, "%.2f")) {
			ApplyPositionToRuntime();
		}
		ImGui::SliderFloat("重さ##golf_mass", &_mass, 0.1f, 30.0f, "%.2f kg");
		ImGui::SliderFloat("外力の上向き速度上限##golf_external_up_limit", &_maxExternalUpwardVelocity, 0.0f, 30.0f, "%.2f");

		if (ImGuiWidgets::DragVec3("速度を直接上書き##golf_velocity_override", _velocity, Vec3::zero, 0.1f, "%.2f[m/s]")) {
			_bIsInFlight = true;
		}

		ImGui::Separator();
		ImGui::Text("状態: %s", _bIsInFlight ? "飛行中" : "停止中");
		ImGui::Text("経過時間: %.2f 秒", _elapsedTime);
		ImGui::ProgressBar(
			_elapsedTime / std::max(0.001f, _flightTime),
			ImVec2(0, 0),
			"進行度"
		);
		ImGui::Text("現在位置: (%.2f, %.2f, %.2f)", _position.x, _position.y, _position.z);
		ImGui::Text("現在速度: (%.2f, %.2f, %.2f)", _velocity.x, _velocity.y, _velocity.z);
		ImGui::Text("速度の大きさ: %.2f units/sec", _velocity.Length());

		ImGui::Separator();
		ImGui::Text("=== 発射位置設定 ===");
		if (ImGuiWidgets::DragVec3("発射位置##golf_start_point", _startPoint, Vec3::zero, 0.1f, "%.2f")) {
			_startPosEntity = nullptr;
			_startPosEntityGuid.clear();
			if (!_bIsInFlight && !_bIsBeingSucked) {
				SetStartPoint(_startPoint);
			}
		}
		if (ImGui::Button("発射位置へボールを移動##golf_apply_start", ImVec2(190, 0))) {
			SetStartPoint(_startPoint);
		}
		ImGui::SameLine();
		if (ImGui::Button("現在位置を発射位置にする##golf_start_from_current", ImVec2(210, 0))) {
			_startPosEntity = nullptr;
			_startPosEntityGuid.clear();
			_startPoint = _position;
		}
		ImGui::Text("保存される発射位置: (%.2f, %.2f, %.2f)", _startPoint.x, _startPoint.y, _startPoint.z);

		std::vector<Unnamed::Entity*> startPosEntities;
		std::vector<Unnamed::Entity*> endPosEntities;
		if (auto* world = GetWorld()) {
			if (auto* scene = world->GetScenePtr()) {
				for (const auto& entity : scene->GetEntities()) {
					if (!entity) {
						continue;
					}
					if (entity->GetComponent<GolfBallStartPosComponent>()) {
						startPosEntities.push_back(entity.get());
					}
					if (entity->GetComponent<GolfBallEndPosComponent>()) {
						endPosEntities.push_back(entity.get());
					}
				}
			}
		}

		int startIndex = -1;
		std::vector<const char*> startItems;
		for (const auto* entity : startPosEntities) {
			if (entity == _startPosEntity) {
				startIndex = static_cast<int>(startItems.size());
			}
			startItems.push_back(entity->GetName().data());
		}
		if (!startItems.empty() && ImGui::Combo("発射位置マーカー##combo_start", &startIndex, startItems.data(), static_cast<int>(startItems.size()))) {
			if (startIndex >= 0 && startIndex < static_cast<int>(startPosEntities.size())) {
				SetStartPosEntity(startPosEntities[startIndex]);
			}
		}

		ImGui::Separator();
		ImGui::Text("=== 着弾位置設定 ===");
		if (ImGuiWidgets::DragVec3("着弾基準位置##golf_target_point", _targetBase, Vec3::zero, 0.1f, "%.2f")) {
			_targetPosEntity = nullptr;
			_targetPosEntityGuid.clear();
		}
		if (ImGui::Button("ランダム着地点を再計算##golf_refresh_target_offset", ImVec2(220, 0))) {
			RefreshTargetRandomOffset();
		}
		const Vec3 finalTarget = _targetBase + _targetRandomOffset;
		ImGui::Text("着弾基準位置: (%.2f, %.2f, %.2f)", _targetBase.x, _targetBase.y, _targetBase.z);
		ImGui::Text("ランダム補正: (%.2f, %.2f, %.2f)", _targetRandomOffset.x, _targetRandomOffset.y, _targetRandomOffset.z);
		ImGui::Text("最終着地点: (%.2f, %.2f, %.2f)", finalTarget.x, finalTarget.y, finalTarget.z);

		int endIndex = -1;
		std::vector<const char*> endItems;
		for (const auto* entity : endPosEntities) {
			if (entity == _targetPosEntity) {
				endIndex = static_cast<int>(endItems.size());
			}
			endItems.push_back(entity->GetName().data());
		}
		if (!endItems.empty() && ImGui::Combo("着弾位置マーカー##combo_end", &endIndex, endItems.data(), static_cast<int>(endItems.size()))) {
			if (endIndex >= 0 && endIndex < static_cast<int>(endPosEntities.size())) {
				SetTargetPosEntity(endPosEntities[endIndex]);
			}
		}

		ImGui::Separator();
		ImGui::Text("=== 物理パラメータ ===");
		ImGui::SliderFloat("重力##golf_gravity", &_gravity, 0.0f, 20.0f, "%.2f");
		ImGui::SliderFloat("到達時間##golf_flight_time", &_flightTime, 0.1f, 30.0f, "%.2f");
		ImGui::SliderFloat("着地点ランダム半径##golf_random_radius", &_randomRadius, 0.0f, 10.0f, "%.2f");
		ImGui::SliderFloat("最大速度##golf_max_speed", &_maxSpeedClamp, 0.0f, 100.0f, "%.2f");
		ImGui::SliderFloat("風 X##golf_wind_x", &_wind.x, -10.0f, 10.0f, "%.2f");
		ImGui::SliderFloat("風 Y##golf_wind_y", &_wind.y, -10.0f, 10.0f, "%.2f");
		ImGui::SliderFloat("風 Z##golf_wind_z", &_wind.z, -10.0f, 10.0f, "%.2f");
		ImGui::SliderFloat("ホーミング強度##golf_homing_strength", &_homingStrength, 0.0f, 5.0f, "%.2f");
		ImGui::SliderFloat("ホーミング開始時間##golf_homing_start", &_homingStartTime, 0.0f, _flightTime, "%.2f");
		ImGui::SliderFloat("ホーミング終了時間##golf_homing_end", &_homingEndTime, _homingStartTime, _flightTime, "%.2f");
		ImGui::SliderFloat("反発係数##golf_bounce", &_bounceDamping, 0.0f, 1.0f, "%.3f");
		ImGui::SliderFloat("最小バウンド速度##golf_min_bounce_speed", &_minBounceVerticalSpeed, 0.0f, 5.0f, "%.3f");
		ImGui::SliderFloat("衝突時横減衰##golf_ground_collision_damping", &_groundCollisionDamping, 0.0f, 1.0f, "%.3f");
		ImGui::SliderFloat("摩擦係数##golf_friction", &_frictionCoefficient, 0.0f, 1.0f, "%.3f");
		ImGui::SliderFloat("地面の高さ##golf_ground_level", &_groundLevel, -10.0f, 10.0f, "%.2f");
		ImGui::Checkbox("円形地面エリアを使う##golf_use_circular_ground_area", &_useCircularGroundArea);
		float groundAreaCenter[2] = { _groundAreaCenter.x, _groundAreaCenter.z };
		if (ImGui::DragFloat2("地面エリア中心 XZ##golf_ground_area_center", groundAreaCenter, 0.1f, -999.0f, 999.0f, "%.2f")) {
			// NOTE: 地面高さは _groundLevel が担当するため、エリア調整では X/Z だけを更新する。
			_groundAreaCenter.x = groundAreaCenter[0];
			_groundAreaCenter.z = groundAreaCenter[1];
		}
		if (ImGui::DragFloat("地面エリア半径##golf_ground_area_radius", &_groundAreaRadius, 0.1f, 0.0f, 999.0f, "%.2f")) {
			// NOTE: 負の半径は円外判定を破綻させるため、保存前に下限で丸める。
			_groundAreaRadius = std::max(0.0f, _groundAreaRadius);
		}
		ImGui::SliderFloat("停止判定速度##golf_stop_threshold", &_stopVelocityThreshold, 0.001f, 0.1f, "%.4f");
		ImGui::Checkbox("接地中##golf_grounded", &_bIsGrounded);

		ImGui::Separator();
		if (ImGui::Button("発射##golf_launch", ImVec2(100, 30))) {
			Launch();
		}
		ImGui::SameLine();
		if (ImGui::Button("リセット##golf_reset", ImVec2(100, 30))) {
			_bIsInFlight = false;
			_elapsedTime = 0.0f;
			_velocity = Vec3(0.0f, 0.0f, 0.0f);
			SetStartPoint(_startPoint);
		}
		ImGui::SameLine();
		if (ImGui::Button("停止##golf_stop", ImVec2(100, 30))) {
			_bIsInFlight = false;
		}
	}
}
#endif
