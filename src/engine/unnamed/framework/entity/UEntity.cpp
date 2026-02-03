#include "UEntity.h"

#include "core/guidgenerator/GuidGenerator.h"

#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed {
	static constexpr std::string_view kChannel = "UEntity";

	UEntity::UEntity(
		const std::string_view& name,
		const uint64_t          guid,
		const bool              isEditorOnly
	) :
		mName(name),
		mGuid(guid),
		mIsEditorOnly(isEditorOnly) {}

	UEntity::~UEntity() { OnDestroy(); };

	void UEntity::OnRegister() {
		DevMsg(
			kChannel,
			"Entity '{}' (GUID: {}) registered.",
			mName,
			std::to_string(mGuid)
		);
	}

	void UEntity::PostRegister() {
		DevMsg(
			kChannel,
			"Entity '{}' (GUID: {}) post-registered.",
			mName,
			std::to_string(mGuid)
		);
	}

	void UEntity::PrePhysicsTick(const float deltaTime) const {
		if (!mIsActive) { return; }

		for (const auto& component : mComponents) {
			if (!component->IsActive()) continue;
			component->PrePhysicsTick(deltaTime);
		}
	}

	void UEntity::Tick(const float deltaTime) const {
		if (!mIsActive) { return; }

		for (const auto& component : mComponents) {
			if (!component->IsActive()) continue;
			component->OnTick(deltaTime);
		}
	}

	void UEntity::PostPhysicsTick(const float deltaTime) const {
		if (!mIsActive) { return; }

		for (const auto& component : mComponents) {
			if (!component->IsActive()) continue;
			component->PostPhysicsTick(deltaTime);
		}
	}

	void UEntity::OnPreRender() const {
		if (!mIsActive) { return; }

		for (const auto& component : mComponents) {
			if (!component->IsActive()) continue;
			component->OnPreRender();
		}
	}

	void UEntity::OnRender() const {
		if (!mIsActive) { return; }

		for (const auto& component : mComponents) {
			if (!component->IsActive()) continue;
			component->OnRender();
		}
	}

	void UEntity::OnPostRender() const {
		if (!mIsActive) { return; }

		for (const auto& component : mComponents) {
			if (!component->IsActive()) continue;
			component->OnPostRender();
		}
	}

	void UEntity::OnEditorTick(const float deltaTime) const {
		if (!mIsActive) { return; }

		for (const auto& component : mComponents) {
			component->OnEditorTick(deltaTime);
		}
	}

	void UEntity::OnEditorRender() const {
		if (!mIsActive) { return; }

		for (const auto& component : mComponents) {
			component->OnEditorRender();
		}
	}

	void UEntity::OnDestroy() {
		DevMsg(
			kChannel,
			"Entity '{}' (GUID: {}) destroyed.",
			mName,
			std::to_string(mGuid)
		);

		for (const auto& component : mComponents) { component->OnDetached(); }
		mComponents.clear();
	}

	UBaseComponent* UEntity::AddComponentInstance(
		std::unique_ptr<UBaseComponent> component
	) {
		if (!component) return nullptr;

		component->SetOwner(this);
		UBaseComponent* raw = component.get();

		mComponents.emplace_back(std::move(component));

		// TypeId索引（あなたの自前TypeId方式）
		mComponentsByType[raw->GetTypeId()].emplace_back(raw);

		raw->OnAttached();
		return raw;
	}

	void UEntity::RemoveComponent(UBaseComponent* component) {
		if (!component) return;

		// 索引から抜く
		{
			const auto key = component->GetTypeId();
			auto       it  = mComponentsByType.find(key);
			if (it != mComponentsByType.end()) {
				auto& vec = it->second;
				std::erase(vec, component);
				if (vec.empty()) { mComponentsByType.erase(it); }
			}
		}

		// 所有配列から抜く
		const auto it = std::ranges::find_if(
			mComponents,
			[component](const std::unique_ptr<UBaseComponent>& comp) {
				return comp.get() == component;
			}
		);

		if (it != mComponents.end()) {
			(*it)->OnDetached();
			mComponents.erase(it);
		}
	}

	std::string_view UEntity::GetName() const { return mName; }

	void UEntity::SetName(const std::string_view& name) { mName = name; }

	bool UEntity::IsEditorOnly() const noexcept { return mIsEditorOnly; }

	bool UEntity::IsActive() const noexcept { return mIsActive; }

	void UEntity::SetActive(const bool isActive) noexcept {
		mIsActive = isActive;
	}

	uint64_t UEntity::GetGuid() const noexcept { return mGuid; }
}
