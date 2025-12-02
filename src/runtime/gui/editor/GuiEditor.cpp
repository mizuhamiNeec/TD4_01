#include "GuiEditor.h"

#include "engine/subsystem/console/Log.h"

#include "runtime/gui/UiButton.h"
#include "runtime/gui/UiPanel.h"
#include "runtime/gui/UiRoot.h"
#include "runtime/gui/UiScreenStack.h"
#include "runtime/gui/UiWidget.h"
#include "runtime/gui/layout/UiHorizontalLayout.h"
#include "runtime/gui/layout/UiVerticalLayout.h"

#ifdef _DEBUG
namespace Unnamed::Gui {
	void DrawWidgetTreeNode(UiWidget* widget) {
		if (!widget) return;

		const bool isSelected = (widget == gSelectedWidget);

		ImGuiTreeNodeFlags flags =
			ImGuiTreeNodeFlags_OpenOnArrow |
			ImGuiTreeNodeFlags_SpanAvailWidth;
		
		if (widget->GetChildren().empty() && widget->GetReferenceChildren().empty()) {
			flags |= ImGuiTreeNodeFlags_Leaf |
				ImGuiTreeNodeFlags_NoTreePushOnOpen;
		}
		if (isSelected) {
			flags |= ImGuiTreeNodeFlags_Selected;
		}

		std::string label;
		if (widget->GetName().empty()) {
			label = "(unnamed)";
		} else {
			label = std::string(widget->GetName());
		}

		const bool open = ImGui::TreeNodeEx(
			widget, flags, "%s", label.c_str()
		);

		if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
			gSelectedWidget = widget;
		}

