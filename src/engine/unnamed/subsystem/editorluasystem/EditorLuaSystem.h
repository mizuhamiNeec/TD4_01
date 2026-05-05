#pragma once
#include <string>

#include <thirdparty/lua/lua.hpp>

#include "../interface/ISubsystem.h"

namespace Unnamed {
	class EditorLuaSystem : public ISubsystem {
	public:
		// ---- ISubsystem ----------------------------------------------------
		bool Init() override;
		void Shutdown() override;

		[[nodiscard]] const std::string_view GetName() const override;

		// ---- EditorLuaSystem -----------------------------------------------
		bool ExecuteString(const std::string& src, std::string* outError) const;
		bool CallFunction(const char* name, std::string* outError) const;

	private:
		lua_State* mState = nullptr;
	};
}
