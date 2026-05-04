#include "EditorLuaSystem.h"

#include <thirdparty/lua/lua.hpp>

#include "core/assets/loader/TextureLoaderDirectXTex.h"

namespace Unnamed {
	namespace {
		void SetError(std::string* outError, const char* message) {
			if (outError) {
				*outError = message ? message : "Unknown Lua error";
			}
		}

		int LuaUiText(lua_State* lua) {
			const char* text = luaL_checkstring(lua, 1);
			ImGui::TextUnformatted(text);
			return 0;
		}

		int LuaUiButton(lua_State* lua) {
			const char* text    = luaL_checkstring(lua, 1);
			const bool  pressed = ImGui::Button(text);

			lua_pushboolean(lua, pressed);
			return 1;
		}

		void RegisterEditorLuaBindings(lua_State* lua) {
			lua_register(lua, "Ui_Text", LuaUiText);
			lua_register(lua, "Ui_Button", LuaUiButton);
		}
	}

	bool EditorLuaSystem::Init() {
		if (mState) {
			return true;
		}

		mState = luaL_newstate();
		if (!mState) {
			return false;
		}

		luaL_openlibs(mState);
		RegisterEditorLuaBindings(mState);

		return true;
	}

	void EditorLuaSystem::Shutdown() {
		if (mState) {
			lua_close(mState);
			mState = nullptr;
		}
	}

	const std::string_view EditorLuaSystem::GetName() const {
		return "EditorLua";
	}

	bool EditorLuaSystem::ExecuteString(
		const std::string& src, std::string* outError
	) const {
		if (!mState) {
			SetError(outError, "Lua state is not initialized");
			return false;
		}

		if (outError) {
			outError->clear();
		}

		const int loadResult = luaL_loadbuffer(
			mState,
			src.data(),
			src.size(),
			"EditorGuiScript"
		);

		if (loadResult != LUA_OK) {
			SetError(outError, lua_tostring(mState, -1));
			lua_pop(mState, 1);
			return false;
		}

		const int callResult = lua_pcall(mState, 0, 0, 0);
		if (callResult != LUA_OK) {
			SetError(outError, lua_tostring(mState, -1));
			lua_pop(mState, 1);
			return false;
		}

		return true;
	}

	bool EditorLuaSystem::CallFunction(
		const char* name, std::string* outError
	) const {
		if (!mState) {
			SetError(outError, "Lua state is not initialized");
			return false;
		}

		if (!name || name[0] == '\0') {
			SetError(outError, "Lua function name is empty");
			return false;
		}

		if (outError) {
			outError->clear();
		}

		lua_getglobal(mState, name);

		if (!lua_isfunction(mState, -1)) {
			lua_pop(mState, 1);

			std::string message = "Lua function not found: ";
			message             += name;
			SetError(outError, message.c_str());
			return false;
		}

		const int callResult = lua_pcall(mState, 0, 0, 0);
		if (callResult != LUA_OK) {
			SetError(outError, lua_tostring(mState, -1));
			lua_pop(mState, 1);
			return false;
		}

		return true;
	}
}
