#include "UBaseComponent.h"

namespace Unnamed {
	UBaseComponent::UBaseComponent()  = default;
	UBaseComponent::~UBaseComponent() = default;

	void UBaseComponent::OnAttached() {}
	void UBaseComponent::OnDetached() {}
	void UBaseComponent::PrePhysicsTick(float) {}
	void UBaseComponent::OnTick(float) {}
	void UBaseComponent::PostPhysicsTick(float) {}
	void UBaseComponent::OnPreRender() const {}
	void UBaseComponent::OnRender() const {}
	void UBaseComponent::OnPostRender() const {}
	void UBaseComponent::OnEditorTick(float) {}
	void UBaseComponent::OnEditorRender() const {}

	UEntity* UBaseComponent::GetOwner() const { return mOwner; }

	void UBaseComponent::SetOwner(UEntity* owner) { mOwner = owner; }

	bool UBaseComponent::IsActive() const noexcept { return mIsActive; }

	void UBaseComponent::SetActive(const bool isActive) noexcept {
		mIsActive = isActive;
	}

	uint64_t UBaseComponent::GetGuid() const noexcept { return mGuid; }

	void UBaseComponent::SetGuid(const uint64_t guid) noexcept { mGuid = guid; }

	TypeId UBaseComponent::GetTypeId() const {
		return HashTypeName(GetStableName());
	}
}
