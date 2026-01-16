#include "ConVarWriter.h"

#include <fstream>

#include <engine/unnamed/subsystem/console/ConsoleSystem.h>
#include <engine/unnamed/subsystem/console/concommand/UnnamedConVar.h>
#include <engine/unnamed/subsystem/interface/ServiceLocator.h>

namespace Unnamed {
	ConVarWriter::ConVarWriter(const std::string_view path) {
		std::ofstream ofs(path.data(), std::ios::binary);

		if (!ofs) {
			throw std::runtime_error(
				"Failed to open file for writing: " + std::string(path)
			);
		}

		auto console = ServiceLocator::Get<ConsoleSystem>();
		auto vars    = console->GetConVars();

		for (const auto& var : vars) {
			if (var.second->HasFlags(FCVAR::ARCHIVE)) {
				switch (GetConVarType(var.second)) {
				case CVAR_TYPE::NONE:
					break;
				case CVAR_TYPE::BOOL: {
					auto* bVar = dynamic_cast<UnnamedConVar<bool>*>(var.second);
					std::string valueStr = static_cast<bool>(*bVar) ?
						                       "true" :
						                       "false";
					ofs << var.first << " " << valueStr << "\n";
				}
				break;

				case CVAR_TYPE::INT: {
					auto* iVar = dynamic_cast<UnnamedConVar<int>*>(var.second);
					ofs << var.first << " " << static_cast<int>(*iVar) <<
						"\n";
				}
				break;

				case CVAR_TYPE::FLOAT: {
					auto* fVar = dynamic_cast<UnnamedConVar<float>*>(var.
						second);
					ofs << var.first << " " << static_cast<float>(*fVar) <<
						"\n";
				}
				break;

				case CVAR_TYPE::DOUBLE: {
					auto* dVar = dynamic_cast<UnnamedConVar<double>*>(var.
						second);
					ofs << var.first << " " << static_cast<double>(*dVar) <<
						"\n";
				}
				break;

				case CVAR_TYPE::STRING: {
					auto* sVar = dynamic_cast<UnnamedConVar<std::string>*>(var.
						second);
					ofs << var.first << " " << static_cast<std::string>(*sVar)
						<< "\n";
				}
				break;
				case CVAR_TYPE::VEC3:
					// 使う...か?
					break;
				}
			}
		}

		ofs.flush();
	}
}
