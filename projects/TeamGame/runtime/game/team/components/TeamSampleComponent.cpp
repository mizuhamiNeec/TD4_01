#include "TeamSampleComponent.h"

#include "core/ComponentRegistry.h"

#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed {
	void TeamSampleComponent::OnAttached() {
		BaseComponent::OnAttached();
		const Entity* owner = GetOwner();
		const std::string_view ownerName = owner ? owner->GetName() : "<NoOwner>";
		DevMsg("TeamGame", "TeamSampleComponent attached to '{}'", ownerName);
	}

	void TeamSampleComponent::Deserialize(const JsonReader& reader) {
		(void)reader;
	}

	void TeamSampleComponent::Serialize(JsonWriter& writer) const {
		(void)writer;
	}

	REGISTER_COMPONENT(TeamSampleComponent);
}
