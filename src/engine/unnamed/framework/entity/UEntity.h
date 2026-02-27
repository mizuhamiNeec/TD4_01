#pragma once
#include <memory>
#include <ranges>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "core/TypeId.h"

#include "../components/base/UBaseComponent.h"


namespace Unnamed {
	/// @class UEntity
	/// @brief エンティティはゲームの基本オブジェクトです。
	/// ゲーム内に登場したり、ロジックを持つオブジェクトを表します。
	/// "ECSとは一切関係ありません"
	class UEntity {
	public:
		/// @brief コンストラクタ
		/// @param name エンティティの名前
		/// @param guid エンティティのGUID
		/// @param isEditorOnly エディター専用か?
		explicit UEntity(
			const std::string_view& name, uint64_t guid,
			bool                    isEditorOnly = false
		);

		~UEntity();

		/// @brief エンティティが登録されたときに呼び出されます。
		void OnRegister();
		/// @brief エンティティが登録された後に呼び出されます。
		void PostRegister();

		/// @brief 物理演算の前に呼び出されます。
		/// @param deltaTime 前のフレームからの経過時間（秒）
		void PrePhysicsTick(float deltaTime) const;
		/// @brief 毎フレーム呼び出されます。
		/// @param deltaTime 前のフレームからの経過時間（秒）
		void Tick(float deltaTime) const;
		/// @brief 物理演算の後に呼び出されます。
		/// @param deltaTime 前のフレームからの経過時間（秒）
		void PostPhysicsTick(float deltaTime) const;

		/// @brief レンダリングの前に呼び出されます。
		void OnPreRender() const;
		/// @brief レンダリング時に呼び出されます。
		void OnRender() const;
		/// @brief レンダリングの後に呼び出されます。
		void OnPostRender() const;

		/// @brief エディターのティック時に呼び出されます。
		/// @param deltaTime 前のフレームからの経過時間（秒）
		void OnEditorTick(float deltaTime) const;
		/// @brief エディターのレンダリング時に呼び出されます。
		void OnEditorRender() const;

		/// @brief エンティティが破棄されるときに呼び出されます。
		void OnDestroy();

		/// @brief コンポーネントを追加します。
		/// @tparam ComponentType 追加するコンポーネントの型
		/// @tparam Args コンポーネントのコンストラクタに渡す引数の型
		/// @param args コンポーネントのコンストラクタに渡す引数
		/// @return 追加されたコンポーネントのポインタ
		template <typename ComponentType, typename... Args>
		ComponentType* AddComponent(Args&&... args);

		/// @brief コンポーネントインスタンスを追加します。
		/// @param component 追加するコンポーネントのユニークポインタ
		/// @return 追加されたコンポーネントのポインタ
		UBaseComponent* AddComponentInstance(
			std::unique_ptr<UBaseComponent> component
		);

		/// @brief 指定した型のコンポーネントを取得します。
		/// @tparam ComponentType 取得するコンポーネントの型
		/// @return 取得したコンポーネントのポインタ。存在しない場合は nullptr。
		template <typename ComponentType>
		[[nodiscard]] ComponentType* GetComponent();

		/// @brief 指定した型のコンポーネントを取得します。(const版)
		/// @tparam ComponentType 取得するコンポーネントの型
		/// @return 取得したコンポーネントのポインタ。存在しない場合は nullptr。
		template <typename ComponentType>
		[[nodiscard]] const ComponentType* GetComponent() const;

		/// @brief 指定した型のすべてのコンポーネントを取得します。
		/// @tparam ComponentType 取得するコンポーネントの型
		/// @param out 取得したコンポーネントのポインタを格納するベクター
		template <typename ComponentType>
		void GetComponents(std::vector<ComponentType*>& out);

		/// @brief 指定した型のすべてのコンポーネントを取得します。(const版)
		/// @tparam ComponentType 取得するコンポーネントの型
		/// @param out 取得したコンポーネントのポインタを格納するベクター
		template <typename ComponentType>
		void GetComponents(std::vector<const ComponentType*>& out) const;

