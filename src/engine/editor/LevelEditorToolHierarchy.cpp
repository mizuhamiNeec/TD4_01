#ifdef _DEBUG
#include "LevelEditorTool.h"

#include <algorithm>
#include <array>
#include <format>
#include <functional>
#include <imgui.h>
#include <map>
#include <set>
#include <unordered_set>

#include "core/ComponentRegistry.h"
#include "core/assets/AssetType.h"
#include "core/string/StrUtil.h"

#include "engine/ImGui/Icons.h"
#include "engine/ImGui/ImGuiUtil.h"
#include "engine/ImGui/ImGuiWidgets.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"

namespace Unnamed {
	namespace {
		struct OutlinerFolderNode {
			std::map<std::string, OutlinerFolderNode> children;
			std::vector<Entity*>                      entities;
		};

		struct ComponentMenuNode {
			std::map<std::string, ComponentMenuNode>                children;
			std::vector<ComponentRegistry::RegisteredComponentInfo> components;
		};

		bool IsValidComponentStableNameForMenu(std::string_view stableName) {
			if (stableName.empty()) {
				return false;
			}
			if (stableName.front() == '.' || stableName.back() == '.') {
				return false;
			}
			return true;
		}

		bool InsertComponentMenuEntry(
			ComponentMenuNode&                                root,
			const ComponentRegistry::RegisteredComponentInfo& info
		) {
			if (!IsValidComponentStableNameForMenu(info.stableName)) {
				return false;
			}

			ComponentMenuNode* node    = &root;
			const size_t       lastDot = info.stableName.find_last_of('.');
			if (lastDot != std::string::npos) {
				size_t begin = 0;
				while (begin < lastDot) {
					const size_t end = info.stableName.find('.', begin);
					if (end == std::string::npos || end > lastDot) {
						break;
					}

					const size_t len = end - begin;
					if (len > 0) {
						node = &node->children[info.stableName.substr(
							begin, len
						)];
					}
					begin = end + 1;
				}
			}

			node->components.emplace_back(info);
			return true;
		}

		void SortComponentMenuTree(ComponentMenuNode& node) {
			std::ranges::sort(
				node.components,
				[](
				const ComponentRegistry::RegisteredComponentInfo& lhs,
				const ComponentRegistry::RegisteredComponentInfo& rhs
			) {
					const std::string_view lhsLabel =
						lhs.displayName.empty() ?
							lhs.stableName :
							lhs.displayName;
					const std::string_view rhsLabel =
						rhs.displayName.empty() ?
							rhs.stableName :
							rhs.displayName;
					return lhsLabel < rhsLabel;
				}
			);

			for (auto& child : node.children | std::views::values) {
				SortComponentMenuTree(child);
			}
		}

		bool DrawComponentAddMenuRecursive(
			const ComponentMenuNode&               node,
			Entity&                                entity,
			const std::unordered_set<std::string>& existingStableNames
		) {
			bool added = false;

			for (const auto& [scopeName, childNode] : node.children) {
				if (scopeName.empty()) {
					continue;
				}

				if (ImGuiWidgets::BeginMenuEx(
					scopeName.c_str(),
					StrUtil::ConvertToUtf8(kIconBomb).c_str(), true, 4.0f
				)) {
					added |= DrawComponentAddMenuRecursive(
						childNode, entity, existingStableNames
					);
					ImGui::EndMenu();
				}
			}

			for (const auto& info : node.components) {
				if (!IsValidComponentStableNameForMenu(info.stableName)) {
					continue;
				}

				const std::string visibleLabel =
					info.displayName.empty() ?
						info.stableName :
						info.displayName;

				const std::string label =
					visibleLabel + "###" + info.stableName;

				const bool alreadyExists =
					existingStableNames.contains(info.stableName);

				if (
					ImGuiWidgets::MenuItemWithIcon(
						label.c_str(), kIconQuestionMark, nullptr, false,
						!alreadyExists
					)
				) {
					auto component = ComponentRegistry::Get().Create(
						info.stableName
					);
					if (!component) {
						continue;
					}
					entity.AddComponentInstance(std::move(component));
					added = true;
				}
			}

			return added;
		}

