#include "Entity.h"

#include <algorithm>

#include "core/guidgenerator/GuidGenerator.h"

#include "engine/scene/Scene.h"
#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed {
	static constexpr std::string_view kChannel = "Entity";

	Entity::Entity(
		const std::string_view& name,
		const uint64_t          guid,
		const bool              isEditorOnly
	) :
		mName(name),
		mGuid(guid),
		mIsEditorOnly(isEditorOnly) {}

	Entity::~Entity() {
		OnDestroy();
	};

	void Entity::OnRegister() {
		DevMsg(
			kChannel,
			"Entity '{}' (GUID: {}) registered.",
			mName,
			std::to_string(mGuid)
		);
	}

	void Entity::PostRegister() {
		DevMsg(
			kChannel,
			"Entity '{}' (GUID: {}) post-registered.",
			mName,
			std::to_string(mGuid)
		);
	}

	void Entity::PrePhysicsTick(const float deltaTime) const {
		if (!mIsActive) {
			return;
		}

		for (const auto& component : mComponents) {
			if (!component->IsActive()) {
				continue;
			}
			component->PrePhysicsTick(deltaTime);
		}
	}

	uint32_t Entity::PrePhysicsTick(
		const float                    deltaTime,
		const BaseComponent::TickGroup group
	) const {
		if (!mIsActive) {
			return 0;
		}

		uint32_t invokedCount = 0;
		for (const auto& component : mComponents) {
			if (!component->IsActive()) {
				continue;
			}
			if (component->GetTickGroup() != group) {
				continue;
			}
			component->PrePhysicsTick(deltaTime);
			++invokedCount;
		}
		return invokedCount;
	}

	void Entity::Tick(const float deltaTime) const {
		if (!mIsActive) {
			return;
		}

		for (const auto& component : mComponents) {
			if (!component->IsActive()) {
				continue;
			}
			component->OnTick(deltaTime);
		}
	}

	uint32_t Entity::Tick(
		const float                    deltaTime,
		const BaseComponent::TickGroup group
	) const {
		if (!mIsActive) {
			return 0;
		}

		uint32_t invokedCount = 0;
		for (const auto& component : mComponents) {
			if (!component->IsActive()) {
				continue;
			}
			if (component->GetTickGroup() != group) {
				continue;
			}
			component->OnTick(deltaTime);
			++invokedCount;
		}
		return invokedCount;
	}

	void Entity::PostPhysicsTick(const float deltaTime) const {
		if (!mIsActive) {
			return;
		}

		for (const auto& component : mComponents) {
			if (!component->IsActive()) {
				continue;
			}
			component->PostPhysicsTick(deltaTime);
		}
	}

	uint32_t Entity::PostPhysicsTick(
		const float                    deltaTime,
		const BaseComponent::TickGroup group
	) const {
		if (!mIsActive) {
			return 0;
		}

		uint32_t invokedCount = 0;
		for (const auto& component : mComponents) {
			if (!component->IsActive()) {
				continue;
			}
			if (component->GetTickGroup() != group) {
				continue;
			}
			component->PostPhysicsTick(deltaTime);
			++invokedCount;
		}
		return invokedCount;
	}

	void Entity::OnPreRender() const {
		if (!mIsActive) {
			return;
		}

		for (const auto& component : mComponents) {
			if (!component->IsActive()) {
				continue;
			}
			component->OnPreRender();
		}
	}

	void Entity::OnRender() const {
		if (!mIsActive) {
			return;
		}

		for (const auto& component : mComponents) {
			if (!component->IsActive()) {
				continue;
			}
			component->OnRender();
		}
	}

	void Entity::OnPostRender() const {
		if (!mIsActive) {
			return;
		}

		for (const auto& component : mComponents) {
			if (!component->IsActive()) {
				continue;
			}
			component->OnPostRender();
		}
	}

	void Entity::OnEditorTick(const float deltaTime) const {
		if (!mIsActive) {
			return;
		}

		for (const auto& component : mComponents) {
			component->OnEditorTick(deltaTime);
		}
	}

	void Entity::OnEditorRender() const {
		if (!mIsActive) {
			return;
		}

		for (const auto& component : mComponents) {
			component->OnEditorRender();
		}
	}

	void Entity::OnDestroy() {
		if (mDestroyed) {
			return;
		}
		mDestroyed = true;

		DevMsg(
			kChannel,
			"Entity '{}' (GUID: {}) destroyed.",
			mName,
			std::to_string(mGuid)
		);

		for (const auto& component : mComponents) {
			component->OnDetached();
		}
		mComponents.clear();
		mComponentsByType.clear();
	}

	bool Entity::IsPendingDestroy() const noexcept {
		return mPendingDestroy;
	}

	void Entity::MarkPendingDestroy() noexcept {
		mPendingDestroy = true;
	}

	BaseComponent* Entity::AddComponentInstance(
		std::unique_ptr<BaseComponent> component
	) {
		if (!component) {
			return nullptr;
		}

		component->SetOwner(this);
		BaseComponent* raw = component.get();

		mComponents.emplace_back(std::move(component));

		// TypeId索引（あなたの自前TypeId方式）
		mComponentsByType[raw->GetTypeId()].emplace_back(raw);

		raw->OnAttached();
		return raw;
	}

	Scene* Entity::GetScene() const noexcept {
		return mScene;
	}

	void Entity::SetScene(Scene* scene) noexcept {
		mScene = scene;
	}

	World* Entity::GetWorld() const noexcept {
		return mScene ? mScene->GetWorld() : nullptr;
	}

	std::string_view Entity::GetName() const {
		return mName;
	}

	void Entity::SetName(const std::string_view& name) {
		mName = name;
	}

	bool Entity::IsEditorOnly() const noexcept {
		return mIsEditorOnly;
	}

	bool Entity::IsActive() const noexcept {
		return mIsActive;
	}

	void Entity::SetActive(const bool isActive) noexcept {
		mIsActive = isActive;
	}

	bool Entity::IsVisible() const noexcept {
		return mIsVisible;
	}

	void Entity::SetVisible(const bool isVisible) noexcept {
		mIsVisible = isVisible;
	}

	uint64_t Entity::GetGuid() const noexcept {
		return mGuid;
	}

	std::string_view Entity::GetFolderPath() const noexcept {
		return mFolderPath;
	}

	void Entity::SetFolderPath(const std::string_view folderPath) {
		mFolderPath = folderPath;
		std::ranges::replace(mFolderPath, '\\', '/');
		while (!mFolderPath.empty() && mFolderPath.front() == '/') {
			mFolderPath.erase(mFolderPath.begin());
		}
		while (!mFolderPath.empty() && mFolderPath.back() == '/') {
			mFolderPath.pop_back();
		}
	}
}
