#include "BaseComponent.h"

#include "../../entity/Entity.h"

namespace Unnamed {
	BaseComponent::BaseComponent()  = default;
	BaseComponent::~BaseComponent() = default;

	void BaseComponent::OnAttached() {}
	void BaseComponent::OnDetached() {}
	void BaseComponent::PrePhysicsTick(float) {}
	void BaseComponent::OnTick(float) {}
	void BaseComponent::PostPhysicsTick(float) {}
	void BaseComponent::OnPreRender() const {}
	void BaseComponent::OnRender() const {}
	void BaseComponent::OnPostRender() const {}
	void BaseComponent::OnEditorTick(float) {}
	void BaseComponent::OnEditorRender() const {}

#ifdef _DEBUG
	void BaseComponent::DrawInspectorImGui() {}
#endif

	Entity* BaseComponent::GetOwner() const {
		return mOwner;
	}

	void BaseComponent::SetOwner(Entity* owner) {
		mOwner = owner;
	}

	UScene* BaseComponent::GetScene() const noexcept {
		return mOwner ? mOwner->GetScene() : nullptr;
	}

	UWorld* BaseComponent::GetWorld() const noexcept {
		return mOwner ? mOwner->GetWorld() : nullptr;
	}

	bool BaseComponent::IsActive() const noexcept {
		return mIsActive;
	}

	void BaseComponent::SetActive(const bool isActive) noexcept {
		mIsActive = isActive;
	}

	uint64_t BaseComponent::GetGuid() const noexcept {
		return mGuid;
	}

	void BaseComponent::SetGuid(const uint64_t guid) noexcept {
		mGuid = guid;
	}

	TypeId BaseComponent::GetTypeId() const {
		return HashTypeName(GetStableName());
	}
}