		std::vector<std::string> SplitFolderPath(std::string_view folderPath) {
			std::vector<std::string> parts;
			size_t                   begin = 0;
			while (begin < folderPath.size()) {
				const size_t end = folderPath.find('/', begin);
				const size_t len = (end == std::string_view::npos ?
					                    folderPath.size() :
					                    end) - begin;
				if (len > 0) {
					parts.emplace_back(folderPath.substr(begin, len));
				}
				if (end == std::string_view::npos) {
					break;
				}
				begin = end + 1;
			}
			return parts;
		}

		std::string NormalizeFolderPath(const std::string_view folderPath) {
			std::string path(folderPath);
			std::ranges::replace(path, '\\', '/');
			while (!path.empty() && path.front() == '/') {
				path.erase(path.begin());
			}
			while (!path.empty() && path.back() == '/') {
				path.pop_back();
			}
			return path;
		}

		std::string JoinFolderPath(
			std::string_view parent, std::string_view child
		) {
			if (parent.empty()) {
				return std::string(child);
			}
			if (child.empty()) {
				return std::string(parent);
			}
			return std::string(parent) + "/" + std::string(child);
		}

		std::string GetFolderLeafName(std::string_view folderPath) {
			const size_t slashPos = folderPath.find_last_of('/');
			return slashPos == std::string_view::npos ?
				       std::string(folderPath) :
				       std::string(folderPath.substr(slashPos + 1));
		}

		bool IsFolderEqualOrDescendant(
			const std::string_view path, const std::string_view ancestor
		) {
			if (ancestor.empty()) {
				return true;
			}
			if (path == ancestor) {
				return true;
			}
			if (path.size() <= ancestor.size()) {
				return false;
			}
			return path.starts_with(ancestor) &&
			       path[ancestor.size()] == '/';
		}

		OutlinerFolderNode* EnsureFolderNode(
			OutlinerFolderNode& root, std::string_view folderPath
		) {
			OutlinerFolderNode* node = &root;
			for (const auto& part : SplitFolderPath(folderPath)) {
				node = &node->children[part];
			}
			return node;
		}

		void BuildOutlinerTree(
			OutlinerFolderNode& root, const Scene& scene
		) {
			for (const std::string& folder : scene.GetFolders()) {
				EnsureFolderNode(root, folder);
			}
			for (const auto& ePtr : scene.GetEntities()) {
				if (!ePtr) {
					continue;
				}
				Entity*             entity = ePtr.get();
				OutlinerFolderNode* node   = EnsureFolderNode(
					root, entity->GetFolderPath()
				);
				node->entities.emplace_back(entity);
			}
		}

		std::string MakeUniqueFolderPath(
			const Scene& scene, std::string_view parentFolderPath
		) {
			const std::string parent = NormalizeFolderPath(parentFolderPath);
			int               suffix = 0;
			for (;;) {
				const std::string candidateName =
					suffix == 0 ?
						"NewFolder" :
						"NewFolder" + std::to_string(suffix);
				const std::string candidatePath = JoinFolderPath(
					parent, candidateName
				);
				if (
					std::ranges::find(scene.GetFolders(), candidatePath) ==
					scene.GetFolders().end()
				) {
					return candidatePath;
				}
				++suffix;
			}
		}

		bool IsTransformAncestor(
			const TransformComponent* possibleAncestor,
			const TransformComponent* node
		) {
			const TransformComponent* current = node;
			while (current) {
				if (current == possibleAncestor) {
					return true;
				}
				current = current->Parent();
			}
			return false;
		}

	}