		// 子を描画
		if (open) {
			// 所有している子を描画
			if (!widget->GetChildren().empty()) {
				for (const auto& child : widget->GetChildren()) {
					if (child) {
						DrawWidgetTreeNode(child.get());
					}
				}
			}

			// 参照している子も描画
			if (!widget->GetReferenceChildren().empty()) {
				for (auto* child : widget->GetReferenceChildren()) {
					if (child) {
						DrawWidgetTreeNode(child);
					}
				}
			}

			if (!(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen)) {
				ImGui::TreePop();
			}
		}
	}

	void DrawUiHierarchyWindow(const UiRoot& uiRoot) {
		if (!ImGui::Begin("Ui Outliner")) {
			ImGui::End();
			return;
		}

		UiWidget* rootWidget = uiRoot.GetRootWidget();
		if (rootWidget) {
			DrawWidgetTreeNode(rootWidget);
		} else {
			ImGui::TextUnformatted("No root widget.");
		}

		ImGui::End();
	}

	void DrawRectInspector(UiWidget* widget) {
		Rect  rect         = widget->GetLocalRect();
		float rectArray[4] = {
			rect.x, rect.y,
			rect.width, rect.height
		};

		if (ImGui::DragFloat4("Local Rect (x,y,w,h)", rectArray)) {
			rect.x      = rectArray[0];
			rect.y      = rectArray[1];
			rect.width  = rectArray[2];
			rect.height = rectArray[3];
			widget->SetLocalRect(rect);
		}
	}

	void DrawAnchorsInspector(UiWidget* widget) {
		Anchors anchors        = widget->GetAnchors();
		float   anchorArray[4] = {
			anchors.minX, anchors.minY, anchors.maxX, anchors.maxY
		};

		if (ImGui::SliderFloat4("Anchors (minX,minY,maxX,maxY)", anchorArray,
		                        0.0f, 1.0f)) {
			anchors.minX = anchorArray[0];
			anchors.minY = anchorArray[1];
			anchors.maxX = anchorArray[2];
			anchors.maxY = anchorArray[3];
			widget->SetAnchors(anchors);
		}
	}

	void DrawPivotInspector(UiWidget* widget) {
		Pivot pivot         = widget->GetPivot();
		float pivotArray[2] = {pivot.x, pivot.y};

		if (ImGui::SliderFloat2("Pivot (x, y)", pivotArray, 0.0f, 1.0f)) {
			pivot.x = pivotArray[0];
			pivot.y = pivotArray[1];
			widget->SetPivot(pivot);
		}
	}

	void DrawMarginsInspector(UiWidget* widget) {
		Margins margins        = widget->GetMargins();
		float   marginArray[4] = {
			margins.left, margins.top, margins.right, margins.bottom
		};

		if (
			ImGui::DragFloat4("Margins (L, T, R, B)", marginArray)
		) {
			margins.left   = marginArray[0];
			margins.top    = marginArray[1];
			margins.right  = marginArray[2];
			margins.bottom = marginArray[3];
			widget->SetMargins(margins);
		}
	}

	void DrawBasicFlagsInspector(UiWidget* widget) {
		bool visible = widget->IsVisible();
		if (ImGui::Checkbox("Visible", &visible)) {
			widget->SetVisible(visible);
		}

		bool enabled = widget->IsEnabled();
		if (ImGui::Checkbox("Enabled", &enabled)) {
			widget->SetEnabled(enabled);
		}
	}

	void DrawNameInspector(UiWidget* widget) {
		std::string name = std::string(widget->GetName());
		char        buffer[128]{};
		std::snprintf(buffer, sizeof(buffer), "%s", name.c_str());

		if (ImGui::InputText("Name", buffer, sizeof(buffer))) {
			widget->SetName(buffer);
		}
	}

	void DrawSizePolicyInspector(UiWidget* widget) {
		auto        sizePolicy = widget->GetSizePolicy();
		const char* policies[] = {
			"FIXED",
			"EXPAND"
		};
		int horizontalPolicy = static_cast<int>(sizePolicy.horizontal);
		int verticalPolicy   = static_cast<int>(sizePolicy.vertical);
		if (ImGui::Combo("Horizontal Size Policy", &horizontalPolicy,
		                 policies, IM_ARRAYSIZE(policies))) {
			sizePolicy.horizontal =
				static_cast<UiSizePolicyAxis>(horizontalPolicy);
			widget->SetSizePolicy(
				sizePolicy.horizontal,
				sizePolicy.vertical
			);
		}
		if (ImGui::Combo("Vertical Size Policy", &verticalPolicy,
		                 policies, IM_ARRAYSIZE(policies))) {
			sizePolicy.vertical =
				static_cast<UiSizePolicyAxis>(verticalPolicy);
			widget->SetSizePolicy(
				sizePolicy.horizontal,
				sizePolicy.vertical
			);
		}
	}

	void DrawConstraintsInspector(UiWidget* widget) {
		auto  constraints        = widget->GetSizeConstraints();
		float constraintArray[4] = {
			constraints.minWidth, constraints.minHeight,
			constraints.maxWidth, constraints.maxHeight
		};

		if (ImGui::DragFloat4("Size Constraints (minW,minH,maxW,maxH)",
		                      constraintArray)) {
			constraints.minWidth  = constraintArray[0];
			constraints.minHeight = constraintArray[1];
			constraints.maxWidth  = constraintArray[2];
			constraints.maxHeight = constraintArray[3];
			widget->SetSizeConstraints(constraints);
		}
	}

	void DrawUiInspectorWindow() {
		if (!ImGui::Begin("Ui Inspector")) {
			ImGui::End();
			return;
		}

		if (!gSelectedWidget) {
			ImGui::TextUnformatted("No widget selected.");
			ImGui::End();
			return;
		}

		DrawNameInspector(gSelectedWidget);

		ImGui::Separator();
		DrawBasicFlagsInspector(gSelectedWidget);
		ImGui::Separator();

		DrawRectInspector(gSelectedWidget);
		DrawAnchorsInspector(gSelectedWidget);
		DrawPivotInspector(gSelectedWidget);
		DrawMarginsInspector(gSelectedWidget);
		DrawSizePolicyInspector(gSelectedWidget);
		DrawConstraintsInspector(gSelectedWidget);

		DrawAddChildControls(gSelectedWidget);

		ImGui::End();
	}

	void DrawUiEditorMenu(
		UiDocumentManager&           manager,
		std::shared_ptr<UiDocument>& activeDocument,
		UiScreenStack*               screenStack
	) {
		ImGui::Begin("UI Document");

		static char pathBuffer[256] = "./content/parkour/ui/MainMenu.ui.json";
		ImGui::InputText("Path", pathBuffer, sizeof(pathBuffer));

		// Save
		if (ImGui::Button("Save Document")) {
			if (activeDocument) {
				// UiRootからrootWidgetを取り戻してDocumentに設定
				std::unique_ptr<UiWidget> tempRoot;
				bool needsRestore = false;
				
				if (screenStack && screenStack->GetUiRoot()) {
					UiWidget* uiRootWidget = screenStack->GetUiRoot()->GetRootWidget();
					// UiRootの最初の子がactiveDocumentのroot
					if (uiRootWidget && !uiRootWidget->GetChildren().empty()) {
						UiWidget* firstChild = uiRootWidget->GetChildren()[0].get();
						if (firstChild) {
							// 子を取り出してDocumentに設定
							tempRoot = screenStack->GetUiRoot()->TakeChild(firstChild);
							if (tempRoot) {
								activeDocument->SetRootWidget(std::move(tempRoot));
								needsRestore = true;
							}
						}
					}
				}
				
				// 保存実行
				bool saveSuccess = activeDocument->Save(pathBuffer);
				
				// rootWidgetをUiRootに戻す
				if (needsRestore) {
					tempRoot = activeDocument->TakeRootWidget();
					if (tempRoot && screenStack && screenStack->GetUiRoot()) {
						screenStack->GetUiRoot()->AddChild(std::move(tempRoot));
					}
				}
				
				if (saveSuccess) {
					Msg(kChannelNone, "Document saved: {}", pathBuffer);
				} else {
					Error(kChannelNone, "Failed to save document: {}", pathBuffer);
				}
			} else {
				Warning(kChannelNone, "No active document to save");
			}
		}

		// Load
		if (ImGui::Button("Load Document")) {
			auto doc = manager.LoadDocument(pathBuffer);
			if (doc) {
				Msg(kChannelNone, "Document loaded: {}", pathBuffer);
				activeDocument = doc;

				// 選択状態をクリア
				gSelectedWidget = nullptr;
				
				if (screenStack) {
					screenStack->Clear();
					auto screen = std::make_shared<UiScreen>(doc);
					screenStack->PushScreen(screen);
				}
			} else {
				Error(kChannelNone, "Failed to load document: {}", pathBuffer);
			}
		}

		// Document が空なら Root 追加ボタンを出す
		if (activeDocument && !activeDocument->GetRootWidget()) {
			ImGui::Separator();
			ImGui::TextUnformatted("No root widget in document.");
			if (ImGui::Button("Create Root VerticalLayout")) {
				auto root = std::make_unique<UiVerticalLayout>();
				root->SetName("RootLayout");
				root->SetAnchors({0.0f, 0.0f, 1.0f, 1.0f});
				root->SetPivot({0.0f, 0.0f});
				root->SetLocalRect({0.0f, 0.0f, 0.0f, 0.0f});

				activeDocument->SetRootWidget(std::move(root));

				// ランタイムの画面にも適用
				if (screenStack) {
					screenStack->Clear();
					auto screen = std::make_shared<UiScreen>(activeDocument);
					screenStack->PushScreen(screen);
				}
			}
		}

		ImGui::End();
	}

	void DrawAddChildControls(UiWidget* parent) {
		ImGui::Separator();
		ImGui::TextUnformatted("Add Child");

		if (!parent) {
			ImGui::TextUnformatted("No parent selected.");
			return;
		}

		// Panel
		if (ImGui::Button("Add Panel")) {
			auto panel = std::make_unique<UiPanel>();
			panel->SetName("Panel");
			panel->SetLocalRect({0.0f, 0.0f, 200.0f, 100.0f});
			panel->SetAnchors({0.0f, 0.0f, 0.0f, 0.0f});
			panel->SetPivot({0.0f, 0.0f});
			parent->AddChild(std::move(panel));
		}
		
		ImGui::SameLine();

		// Button
		if (ImGui::Button("Add Button")) {
			auto button = std::make_unique<UiButton>();
			button->SetName("Button");
			button->SetText("Button");
			button->SetLocalRect({0.0f, 0.0f, 160.0f, 40.0f});
			button->SetAnchors({0.0f, 0.0f, 0.0f, 0.0f});
			button->SetPivot({0.0f, 0.0f});
			parent->AddChild(std::move(button));
		}
		
		// Layout 系
		if (ImGui::Button("Add Vertical Layout")) {
			auto layout = std::make_unique<UiVerticalLayout>();
			layout->SetName("VerticalLayout");
			layout->SetPadding({8.0f, 8.0f, 8.0f, 8.0f});
			layout->SetSpacing(4.0f);
			layout->SetAnchors({0.0f, 0.0f, 1.0f, 1.0f});
			layout->SetPivot({0.0f, 0.0f});
			layout->SetLocalRect({0.0f, 0.0f, 0.0f, 0.0f});
			parent->AddChild(std::move(layout));
		}

		ImGui::SameLine();
		if (ImGui::Button("Add Horizontal Layout")) {
			auto layout = std::make_unique<UiHorizontalLayout>();
			layout->SetName("HorizontalLayout");
			layout->SetPadding({8.0f, 8.0f, 8.0f, 8.0f});
			layout->SetSpacing(4.0f);
			layout->SetAnchors({0.0f, 0.0f, 1.0f, 1.0f});
			layout->SetPivot({0.0f, 0.0f});
			layout->SetLocalRect({0.0f, 0.0f, 0.0f, 0.0f});
			parent->AddChild(std::move(layout));
		}
	}
}
#endif