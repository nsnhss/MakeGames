#include "layer_stack.h"

namespace mg {

LayerStack::~LayerStack()
{
    DetachAll();
}

void LayerStack::DetachAll()
{
    // 按反序 Detach
    for (auto it = ordered_.rbegin(); it != ordered_.rend(); ++it)
        (*it)->OnDetach();
    layers_.clear();
    overlays_.clear();
    ordered_.clear();
}

void LayerStack::PushLayer(std::unique_ptr<Layer> layer)
{
    layer->OnAttach();
    layers_.push_back(std::move(layer));
    RebuildOrder();
}

void LayerStack::PushOverlay(std::unique_ptr<Layer> overlay)
{
    overlay->OnAttach();
    overlays_.push_back(std::move(overlay));
    RebuildOrder();
}

void LayerStack::PopLayer(const std::string& name)
{
    for (auto it = layers_.begin(); it != layers_.end(); ++it) {
        if ((*it)->GetName() == name) {
            (*it)->OnDetach();
            layers_.erase(it);
            RebuildOrder();
            return;
        }
    }
}

void LayerStack::PopOverlay(const std::string& name)
{
    for (auto it = overlays_.begin(); it != overlays_.end(); ++it) {
        if ((*it)->GetName() == name) {
            (*it)->OnDetach();
            overlays_.erase(it);
            RebuildOrder();
            return;
        }
    }
}

void LayerStack::RebuildOrder()
{
    ordered_.clear();
    ordered_.reserve(layers_.size() + overlays_.size());
    for (auto& l : layers_) ordered_.push_back(l.get());
    for (auto& o : overlays_) ordered_.push_back(o.get());
}

} // namespace mg
