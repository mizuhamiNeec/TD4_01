#include "UCameraComponent.h"

#include <core/unnamed/json/JsonWriter.h>
#include <core/unnamed/json/JsonReader.h>

#include <engine/unnamed/gameframework/component/Transform/TransformComponent.h>

namespace Unnamed {
	/// @brief ビュー行列を取得する
	/// @param transformComponent トランスフォームコンポーネント
	/// @return ビュー行列
	Mat4 UCameraComponent::View(
		const TransformComponent* transformComponent
	) {
		const Mat4 w = transformComponent ?
			               transformComponent->WorldMat() :
			               Mat4::identity;
		return w.Inverse();
	}

	/// @brief 射影行列を取得する
	/// @param aspectRatio アスペクト比
	/// @return 射影行列
	Mat4 UCameraComponent::Proj(const float aspectRatio) const {
		return Mat4::PerspectiveFovMat(
			mFovY, aspectRatio, mZNear, mZFar
		);
	}

	/// @brief シリアライズ処理
	/// @param writer JSONライター
	void UCameraComponent::Serialize(JsonWriter& writer) const {
		writer.Key("fov");
		writer.Write(mFovY);
		writer.Key("zNear");
		writer.Write(mZNear);
		writer.Key("zFar");
		writer.Write(mZFar);
	}

	/// @brief デシリアライズ処理
	/// @param reader JSONリーダー
	void UCameraComponent::Deserialize(const JsonReader& reader) {
		if (reader.Has("fov")) { mFovY = reader["fov"].GetFloat(); }
		if (reader.Has("zNear")) { mZNear = reader["zNear"].GetFloat(); }
		if (reader.Has("zFar")) { mZFar = reader["zFar"].GetFloat(); }
	}
}