		/// @brief 指定した型のコンポーネントを取得するか、存在しない場合は新たに追加します。
		/// @tparam ComponentType 取得または追加するコンポーネントの型
		/// @tparam Args コンポーネントのコンストラクタに渡す引数の型
		/// @param args コンポーネントのコンストラクタに渡す引数
		/// @return 取得または追加されたコンポーネントのポインタ
		template <typename ComponentType, typename... Args>
		[[nodiscard]] ComponentType* GetOrAddComponent(Args&&... args);

		/// @brief コンポーネントを削除します。
		/// @param component 削除するコンポーネントのポインタ
		void RemoveComponent(UBaseComponent* component);

		/// @brief エンティティの名前を取得します。
		/// @return エンティティの名前
		[[nodiscard]] std::string_view GetName() const;

		/// @brief エンティティの名前を設定します。
		/// @param name エンティティの新しい名前
		void SetName(const std::string_view& name);

		/// @brief エンティティがエディター専用かどうかを取得します。
		/// @return エディター専用の場合は true、そうでない場合は false
		[[nodiscard]] bool IsEditorOnly() const noexcept;

		/// @brief エンティティがアクティブかどうかを取得します。
		/// @return アクティブな場合は true、非アクティブな場合は false
		[[nodiscard]] bool IsActive() const noexcept;

		/// @brief エンティティをアクティブまたは非アクティブに設定します。
		/// @param isActive アクティブにする場合は true、非アクティブにする場合は false
		void SetActive(bool isActive) noexcept;

		/// @brief エンティティのGUIDを取得します。
		/// @return エンティティのGUID
		[[nodiscard]] uint64_t GetGuid() const noexcept;

		/// @brief 全てのコンポーネントに対して関数を実行します。
		/// @tparam Func 呼び出し可能オブジェクト型（`void(UBaseComponent&)` 等）
		/// @param func 各コンポーネントに対して呼ばれる関数
		/// @return 全て走査した場合 true。途中で中断された場合 false。
		template <typename Func>
		bool ForEachComponent(Func&& func);

		/// @brief 全てのコンポーネントに対して関数を実行します。(const版)
		/// @tparam Func 呼び出し可能オブジェクト型（`void(const UBaseComponent&)` 等）
		/// @param func 各コンポーネントに対して呼ばれる関数
		/// @return 全て走査した場合 true。途中で中断された場合 false。
		template <typename Func>
		bool ForEachComponent(Func&& func) const;

		/// @brief 指定した型の全てのコンポーネントに対して関数を実行します。
		/// @tparam ComponentType 対象コンポーネント型
		/// @tparam Func 呼び出し可能オブジェクト型（`void(ComponentType&)` 等）
		/// @param func 各コンポーネントに対して呼ばれる関数
		/// @return 全て走査した場合 true。途中で中断された場合 false。
		template <typename ComponentType, typename Func>
		bool ForEachComponent(Func&& func);

		/// @brief 指定した型の全てのコンポーネントに対して関数を実行します。(const版)
		/// @tparam ComponentType 対象コンポーネント型
		/// @tparam Func 呼び出し可能オブジェクト型（`void(const ComponentType&)` 等）
		/// @param func 各コンポーネントに対して呼ばれる関数
		/// @return 全て走査した場合 true。途中で中断された場合 false。
		template <typename ComponentType, typename Func>
		bool ForEachComponent(Func&& func) const;

	protected:
		// 所有しているコンポーネント
		std::vector<std::unique_ptr<UBaseComponent>> mComponents;

		std::unordered_map<TypeId, std::vector<UBaseComponent*>>
		mComponentsByType;

		std::string mName         = "unnamed"; // 名前
		uint64_t    mGuid         = 0;         // GUID
		bool        mIsEditorOnly = false;     // エディター専用か?
		bool        mIsActive     = true;      // アクティブか?
		bool        mDestroyed    = false;     // OnDestroy二重呼び出し防止
	};

