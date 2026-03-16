#pragma once
#include <cstdint>
#include <string_view>

#include "core/TypeId.h"

namespace Unnamed {
	class World;
	class Scene;
	class Entity;
	class JsonWriter;
	class JsonReader;

	/// @brief Component は Entity に取り付けられるオブジェクトです。
	/// 取り付けられた Entity にどの用に振る舞わせるかを定義します。
	class BaseComponent {
	public:
		explicit BaseComponent();
		virtual  ~BaseComponent();

		/// @brief コンポーネントがエンティティに取り付けられたときに呼び出されます。
		virtual void OnAttached();
		/// @brief コンポーネントがエンティティから取り外されたときに呼び出されます。
		virtual void OnDetached();

		/// @brief 物理演算の前に呼び出されます。
		/// @param deltaTime 前のフレームからの経過時間 [秒]
		virtual void PrePhysicsTick(float deltaTime);
		/// @brief 毎フレーム呼び出されます。
		/// @param deltaTime 前のフレームからの経過時間 [秒]
		virtual void OnTick(float deltaTime);
		/// @brief 物理演算の後に呼び出されます。
		/// @param deltaTime 前のフレームからの経過時間 [秒]
		virtual void PostPhysicsTick(float deltaTime);

		/// @brief レンダリングの前に呼び出されます。
		virtual void OnPreRender() const;
		/// @brief レンダリング時に呼び出されます。
		virtual void OnRender() const;
		/// @brief レンダリングの後に呼び出されます。
		virtual void OnPostRender() const;

		/// @brief エディターのティック時に呼び出されます。
		/// @param deltaTime 前のフレームからの経過時間 [秒]
		virtual void OnEditorTick(float deltaTime);
		/// @brief エディターのレンダリング時に呼び出されます。
		virtual void OnEditorRender() const;

		/// @brief コンポーネントの安定した名前を取得します。オーバーライド必須
		[[nodiscard]] virtual std::string_view GetStableName() const = 0;

		/// @brief エディタでの表示やコンソールのログに使用されます。オーバーライド必須
		[[nodiscard]] virtual std::string_view GetComponentName() const = 0;

#ifdef _DEBUG
		/// @brief エディタのインスペクターでこのコンポーネントのプロパティを描画します。
		/// @details ImGuiはReleaseに含まれないので_DEBUGで囲んでください。必要に応じてオーバーライドしてください。
		virtual void DrawInspectorImGui();
#endif

		/// @brief コンポーネントの値を読み込む際に使用されます。オーバーライド必須
		virtual void Deserialize(const JsonReader& reader) = 0;

		/// @brief コンポーネントの値を書き込む際に使用されます。オーバーライド必須
		virtual void Serialize(JsonWriter& writer) const = 0;

		/// @brief このコンポーネントを所有しているエンティティを取得します。
		/// @return 所有しているエンティティのポインタ
		[[nodiscard]] Entity* GetOwner() const;

		/// @brief このコンポーネントを所有しているエンティティを設定します。
		/// @param owner 所有するエンティティのポインタ
		void SetOwner(Entity* owner);

		/// @brief このコンポーネントが所属するシーンを取得します。
		/// @return 所属するシーンのポインタ
		[[nodiscard]] Scene* GetScene() const noexcept;

		/// @brief このコンポーネントが所属するワールドを取得します。
		/// @return 所属するワールドのポインタ
		[[nodiscard]] World* GetWorld() const noexcept;

		/// @brief コンポーネントがアクティブかどうかを取得します。
		/// @return アクティブな場合は true、非アクティブな場合は false
		[[nodiscard]] bool IsActive() const noexcept;

		/// @brief コンポーネントをアクティブまたは非アクティブに設定します。
		/// @param isActive アクティブにする場合は true、非アクティブにする場合は false
		void SetActive(bool isActive) noexcept;

		/// @brief コンポーネントの一意の識別子 (GUID) を取得します。
		/// @return コンポーネントの GUID
		[[nodiscard]] uint64_t GetGuid() const noexcept;

		/// @brief コンポーネントの一意の識別子 (GUID) を設定します。
		/// @param guid コンポーネントの新しい GUID
		void SetGuid(uint64_t guid) noexcept;

		/// @brief コンポーネントの型識別子を取得します。
		/// @return コンポーネントの型識別子
		[[nodiscard]] TypeId GetTypeId() const;

	protected:
		Entity*  mOwner = nullptr; // 所有しているエンティティ
		uint64_t mGuid  = 0;       // GUID

		bool mIsActive = true; // アクティブか?
	};
}
