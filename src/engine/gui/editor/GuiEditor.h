#pragma once
#include "engine/gui/UiDocumentManager.h"
#include "engine/gui/UiScreenStack.h"

#ifdef _DEBUG
namespace Unnamed::Gui {
	class UiWidget;
	class UiRoot;

	void DrawWidgetTreeNode(UiWidget* widget);
	void DrawUiHierarchyWindow(const UiRoot& uiRoot);
	void DrawRectInspector(UiWidget* widget);
	void DrawAnchorsInspector(UiWidget* widget);
	void DrawPivotInspector(UiWidget* widget);
	void DrawMarginsInspector(UiWidget* widget);
	void DrawBasicFlagsInspector(UiWidget* widget);
	void DrawNameInspector(UiWidget* widget);
	void DrawSizePolicyInspector(UiWidget* widget);
	void DrawConstraintsInspector(UiWidget* widget);
	void DrawUiInspectorWindow();
	void DrawUiEditorMenu(
		UiDocumentManager&           manager,
		std::shared_ptr<UiDocument>& activeDocument,
		UiScreenStack*               screenStack
	);
	void DrawAddChildControls(UiWidget* parent);
}

namespace {
	Unnamed::Gui::UiWidget* gSelectedWidget = nullptr;
}
#endif
