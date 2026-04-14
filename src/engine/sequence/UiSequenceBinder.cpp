#include "UiSequenceBinder.h"

#include "engine/gui/UiRoot.h"
#include "engine/gui/UiWidget.h"
#include "engine/gui/components/UiLayoutComponents.h"
#include "engine/gui/components/UiPanelStyleComponent.h"
#include "engine/gui/components/UiTransformComponent.h"
#include "engine/scene/Scene.h"
#include "engine/unnamed/framework/components/ui/UiCanvasComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"

namespace Unnamed {
	UiSequenceBinder::UiSequenceBinder(Scene* scene)
		: mScene(scene) {}

	void UiSequenceBinder::SetScene(Scene* scene) {
		mScene = scene;
	}

	Scene* UiSequenceBinder::GetScene() const {
		return mScene;
	}

	bool UiSequenceBinder::SupportsTarget(
		const SEQUENCE_BINDING_TARGET target
	) const {
		return target == SEQUENCE_BINDING_TARGET::UI;
	}

	bool UiSequenceBinder::ApplyValue(
		const SequenceBinding& binding,
		const float            value
	) {
		if (
			binding.target != SEQUENCE_BINDING_TARGET::UI ||
			!mScene ||
			binding.ui.canvasEntityGuid == 0
		) {
			return false;
		}

		Entity* canvasEntity = mScene->FindEntity(binding.ui.canvasEntityGuid);
		if (!canvasEntity) {
			return false;
		}

		auto* canvas = canvasEntity->GetComponent<UiCanvasComponent>();
		if (!canvas || !canvas->EnsureRuntimeLoaded()) {
			return false;
		}

		Gui::UiRoot* runtimeRoot = canvas->GetRuntimeRoot();
		if (!runtimeRoot) {
			return false;
		}

		Gui::UiWidget* widget = runtimeRoot->GetRootWidget();
		if (!widget) {
			return false;
		}

		if (!binding.ui.widgetName.empty()) {
			widget = FindWidgetByNameRecursive(widget, binding.ui.widgetName);
		}
		if (!widget) {
			return false;
		}

		Gui::UiComponent* component = widget->GetComponentByTypeName(
			binding.ui.componentType
		);
		if (!component) {
			return false;
		}

		bool applied = false;
		if (binding.ui.componentType == "Transform") {
			if (auto* transform = dynamic_cast<Gui::UiTransformComponent*>(component))
			{
				applied = ApplyTransformProperty(
					*transform,
					binding.ui.propertyPath,
					value
				);
			}
		} else if (
			binding.ui.componentType == "VerticalLayout" ||
			binding.ui.componentType == "HorizontalLayout"
		) {
			if (auto* layout = dynamic_cast<Gui::UiLinearLayoutComponent*>(component))
			{
				applied = ApplyLayoutProperty(
					*layout,
					binding.ui.propertyPath,
					value
				);
			}
		} else if (binding.ui.componentType == "PanelStyle") {
			if (auto* panelStyle = dynamic_cast<Gui::UiPanelStyleComponent*>(component))
			{
				applied = ApplyPanelStyleProperty(
					*panelStyle,
					binding.ui.propertyPath,
					value
				);
			}
		}

		if (applied) {
			widget->MarkDirty(Gui::DIRTY_FLAGS::LAYOUT | Gui::DIRTY_FLAGS::DRAW);
		}
		return applied;
	}

	Gui::UiWidget* UiSequenceBinder::FindWidgetByNameRecursive(
		Gui::UiWidget*       root,
		const std::string_view widgetName
	) const {
		if (!root) {
			return nullptr;
		}
		if (root->GetName() == widgetName) {
			return root;
		}

		for (const auto& child : root->GetChildren()) {
			if (!child) {
				continue;
			}
			if (Gui::UiWidget* found = FindWidgetByNameRecursive(
				child.get(),
				widgetName
			)) {
				return found;
			}
		}

		for (Gui::UiWidget* child : root->GetReferenceChildren()) {
			if (Gui::UiWidget* found = FindWidgetByNameRecursive(child, widgetName)) {
				return found;
			}
		}

		return nullptr;
	}