	template <typename ComponentType, typename... Args>
	ComponentType* UEntity::AddComponent(Args&&... args) {
		static_assert(
			std::is_base_of_v<UBaseComponent, ComponentType>,
			"T は UBaseComponent の派生クラスでなければなりません。"
		);
		static_assert(
			requires { ComponentType{}.GetStableName(); },
			"ComponentType は std::string_view GetStableName() const を持つ必要があります。"
		);

		auto ptr = std::make_unique<ComponentType>(
			std::forward<Args>(args)...
		);

		auto* raw = ptr.get();
		raw->SetOwner(this);

		// 先に所有へ入れる
		mComponents.emplace_back(std::move(ptr));

		// 型索引へ登録
		const TypeId typeId = HashTypeName(ComponentType{}.GetStableName());
		mComponentsByType[typeId].emplace_back(raw);

		raw->OnAttached();
		return raw;
	}

	template <typename ComponentType>
	ComponentType* UEntity::GetComponent() {
		static_assert(
			std::is_base_of_v<UBaseComponent, ComponentType>,
			"ComponentType は UBaseComponent の派生クラスでなければなりません。"
		);
		static_assert(
			requires { ComponentType{}.GetStableName(); },
			"ComponentType は std::string_view GetStableName() const を持つ必要があります。"
		);

		const TypeId typeId = HashTypeName(ComponentType{}.GetStableName());

		const auto it = mComponentsByType.find(typeId);
		if (it == mComponentsByType.end() || it->second.empty()) {
			return nullptr;
		}
		return static_cast<ComponentType*>(it->second.front());
	}

	template <typename ComponentType>
	const ComponentType* UEntity::GetComponent() const {
		static_assert(
			std::is_base_of_v<UBaseComponent, ComponentType>,
			"T は UBaseComponent の派生クラスでなければなりません。"
		);
		static_assert(
			requires { ComponentType{}.GetStableName(); },
			"ComponentType は std::string_view GetStableName() const を持つ必要があります。"
		);

		const TypeId typeId = HashTypeName(ComponentType{}.GetStableName());

		const auto it = mComponentsByType.find(typeId);
		if (it == mComponentsByType.end() || it->second.empty()) {
			return nullptr;
		}
		return static_cast<const ComponentType*>(it->second.front());
	}

	template <typename ComponentType>
	void UEntity::GetComponents(std::vector<ComponentType*>& out) {
		static_assert(
			std::is_base_of_v<UBaseComponent, ComponentType>,
			"T は UBaseComponent の派生クラスでなければなりません。"
		);
		static_assert(
			requires { ComponentType{}.GetStableName(); },
			"ComponentType は std::string_view GetStableName() const を持つ必要があります。"
		);

		out.clear();
		const TypeId typeId = HashTypeName(ComponentType{}.GetStableName());
		const auto   it     = mComponentsByType.find(typeId);
		if (it == mComponentsByType.end()) return;

		out.reserve(it->second.size());
		for (UBaseComponent* c : it->second) {
			out.push_back(static_cast<ComponentType*>(c));
		}
	}

	template <typename ComponentType>
	void UEntity::GetComponents(std::vector<const ComponentType*>& out) const {
		static_assert(
			std::is_base_of_v<UBaseComponent, ComponentType>,
			"T は UBaseComponent の派生クラスでなければなりません。"
		);
		static_assert(
			requires { ComponentType{}.GetStableName(); },
			"ComponentType は std::string_view GetStableName() const を持つ必要があります。"
		);

		out.clear();
		const TypeId typeId = HashTypeName(ComponentType{}.GetStableName());
		const auto   it     = mComponentsByType.find(typeId);
		if (it == mComponentsByType.end()) return;

		out.reserve(it->second.size());
		for (UBaseComponent* c : it->second) {
			out.push_back(static_cast<const ComponentType*>(c));
		}
	}

