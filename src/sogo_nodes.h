#pragma once

#include "sogo.h"

namespace sogo {
    extern void SplitNodeDesc(const GraphRuntimeSettings* graph_runtime_settings, NodeDesc* out_node_desc);
    extern void MergeNodeDesc(const GraphRuntimeSettings* graph_runtime_settings, NodeDesc* out_node_desc);
    extern void GainNodeDesc(const GraphRuntimeSettings* graph_runtime_settings, NodeDesc* out_node_desc);
    extern void SineNodeDesc(const GraphRuntimeSettings* graph_runtime_settings, NodeDesc* out_node_desc);
    extern void ToStereoNodeDesc(const GraphRuntimeSettings* graph_runtime_settings, NodeDesc* out_node_desc);
    extern void DCNodeDesc(const GraphRuntimeSettings* graph_runtime_settings, NodeDesc* out_node_desc);
    extern void SequentialTriggerDesc(const GraphRuntimeSettings* graph_runtime_settings, NodeDesc* out_node_desc);
}
