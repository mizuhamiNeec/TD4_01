#pragma once
#include <cstdint>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Unnamed {
	class UEntity;
	class UBaseComponent;
	class JsonReader;
	class JsonWriter;

	using EntityId = uint64_t;

	struct SceneEntityHandle {
		EntityId id = 0;
	};

	class UScene {
	public:
		UScene();
		~UScene();

		// コピー・コピー代入禁止
		UScene(const UScene&)            = delete;
		UScene& operator=(const UScene&) = delete;

		/// @brief 新しいエンティティをシーンに作成します。
		/// @param name エンティティの名前
		/// @param id
		/// @param isEditorOnly
		/// @return 作成されたエンティティの参照
		[[nodiscard]] UEntity& CreateEntity(
			std::string_view name, uint64_t id, bool isEditorOnly
		);

		/// @brief 指定したIDのエンティティをシーンから破棄します。
		/// @param id エンティティID
		void DestroyEntity(EntityId id);

		/// @brief 指定したIDのエンティティをシーン内で検索します。
		/// @param id エンティティID
		/// @return エンティティのポインタ。存在しない場合は nullptr。
		[[nodiscard]] UEntity* FindEntity(EntityId id);

		/// @brief 指定したIDのエンティティをシーン内で検索します。(const版)
		/// @param id エンティティID
		/// @return エンティティのポインタ。存在しない場合は nullptr。
		[[nodiscard]] const UEntity* FindEntity(EntityId id) const;

		/// @brief シーン内のエンティティ数を取得します。
		[[nodiscard]] size_t GetEntityCount() const;

		/// @brief シーン内のすべてのエンティティを取得します。
		[[nodiscard]]
		const std::vector<std::unique_ptr<UEntity>>& GetEntities() const;

		/// @brief シーンをシリアライズします。
		/// @param writer JSONライター
		void Serialize(const JsonWriter& writer) const;

		/// @brief シーンをデシリアライズします。
		/// @param reader JSONリーダー
		void Deserialize(const JsonReader& reader);

		/// @brief シーンがロードされた後に呼び出されます。
		void OnPostLoad();

		/// @brief シーンの全エンティティを削除して初期状態に戻します。
		void Reset();

		/// @brief エンティティIDを割り当てます。
		/// @return 新しいエンティティID
		[[nodiscard]] EntityId AllocateEntityId();

	private:
		EntityId                               mNextEntityId = 1;
		std::vector<std::unique_ptr<UEntity>>  mEntities;
		std::unordered_map<EntityId, UEntity*> mEntityById;
	};
}