	bool UiSequenceBinder::ApplyTransformProperty(
		Gui::UiTransformComponent& component,
		const std::string_view     propertyPath,
		const float                value
	) const {
		if (propertyPath == "rect.x") {
			Gui::Rect rect = component.GetRect();
			rect.x = value;
			component.SetRect(rect);
			return true;
		}
		if (propertyPath == "rect.y") {
			Gui::Rect rect = component.GetRect();
			rect.y = value;
			component.SetRect(rect);
			return true;
		}
		if (propertyPath == "rect.width") {
			Gui::Rect rect = component.GetRect();
			rect.width = value;
			component.SetRect(rect);
			return true;
		}
		if (propertyPath == "rect.height") {
			Gui::Rect rect = component.GetRect();
			rect.height = value;
			component.SetRect(rect);
			return true;
		}

		if (propertyPath == "anchors.minX") {
			Gui::Anchors anchors = component.GetAnchors();
			anchors.minX = value;
			component.SetAnchors(anchors);
			return true;
		}
		if (propertyPath == "anchors.minY") {
			Gui::Anchors anchors = component.GetAnchors();
			anchors.minY = value;
			component.SetAnchors(anchors);
			return true;
		}
		if (propertyPath == "anchors.maxX") {
			Gui::Anchors anchors = component.GetAnchors();
			anchors.maxX = value;
			component.SetAnchors(anchors);
			return true;
		}
		if (propertyPath == "anchors.maxY") {
			Gui::Anchors anchors = component.GetAnchors();
			anchors.maxY = value;
			component.SetAnchors(anchors);
			return true;
		}

		if (propertyPath == "margins.left") {
			Gui::Margins margins = component.GetMargins();
			margins.left = value;
			component.SetMargins(margins);
			return true;
		}
		if (propertyPath == "margins.top") {
			Gui::Margins margins = component.GetMargins();
			margins.top = value;
			component.SetMargins(margins);
			return true;
		}
		if (propertyPath == "margins.right") {
			Gui::Margins margins = component.GetMargins();
			margins.right = value;
			component.SetMargins(margins);
			return true;
		}
		if (propertyPath == "margins.bottom") {
			Gui::Margins margins = component.GetMargins();
			margins.bottom = value;
			component.SetMargins(margins);
			return true;
		}

		if (propertyPath == "pivot.x") {
			Gui::Pivot pivot = component.GetPivot();
			pivot.x = value;
			component.SetPivot(pivot);
			return true;
		}
		if (propertyPath == "pivot.y") {
			Gui::Pivot pivot = component.GetPivot();
			pivot.y = value;
			component.SetPivot(pivot);
			return true;
		}

		return false;
	}

	bool UiSequenceBinder::ApplyLayoutProperty(
		Gui::UiLinearLayoutComponent& component,
		const std::string_view        propertyPath,
		const float                   value
	) const {
		if (propertyPath == "spacing") {
			component.SetSpacing(value);
			return true;
		}

		if (propertyPath == "padding.left") {
			Gui::LayoutPadding padding = component.GetPadding();
			padding.left = value;
			component.SetPadding(padding);
			return true;
		}
		if (propertyPath == "padding.top") {
			Gui::LayoutPadding padding = component.GetPadding();
			padding.top = value;
			component.SetPadding(padding);
			return true;
		}
		if (propertyPath == "padding.right") {
			Gui::LayoutPadding padding = component.GetPadding();
			padding.right = value;
			component.SetPadding(padding);
			return true;
		}
		if (propertyPath == "padding.bottom") {
			Gui::LayoutPadding padding = component.GetPadding();
			padding.bottom = value;
			component.SetPadding(padding);
			return true;
		}

		return false;
	}

	bool UiSequenceBinder::ApplyPanelStyleProperty(
		Gui::UiPanelStyleComponent& component,
		const std::string_view      propertyPath,
		const float                 value
	) const {
		if (propertyPath == "fillColor.r") {
			Gui::Color color = component.GetBackgroundColor();
			color.r = value;
			component.SetBackgroundColor(color);
			return true;
		}
		if (propertyPath == "fillColor.g") {
			Gui::Color color = component.GetBackgroundColor();
			color.g = value;
			component.SetBackgroundColor(color);
			return true;
		}
		if (propertyPath == "fillColor.b") {
			Gui::Color color = component.GetBackgroundColor();
			color.b = value;
			component.SetBackgroundColor(color);
			return true;
		}
		if (propertyPath == "fillColor.a") {
			Gui::Color color = component.GetBackgroundColor();
			color.a = value;
			component.SetBackgroundColor(color);
			return true;
		}

		if (propertyPath == "borderColor.r") {
			Gui::Color color = component.GetBorderColor();
			color.r = value;
			component.SetBorderColor(color);
			return true;
		}
		if (propertyPath == "borderColor.g") {
			Gui::Color color = component.GetBorderColor();
			color.g = value;
			component.SetBorderColor(color);
			return true;
		}
		if (propertyPath == "borderColor.b") {
			Gui::Color color = component.GetBorderColor();
			color.b = value;
			component.SetBorderColor(color);
			return true;
		}
		if (propertyPath == "borderColor.a") {
			Gui::Color color = component.GetBorderColor();
			color.a = value;
			component.SetBorderColor(color);
			return true;
		}

		if (propertyPath == "cornerRadius") {
			component.SetCornerRadius(value);
			return true;
		}
		if (propertyPath == "borderThickness") {
			component.SetBorderThickness(value);
			return true;
		}

		return false;
	}
}
