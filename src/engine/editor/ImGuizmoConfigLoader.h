#pragma once
#include <string>

#ifdef _DEBUG
namespace Unnamed {
	class ConsoleSystem;

	class ImGuizmoConfigLoader {
	public:
		ImGuizmoConfigLoader(std::string configPath);
	};
}
#endif
