#pragma once 
#include <engine/unnamed/framework/components/base/BaseComponent.h>

class TitleController : public Unnamed::BaseComponent {
public:
	[[nodiscard]] std::string_view GetStableName() const override;
	[[nodiscard]] std::string_view GetComponentName() const override;
	void                           Deserialize(const Unnamed::JsonReader& reader) override;
	void                           Serialize(Unnamed::JsonWriter& writer) const override;
	void OnAttached() override;
	void OnTick(float deltaTime) override;
	[[nodiscard]] TICK_GROUP GetTickGroup() const override;
};