	template <typename ComponentType, typename... Args>
	ComponentType* UEntity::GetOrAddComponent(Args&&... args) {
		if (auto* component = GetComponent<ComponentType>()) {
			return component;
		}
		return AddComponent<ComponentType>(std::forward<Args>(args)...);
	}

	template <typename Func>
	bool UEntity::ForEachComponent(Func&& func) {
		// RemoveComponent 等で mComponents が変更される可能性に備えてスナップショットを取る
		std::vector<UBaseComponent*> snapshot;
		snapshot.reserve(mComponents.size());
		for (auto& uptr : mComponents) { snapshot.push_back(uptr.get()); }

		for (UBaseComponent* c : snapshot) {
			if (!c) continue;
			if constexpr (std::is_same_v<
				std::invoke_result_t<Func, UBaseComponent&>, bool>) {
				if (!std::forward<Func>(func)(*c)) { return false; }
			} else { std::forward<Func>(func)(*c); }
		}
		return true;
	}

	template <typename Func>
	bool UEntity::ForEachComponent(Func&& func) const {
		std::vector<const UBaseComponent*> snapshot;
		snapshot.reserve(mComponents.size());
		for (const auto& uptr : mComponents) { snapshot.push_back(uptr.get()); }

		for (const UBaseComponent* c : snapshot) {
			if (!c) continue;
			if constexpr (std::is_same_v<
				std::invoke_result_t<Func, const UBaseComponent&>, bool>) {
				if (!std::forward<Func>(func)(*c)) { return false; }
			} else { std::forward<Func>(func)(*c); }
		}
		return true;
	}

	template <typename ComponentType, typename Func>
	bool UEntity::ForEachComponent(Func&& func) {
		static_assert(
			std::is_base_of_v<UBaseComponent, ComponentType>,
			"ComponentType は UBaseComponent の派生クラスでなければなりません。"
		);
		static_assert(
			requires { ComponentType{}.GetStableName(); },
			"ComponentType は std::string_view GetStableName() const を持つ必要があります。"
		);

		const TypeId typeId = HashTypeName(ComponentType{}.GetStableName());
		const auto   it     = mComponentsByType.find(typeId);
		if (it == mComponentsByType.end() || it->second.empty()) {
			return true;
		}

		// RemoveComponent 等で vector が変わる可能性に備えてスナップショットを取る
		const std::vector<UBaseComponent*> snapshot = it->second;
		for (UBaseComponent* base : snapshot) {
			if (!base) continue;
			auto* c = static_cast<ComponentType*>(base);
			if constexpr (std::is_same_v<
				std::invoke_result_t<Func, ComponentType&>, bool>) {
				if (!std::forward<Func>(func)(*c)) { return false; }
			} else { std::forward<Func>(func)(*c); }
		}
		return true;
	}

	template <typename ComponentType, typename Func>
	bool UEntity::ForEachComponent(Func&& func) const {
		static_assert(
			std::is_base_of_v<UBaseComponent, ComponentType>,
			"ComponentType は UBaseComponent の派生クラスでなければなりません。"
		);
		static_assert(
			requires { ComponentType{}.GetStableName(); },
			"ComponentType は std::string_view GetStableName() const を持つ必要があります。"
		);

		const TypeId typeId = HashTypeName(ComponentType{}.GetStableName());
		const auto   it     = mComponentsByType.find(typeId);
		if (it == mComponentsByType.end() || it->second.empty()) {
			return true;
		}

		const std::vector<UBaseComponent*> snapshot = it->second;
		for (UBaseComponent* base : snapshot) {
			if (!base) continue;
			const auto* c = static_cast<const ComponentType*>(base);
			if constexpr (std::is_same_v<
				std::invoke_result_t<Func, const ComponentType&>, bool>) {
				if (!std::forward<Func>(func)(*c)) { return false; }
			} else { std::forward<Func>(func)(*c); }
		}
		return true;
	}
}