	void LevelEditorTool::DrawSceneOutliner() {
		Scene* scene = GetOutlinerScene();
		if (!scene) {
			return;
		}

		if (!ImGui::Begin("Outliner")) {
			ImGui::End();
			return;
		}

		static uint64_t              renameEntityId = 0;
		static std::string           renameFolderPath;
		static std::array<char, 256> renameBuffer = {};

		auto openRenameEntity = [&](Entity& entity) {
			renameEntityId = entity.GetGuid();
			renameFolderPath.clear();
			std::ranges::fill(renameBuffer, '\0');
			const std::string name(entity.GetName());
			memcpy(
				renameBuffer.data(),
				name.c_str(),
				std::min(name.size(), renameBuffer.size() - 1)
			);
			ImGui::OpenPopup("OutlinerRenamePopup");
		};

		auto openRenameFolder = [&](std::string_view folderPath) {
			renameEntityId   = 0;
			renameFolderPath = std::string(folderPath);
			std::ranges::fill(renameBuffer, '\0');
			const std::string leafName = GetFolderLeafName(folderPath);
			memcpy(
				renameBuffer.data(),
				leafName.c_str(),
				std::min(leafName.size(), renameBuffer.size() - 1)
			);
			ImGui::OpenPopup("OutlinerRenamePopup");
		};

		constexpr ImGuiTableFlags tableFlags =
			ImGuiTableFlags_ScrollY |
			ImGuiTableFlags_BordersV |
			ImGuiTableFlags_BordersOuterH |
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_NoBordersInBodyUntilResize;

		auto createEntity = [&](std::string_view folderPath) {
			Entity& entity = scene->CreateEntity(
				"Entity", scene->AllocateEntityId(), false
			);
			entity.SetFolderPath(folderPath);
			scene->AddFolder(folderPath);
			entity.SetVisible(true);
			[[maybe_unused]] auto* addedTransform =
				entity.AddComponent<TransformComponent>();
			mSelectedEntityId = entity.GetGuid();
		};
		auto createFolder = [&](std::string_view folderPath) {
			scene->AddFolder(MakeUniqueFolderPath(*scene, folderPath));
		};

		if (ImGui::BeginPopupContextWindow(
			"OutlinerWindowContext",
			ImGuiPopupFlags_MouseButtonRight |
			ImGuiPopupFlags_NoOpenOverItems
		)) {
			if (ImGui::MenuItem("Add Entity")) {
				createEntity("");
			}
			if (ImGui::MenuItem("Add Folder")) {
				createFolder("");
			}
			ImGui::EndPopup();
		}

		float iconScale = 1.5f;

		auto addButtonSize = ImVec2(
			ImGui::GetFontSize() * iconScale,
			ImGui::GetFontSize() * iconScale
		);

		if (
			ImGuiWidgets::IconButton(
				kIconAdd,
				nullptr,
				addButtonSize,
				iconScale
			)
		) {
			ImGui::OpenPopup("OutlinerAddPopup");
		}
		if (ImGui::BeginPopup("OutlinerAddPopup")) {
			if (ImGui::MenuItem("Add Entity")) {
				createEntity("");
			}
			if (ImGui::MenuItem("Add Folder")) {
				createFolder("");
			}
			ImGui::EndPopup();
		}

		if (ImGui::BeginTable("OutlinerTable", 3, tableFlags)) {
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
			ImGui::TableSetupColumn(
				"Visible", ImGuiTableColumnFlags_WidthFixed
			);
			ImGui::TableSetupColumn(
				"Active", ImGuiTableColumnFlags_WidthFixed
			);
			ImGui::TableHeadersRow();

			OutlinerFolderNode root = {};
			BuildOutlinerTree(root, *scene);
			uint64_t    pendingDeleteEntityId = 0;
			std::string pendingDeleteFolderPath;
			bool        pendingCreateEntity = false;
			bool        pendingCreateFolder = false;
			std::string pendingCreateFolderPath;
			uint64_t    pendingParentChildEntityId  = 0;
			uint64_t    pendingParentTargetEntityId = 0;
			uint64_t    pendingMoveEntityId         = 0;
			std::string pendingMoveEntityFolderPath;
			std::string pendingMoveFolderSourcePath;
			std::string pendingMoveFolderTargetPath;
			std::unordered_map<uint64_t, std::vector<Entity*>>
				childEntitiesByParent;
			for (const auto& entityPtr : scene->GetEntities()) {
				if (!entityPtr) {
					continue;
				}
				Entity*     entity    = entityPtr.get();
				const auto* transform = entity->GetComponent<
					TransformComponent>();
				if (!transform || !transform->Parent() || !transform->Parent()->
				    GetOwner()) {
					continue;
				}
				childEntitiesByParent[transform->Parent()->GetOwner()->
				                                 GetGuid()]
					.emplace_back(entity);
			}

			std::function<void(Entity*)> drawEntityNode;
			drawEntityNode = [&](Entity* entity) {
				if (!entity) {
					return;
				}
				std::vector<Entity*> childrenInSameFolder;
				if (const auto it = childEntitiesByParent.
						find(entity->GetGuid());
					it != childEntitiesByParent.end()) {
					for (Entity* child : it->second) {
						if (
							child &&
							std::string(child->GetFolderPath()) ==
							std::string(entity->GetFolderPath())
						) {
							childrenInSameFolder.emplace_back(child);
						}
					}
				}

				const bool isSelected = entity->GetGuid() == mSelectedEntityId;
				ImGui::PushID(static_cast<int>(entity->GetGuid()));
				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				const bool hasChildren = !childrenInSameFolder.empty();
				const ImGuiTreeNodeFlags nodeFlags =
					ImGuiTreeNodeFlags_SpanFullWidth |
					(hasChildren ?
						 ImGuiTreeNodeFlags_DefaultOpen |
						 ImGuiTreeNodeFlags_OpenOnDoubleClick :
						 ImGuiTreeNodeFlags_Leaf |
						 ImGuiTreeNodeFlags_NoTreePushOnOpen) |
					(isSelected ? ImGuiTreeNodeFlags_Selected : 0);
				ImGui::BeginDisabled(!entity->IsActive());
				const bool opened = ImGui::TreeNodeEx(
					reinterpret_cast<void*>(
						entity->GetGuid()
					),
					nodeFlags,
					"%s",
					entity->GetName().data()
				);
				ImGui::EndDisabled();
				if (ImGui::IsItemClicked()) {
					mSelectedEntityId = entity->GetGuid();
				}
				if (ImGui::BeginDragDropSource()) {
					const uint64_t guid = entity->GetGuid();
					ImGui::SetDragDropPayload(
						"OUTLINER_ENTITY", &guid, sizeof(guid)
					);
					ImGui::TextUnformatted(entity->GetName().data());
					ImGui::EndDragDropSource();
				}
				if (ImGui::BeginDragDropTarget()) {
					if (const ImGuiPayload* payload =
						ImGui::AcceptDragDropPayload(
							"OUTLINER_ENTITY"
						)) {
						const uint64_t draggedGuid = *static_cast<const uint64_t
							*>(
							payload->Data
						);
						if (draggedGuid != entity->GetGuid()) {
							pendingParentChildEntityId  = draggedGuid;
							pendingParentTargetEntityId = entity->GetGuid();
						}
					}
					if (const ImGuiPayload* payload =
						ImGui::AcceptDragDropPayload(
							"OUTLINER_FOLDER"
						)) {
						const char* sourcePath = static_cast<const char*>(
							payload->Data
						);
						if (sourcePath) {
							pendingMoveFolderSourcePath = sourcePath;
							pendingMoveFolderTargetPath = std::string(
								entity->GetFolderPath()
							);
						}
					}
					ImGui::EndDragDropTarget();
				}
				if (ImGui::BeginPopupContextItem("EntityContext")) {
					if (ImGui::MenuItem("Add Entity")) {
						pendingCreateEntity     = true;
						pendingCreateFolderPath = std::string(
							entity->GetFolderPath()
						);
					}
					if (ImGui::MenuItem("Add Folder")) {
						pendingCreateFolder     = true;
						pendingCreateFolderPath = std::string(
							entity->GetFolderPath()
						);
					}
					if (ImGui::MenuItem("Rename")) {
						openRenameEntity(*entity);
					}
					if (ImGui::MenuItem("Delete")) {
						pendingDeleteEntityId = entity->GetGuid();
					}
					ImGui::EndPopup();
				}

				const auto fontSize = ImVec2(
					ImGui::GetFontSize(), ImGui::GetFontSize()
				);

				ImGui::TableNextColumn();
				const bool visible = entity->IsVisible();
				if (
					ImGui::Button(
						StrUtil::ConvertToUtf8(
							visible ? kIconVisibility : kIconVisibilityOff
						).c_str(),
						fontSize
					)
				) {
					entity->SetVisible(!visible);
				}

				ImGui::TableNextColumn();
				const bool active = entity->IsActive();
				if (
					ImGui::Button(
						StrUtil::ConvertToUtf8(
							active ? kIconCheckBoxOn : kIconCheckBoxOff
						).c_str(),
						fontSize
					)
				) {
					entity->SetActive(!active);
				}

				if (opened && hasChildren) {
					for (Entity* child : childrenInSameFolder) {
						drawEntityNode(child);
					}
					ImGui::TreePop();
				}

				ImGui::PopID();
			};

			std::function<void(const OutlinerFolderNode&, const std::string&)>
				drawFolder;
			drawFolder = [&](
				const OutlinerFolderNode& node,
				const std::string&        folderPath
			) {
					if (!folderPath.empty()) {
						const std::string displayName = GetFolderLeafName(
							folderPath
						);
						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						const bool opened = ImGui::TreeNodeEx(
							folderPath.c_str(),
							ImGuiTreeNodeFlags_SpanFullWidth |
							ImGuiTreeNodeFlags_DefaultOpen |
							ImGuiTreeNodeFlags_OpenOnDoubleClick,
							"%s",
							displayName.c_str()
						);
						if (ImGui::BeginDragDropSource()) {
							ImGui::SetDragDropPayload(
								"OUTLINER_FOLDER",
								folderPath.c_str(),
								folderPath.size() + 1
							);
							ImGui::TextUnformatted(displayName.c_str());
							ImGui::EndDragDropSource();
						}
						if (ImGui::BeginDragDropTarget()) {
							if (const ImGuiPayload* payload =
								ImGui::AcceptDragDropPayload(
									"OUTLINER_ENTITY"
								)) {
								const uint64_t draggedGuid = *static_cast<
									const uint64_t*>(payload->Data);
								pendingMoveEntityId         = draggedGuid;
								pendingMoveEntityFolderPath = folderPath;
							}
							if (const ImGuiPayload* payload =
								ImGui::AcceptDragDropPayload(
									"OUTLINER_FOLDER"
								)) {
								const char* sourcePath = static_cast<const char
									*>(
									payload->Data
								);
								if (
									sourcePath &&
									folderPath != sourcePath &&
									!IsFolderEqualOrDescendant(
										folderPath, sourcePath
									)
								) {
									pendingMoveFolderSourcePath = sourcePath;
									pendingMoveFolderTargetPath = folderPath;
								}
							}
							ImGui::EndDragDropTarget();
						}
						if (ImGui::BeginPopupContextItem("FolderContext")) {
							if (ImGui::MenuItem("Add Entity")) {
								pendingCreateEntity     = true;
								pendingCreateFolderPath = folderPath;
							}
							if (ImGui::MenuItem("Add Folder")) {
								pendingCreateFolder     = true;
								pendingCreateFolderPath = folderPath;
							}
							if (ImGui::MenuItem("Rename")) {
								openRenameFolder(folderPath);
							}
							if (ImGui::MenuItem("Delete")) {
								pendingDeleteFolderPath = folderPath;
							}
							ImGui::EndPopup();
						}
						ImGui::TableNextColumn();
						ImGui::TableNextColumn();
						if (!opened) {
							return;
						}
					}

					for (const auto& [childName, childNode] : node.children) {
						const std::string childPath = folderPath.empty() ?
							childName :
							folderPath + "/" + childName;
						drawFolder(childNode, childPath);
					}

					for (Entity* entity : node.entities) {
						const auto* transform = entity ?
							                        entity->GetComponent<
								                        TransformComponent>() :
							                        nullptr;
						const auto* parentTransform = transform ?
							transform->Parent() :
							nullptr;
						const Entity* parentEntity = parentTransform ?
							parentTransform->GetOwner() :
							nullptr;
						if (
							parentEntity &&
							std::string(parentEntity->GetFolderPath()) ==
							std::string(entity->GetFolderPath())
						) {
							continue;
						}
						drawEntityNode(entity);
					}

					if (!folderPath.empty()) {
						ImGui::TreePop();
					}
				};

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TreeNodeEx(
				"__outliner_root__",
				ImGuiTreeNodeFlags_Leaf |
				ImGuiTreeNodeFlags_NoTreePushOnOpen |
				ImGuiTreeNodeFlags_SpanFullWidth,
				"Root"
			);
			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
					"OUTLINER_FOLDER"
				)) {
					const char* sourcePath = static_cast<const char*>(payload->
						Data);
					if (sourcePath) {
						pendingMoveFolderSourcePath = sourcePath;
						pendingMoveFolderTargetPath.clear();
					}
				}
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
					"OUTLINER_ENTITY"
				)) {
					const uint64_t draggedGuid = *static_cast<const uint64_t*>(
						payload->Data
					);
					pendingMoveEntityId = draggedGuid;
					pendingMoveEntityFolderPath.clear();
				}
				ImGui::EndDragDropTarget();
			}
			if (ImGui::BeginPopupContextItem("RootContext")) {
				if (ImGui::MenuItem("Add Entity")) {
					pendingCreateEntity = true;
				}
				if (ImGui::MenuItem("Add Folder")) {
					pendingCreateFolder = true;
				}
				ImGui::EndPopup();
			}
			ImGui::TableNextColumn();
			ImGui::TableNextColumn();

			drawFolder(root, "");

			ImGui::EndTable();

			if (pendingCreateEntity) {
				createEntity(pendingCreateFolderPath);
			}
			if (pendingCreateFolder) {
				createFolder(pendingCreateFolderPath);
			}
			if (pendingMoveEntityId != 0) {
				if (Entity* entity = scene->FindEntity(pendingMoveEntityId)) {
					entity->SetFolderPath(pendingMoveEntityFolderPath);
					scene->AddFolder(pendingMoveEntityFolderPath);
				}
			}
			if (!pendingMoveFolderSourcePath.empty()) {
				scene->MoveFolderSubtree(
					pendingMoveFolderSourcePath, pendingMoveFolderTargetPath
				);
			}
			if (
				pendingParentChildEntityId != 0 &&
				pendingParentTargetEntityId != 0
			) {
				Entity* childEntity = scene->FindEntity(
					pendingParentChildEntityId
				);
				Entity* parentEntity = scene->FindEntity(
					pendingParentTargetEntityId
				);
				if (childEntity && parentEntity) {
					auto* childTransform = childEntity->GetComponent<
						TransformComponent>();
					auto* parentTransform = parentEntity->GetComponent<
						TransformComponent>();
					if (
						childTransform &&
						parentTransform &&
						childTransform != parentTransform &&
						!IsTransformAncestor(childTransform, parentTransform)
					) {
						childTransform->SetParent(parentTransform);
						childEntity->SetFolderPath(
							parentEntity->GetFolderPath()
						);
						scene->AddFolder(parentEntity->GetFolderPath());
					}
				}
			}
			if (pendingDeleteEntityId != 0) {
				if (mSelectedEntityId == pendingDeleteEntityId) {
					mSelectedEntityId = 0;
				}
				scene->DestroyEntity(pendingDeleteEntityId);
			}
			if (!pendingDeleteFolderPath.empty()) {
				scene->DeleteFolderSubtree(pendingDeleteFolderPath);
			}
		}

		if (ImGui::BeginPopup("OutlinerRenamePopup")) {
			ImGui::InputText(
				"##Rename", renameBuffer.data(), renameBuffer.size()
			);
			if (ImGui::Button("Apply")) {
				if (renameEntityId != 0) {
					if (Entity* entity = scene->FindEntity(renameEntityId)) {
						entity->SetName(renameBuffer.data());
					}
				} else if (!renameFolderPath.empty()) {
					scene->RenameFolderSubtree(
						renameFolderPath, renameBuffer.data()
					);
				}
				renameEntityId = 0;
				renameFolderPath.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				renameEntityId = 0;
				renameFolderPath.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		ImGui::End();
	}

	void LevelEditorTool::DrawContentBrowser() {
		EditorContentBrowser::DrawWindow(mContentBrowserState, "Content Browser");
	}

	void LevelEditorTool::DrawInspector() {
		if (!ImGui::Begin("Inspector")) {
			ImGui::End();
			return;
		}

		Entity* entity = GetSelectedEntity();
		if (!entity) {
			ImGui::TextUnformatted("No selected entity.");
			ImGui::End();
			return;
		}

		std::unordered_set<std::string> existingStableNames;
		entity->ForEachComponent(
			[&](const BaseComponent& component) {
				existingStableNames.emplace(component.GetStableName());
			}
		);

		if (
			ImGuiWidgets::IconButton(
				kIconAdd, "AddComponent", ImVec2(0.0f, 0.0f), 1.5f,
				ImGuiDir_Right
			)
		) {
			ImGui::OpenPopup("InspectorAddComponentPopup");
		}

		ImGui::PushStyleVar(
			ImGuiStyleVar_WindowPadding,
			ImVec2(kPopupPadding, kPopupPadding)
		);

		if (ImGui::BeginPopup("InspectorAddComponentPopup")) {
			ComponentMenuNode addComponentRoot    = {};
			size_t            validComponentCount = 0;

			for (
				const auto& info :
				ComponentRegistry::Get().ListRegisteredComponents()
			) {
				if (InsertComponentMenuEntry(addComponentRoot, info)) {
					++validComponentCount;
				}
			}
			SortComponentMenuTree(addComponentRoot);

			if (validComponentCount == 0) {
				// なかなかないけど一応
				ImGui::TextUnformatted("コンポーネントが登録されていません。");
			} else if (
				DrawComponentAddMenuRecursive(
					addComponentRoot, *entity, existingStableNames
				)
			) {
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		ImGui::PopStyleVar();

		ImGui::Separator();

		// すべてのコンポーネントのインスペクタUIを描画
		entity->ForEachComponent(
			[&](BaseComponent& component) {
				bool       componentActive = component.IsActive();
				const bool open            =
					ImGuiUtil::CollapsingHeaderWithCheckbox(
						component.GetIcon(),
						component.GetComponentName().data(),
						component.GetGuid(),
						&componentActive,
						nullptr,
						ImGuiTreeNodeFlags_DefaultOpen
					);
				if (componentActive != component.IsActive()) {
					component.SetActive(componentActive);
				}
				if (open) {
					component.DrawInspectorImGui();
				}
				ImGui::Separator();
			}
		);

		ImGui::End();
	}
}

#endif

