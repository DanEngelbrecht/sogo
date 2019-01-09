#include "sogo.h"

#include <string.h>

#define ALIGN_SIZE(x, align)    (((x) + ((align) - 1)) & ~((align) - 1))

namespace sogo
{

typedef uint16_t TParameterOffset;
typedef uint16_t TTriggerOffset;
typedef uint16_t TResourceOffset;
typedef uint16_t TAudioInputOffset;
typedef uint16_t TAudioOutputOffset;
typedef uint32_t TContextMemoryOffset;

struct Node
{
    RenderNodeCallback      m_Render;
    TParameterOffset        m_ParametersOffset;
    TAudioInputOffset       m_AudioInputsOffset;
    TAudioOutputOffset      m_AudioOutputsOffset;
    TResourceOffset         m_ResourcesOffset;
    TTriggerOffset          m_TriggerOffset;
    TContextMemoryOffset    m_ContextMemoryOffset;
};

struct Graph {
    float* m_Parameters;
    Resource* m_Resources;
    TTriggerCount m_MaxTriggerEventCount;
    TTriggerIndex* m_Triggers;
    TNodeIndex m_NodeCount;
    Node* m_Nodes;
    uint32_t m_ScratchBufferSize;
    float* m_ScratchBuffer;
    uint32_t m_ScratchUsedCount;
    uint8_t* m_ContextMemory;
    TFrameRate m_FrameRate;
    AudioOutput* m_AudioOutputs;
    AudioInput* m_AudioInputs;
    TriggerInput* m_TriggerInputs;
};

static bool GetOutputChannelCount(
    const GraphDescription* graph_description,
    TNodeIndex node_index,
    TAudioOutputIndex output_index,
    TChannelIndex* out_channel_count)
{
    const NodeDescription* node_description = graph_description->m_NodeDescriptions[node_index];
    const AudioOutputDescription* output_description = &node_description->m_AudioOutputDescriptions[output_index];
    switch (output_description->m_Mode)
    {
        case AudioOutputDescription::FIXED:
            *out_channel_count = output_description->m_ChannelCount;
            return true;
        case AudioOutputDescription::PASS_THROUGH:
        case AudioOutputDescription::AS_INPUT:
        {
            for (TConnectionIndex i = 0; i < graph_description->m_ConnectionCount; ++i)
            {
                const NodeConnection* node_connection = &graph_description->m_NodeConnections[i];
                if (node_connection->m_InputNodeIndex == node_index &&
                    node_connection->m_InputIndex == output_description->m_InputIndex)
                {
                    if (node_connection->m_OutputNodeIndex == EXTERNAL_NODE_INDEX)
                    {
                        *out_channel_count = graph_description->m_ExternalAudioInputs[node_connection->m_OutputIndex]->m_ChannelCount;
                        return true;
                    }
                    return GetOutputChannelCount(
                        graph_description,
                        node_connection->m_OutputNodeIndex,
                        node_connection->m_OutputIndex,
                        out_channel_count);
                }
            }
            return false;
        }
        default:
            return false;
    }
}

static bool GetOutputChannelAllocationCount(
    const GraphDescription* graph_description,
    TNodeIndex node_index,
    TAudioOutputIndex output_index,
    TChannelIndex* out_channel_count)
{
    const NodeDescription* node_description = graph_description->m_NodeDescriptions[node_index];
    const AudioOutputDescription* output_description = &node_description->m_AudioOutputDescriptions[output_index];
    switch (output_description->m_Mode)
    {
        case AudioOutputDescription::PASS_THROUGH:
            *out_channel_count = 0;
            return true;
        case AudioOutputDescription::FIXED:
            *out_channel_count = output_description->m_ChannelCount;
            return true;
        case AudioOutputDescription::AS_INPUT:
        {
            for (TConnectionIndex i = 0; i < graph_description->m_ConnectionCount; ++i)
            {
                const NodeConnection* node_connection = &graph_description->m_NodeConnections[i];
                if (node_connection->m_InputNodeIndex == node_index &&
                    node_connection->m_InputIndex == output_description->m_InputIndex)
                {
                    return GetOutputChannelCount(
                        graph_description,
                        node_connection->m_OutputNodeIndex,
                        node_connection->m_OutputIndex,
                        out_channel_count);
                }
            }
            return false;
        }
        default:
            return false;
    }
}

struct GraphProperties {
    TParameterIndex m_ParameterCount;
    TResourceIndex m_ResourceCount;
    TAudioInputIndex m_AudioInputCount;
    TAudioOutputIndex m_AudioOutputCount;
    uint32_t m_GeneratedAudioBufferCount;
    TTriggerCount m_TriggerInputCount;
    TContextMemorySize m_ContextMemorySize;
};

static bool GetGraphProperties(
    const GraphDescription* graph_description,
    const GraphRuntimeSettings* graph_runtime_settings,
    GraphProperties* graph_properties)
{
    TParameterIndex parameter_count = 0;
    TResourceIndex resource_count = 0;
    TTriggerCount trigger_input_count = 0;
    TAudioInputIndex audio_input_count = 0;
    TAudioOutputIndex audio_output_count = 0;
    uint32_t generated_buffer_count = 0;
    TContextMemorySize context_memory_size = 0;

    for (TNodeIndex i = 0; i < graph_description->m_NodeCount; ++i)
    {
        const NodeDescription* node_description = graph_description->m_NodeDescriptions[i];
        NodeProperties node_properties;
        node_properties.m_ContextMemorySize = 0;
        if (node_description->m_GetNodePropertiesCallback != 0x0)
        {
            node_description->m_GetNodePropertiesCallback(graph_runtime_settings, &node_properties);
        }
        context_memory_size += node_properties.m_ContextMemorySize;
        parameter_count += node_description->m_ParameterCount;
        resource_count += node_description->m_ResourceCount;
        audio_input_count += node_description->m_AudioInputCount;
        audio_output_count += node_description->m_AudioOutputCount;
        trigger_input_count += node_description->m_TriggerCount > 0 ? 1 : 0;
        for (TAudioOutputIndex j = 0; j < node_description->m_AudioOutputCount; ++j)
        {
            TAudioOutputIndex buffer_count = 0;
            if (!GetOutputChannelAllocationCount(graph_description, i, j, &buffer_count))
            {
                return false;
            }
            generated_buffer_count += buffer_count;
        }
    }

    for (TConnectionIndex c = 0; c < graph_description->m_ConnectionCount; ++c)
    {
        const NodeConnection* connection = &graph_description->m_NodeConnections[c];
        if (connection->m_InputNodeIndex <= connection->m_OutputNodeIndex)
        {
            return false;
        }
    }

    graph_properties->m_ContextMemorySize = context_memory_size;
    graph_properties->m_ParameterCount = parameter_count;
    graph_properties->m_ResourceCount = resource_count;
    graph_properties->m_AudioInputCount = audio_input_count;
    graph_properties->m_AudioOutputCount = audio_output_count;
    graph_properties->m_GeneratedAudioBufferCount = generated_buffer_count;
    graph_properties->m_TriggerInputCount = trigger_input_count;
    return true;
}

static TGraphSize GetGraphSize(
    const GraphDescription* graph_description,
    const GraphProperties* graph_properties)
{
    TGraphSize s = ALIGN_SIZE(sizeof(Graph), sizeof(void*)) +
        ALIGN_SIZE(sizeof(float) * graph_properties->m_ParameterCount, sizeof(Resource*)) +
        ALIGN_SIZE(sizeof(Resource) * graph_properties->m_ResourceCount, sizeof(RenderNodeCallback)) +
        ALIGN_SIZE(sizeof(Node) * graph_description->m_NodeCount, sizeof(float*)) +
        ALIGN_SIZE(sizeof(AudioOutput) * (graph_properties->m_AudioOutputCount + 1), sizeof(AudioOutput*)) +
        ALIGN_SIZE(sizeof(AudioInput) * graph_properties->m_AudioInputCount, sizeof(TTriggerIndex*)) +
        ALIGN_SIZE(sizeof(TriggerInput) * graph_properties->m_TriggerInputCount, 1);
    return s;
}

bool GetGraphSize(
    const GraphDescription* graph_description,
    const GraphRuntimeSettings* graph_runtime_settings,
    GraphSize* out_graph_size)
{
    GraphProperties graph_properties;
    if (!GetGraphProperties(graph_description, graph_runtime_settings, &graph_properties))
    {
        return false;
    }
    out_graph_size->m_GraphSize = GetGraphSize(graph_description, &graph_properties);
    out_graph_size->m_TriggerBufferSize = graph_properties.m_TriggerInputCount * graph_runtime_settings->m_MaxTriggerEventCount * sizeof(TTriggerIndex);
    out_graph_size->m_ScratchBufferSize = graph_properties.m_GeneratedAudioBufferCount * graph_runtime_settings->m_MaxBatchSize * sizeof(float);
    out_graph_size->m_ContextMemorySize = graph_properties.m_ContextMemorySize;
    return true;
}

static float* AllocateAudioBuffer(HGraph graph, HNode , TChannelIndex channel_count, TFrameIndex frame_count)
{
    uint32_t samples_required = (uint32_t)channel_count * (uint32_t)frame_count;
    uint32_t samples_left = graph->m_ScratchBufferSize - graph->m_ScratchUsedCount;
    if (samples_left >= samples_required)
    {
        float* scratch = &graph->m_ScratchBuffer[graph->m_ScratchUsedCount];
        graph->m_ScratchUsedCount += samples_required;
        return scratch;
    }
    return 0x0;
}

void RenderGraph(HGraph graph, TFrameIndex frame_count)
{
    graph->m_ScratchUsedCount = 0;
    Node* nodes = graph->m_Nodes;
    TNodeIndex node_count = graph->m_NodeCount;

    RenderParameters render_parameters;
    render_parameters.m_AllocateAudioBuffer = AllocateAudioBuffer;
    render_parameters.m_FrameRate = graph->m_FrameRate;
    render_parameters.m_FrameCount = frame_count;

    TNodeIndex i = 0;
    while (i < node_count)
    {
        Node* node = &nodes[i];

        render_parameters.m_AudioInputs = &graph->m_AudioInputs[node->m_AudioInputsOffset];
        render_parameters.m_AudioOutputs = &graph->m_AudioOutputs[node->m_AudioOutputsOffset];
        render_parameters.m_Parameters = &graph->m_Parameters[node->m_ParametersOffset];
        render_parameters.m_Resources = &graph->m_Resources[node->m_ResourcesOffset];
        render_parameters.m_TriggerInput = &graph->m_TriggerInputs[node->m_TriggerOffset];
        render_parameters.m_ContextMemory = &graph->m_ContextMemory[node->m_ContextMemoryOffset];

        node->m_Render(graph, node, &render_parameters);

        ++i;
    }
}

bool SetParameter(HGraph graph, TNodeIndex node_index, TParameterIndex parameter_index, float value)
{
    if (node_index >= graph->m_NodeCount)
    {
        return false;
    }
    Node* node = &graph->m_Nodes[node_index];
    TParameterOffset paramter_offset = node->m_ParametersOffset + parameter_index;
    graph->m_Parameters[paramter_offset] = value;
    return true;
}

bool Trigger(HGraph graph, TNodeIndex node_index, TTriggerIndex trigger_index)
{
    if (node_index >= graph->m_NodeCount)
    {
        return false;
    }
    Node* node = &graph->m_Nodes[node_index];
    TriggerInput* trigger_input = &graph->m_TriggerInputs[node->m_TriggerOffset];
    if (graph->m_MaxTriggerEventCount == trigger_input->m_Count)
    {
        return false;
    }
    if (trigger_input->m_Buffer == 0)
    {
        graph->m_TriggerInputs[node->m_TriggerOffset].m_Buffer = &graph->m_Triggers[node->m_TriggerOffset * graph->m_MaxTriggerEventCount];
    }

    trigger_input->m_Buffer[trigger_input->m_Count++] = trigger_index;
    return true;
}

AudioOutput* GetAudioOutput(HGraph graph, TNodeIndex node_index, TAudioOutputIndex output_index)
{
    Node* node = &graph->m_Nodes[node_index];
    return &graph->m_AudioOutputs[node->m_AudioOutputsOffset + output_index];
}

static void MakeNode(
    HGraph graph,
    const GraphDescription* graph_description,
    const GraphRuntimeSettings* graph_runtime_settings,
    const NodeDescription* node_description,
    TNodeIndex* node_offset,
    TAudioInputOffset* input_offset,
    TAudioOutputOffset* output_offset,
    TParameterOffset* parameters_offset,
    TResourceOffset* resources_offset,
    TTriggerOffset* triggers_offset,
    TContextMemorySize* context_memory_offset)
{
    TNodeIndex node_index = *node_offset;
    (*node_offset) += 1;
    Node* node = &graph->m_Nodes[node_index];

    node->m_Render = node_description->m_RenderCallback;
    node->m_ParametersOffset = *parameters_offset;
    *parameters_offset += node_description->m_ParameterCount;
    node->m_AudioInputsOffset = *input_offset;
    *input_offset += node_description->m_AudioInputCount;
    node->m_AudioOutputsOffset = *output_offset;
    *output_offset += node_description->m_AudioOutputCount;
    node->m_ResourcesOffset = *resources_offset;
    *resources_offset += node_description->m_ResourceCount;
    node->m_TriggerOffset = *triggers_offset;
    *triggers_offset += node_description->m_TriggerCount > 0 ? 1 : 0;

    for (TParameterIndex i = 0; i < node_description->m_ParameterCount; ++i)
    {
        const ParameterDescription* parameter_description = &node_description->m_Parameters[i];
        graph->m_Parameters[node->m_ParametersOffset + i] = parameter_description->m_InitialValue;
    }

    node->m_ContextMemoryOffset = *context_memory_offset;
    NodeProperties node_properties;
    node_properties.m_ContextMemorySize = 0;
    if (node_description->m_GetNodePropertiesCallback != 0x0)
    {
        node_description->m_GetNodePropertiesCallback(graph_runtime_settings, &node_properties);
    }
    *context_memory_offset += node_properties.m_ContextMemorySize;
}

bool SetResource(HGraph graph, TNodeIndex node_index, TResourceIndex resource_index, const Resource* resource)
{
    Node* node = &graph->m_Nodes[node_index];
    Resource* r = &graph->m_Resources[node->m_ResourcesOffset + resource_index];
    r->m_Data = resource->m_Data;
    r->m_Size = resource->m_Size;
    return true;
}

HGraph CreateGraph(
    const GraphDescription* graph_description,
    const GraphRuntimeSettings* graph_runtime_settings,
    const GraphBuffers* graph_buffers)
{
    GraphProperties graph_properties;
    if (!GetGraphProperties(graph_description, graph_runtime_settings, &graph_properties))
    {
        return 0x0;
    }

    HGraph graph = (HGraph)(graph_buffers->m_GraphMem);
    TGraphSize graph_size = GetGraphSize(graph_description, &graph_properties);
    memset(graph, 0, graph_size);

    graph->m_FrameRate = graph_runtime_settings->m_FrameRate;
    graph->m_MaxTriggerEventCount = graph_runtime_settings->m_MaxTriggerEventCount;
    graph->m_NodeCount = graph_description->m_NodeCount;
    graph->m_ScratchBufferSize = graph_properties.m_GeneratedAudioBufferCount * graph_runtime_settings->m_MaxBatchSize;
    graph->m_Triggers = (TTriggerIndex*)graph_buffers->m_TriggerBufferMem;
    graph->m_ScratchBuffer = (float*)graph_buffers->m_ScratchBufferMem;
    graph->m_ContextMemory = (uint8_t*)graph_buffers->m_ContextMem;

    uint8_t* ptr = (uint8_t*)graph_buffers->m_GraphMem;
    uint32_t offset = ALIGN_SIZE(sizeof(Graph), sizeof(void*));

    graph->m_Parameters = (float*)&ptr[offset];
    offset += ALIGN_SIZE(sizeof(float) * graph_properties.m_ParameterCount, sizeof(void*));

    graph->m_Resources = (Resource*)&ptr[offset];
    offset += ALIGN_SIZE(sizeof(Resource) * graph_properties.m_ResourceCount, sizeof(RenderNodeCallback));

    graph->m_Nodes = (Node*)&ptr[offset];
    offset += ALIGN_SIZE(sizeof(Node) * graph_description->m_NodeCount, sizeof(AudioOutput*));
    
    graph->m_AudioInputs = (AudioInput*)&ptr[offset];
    offset += ALIGN_SIZE(sizeof(AudioInput) * graph_properties.m_AudioInputCount, sizeof(float*));

    graph->m_AudioOutputs = (AudioOutput*)&ptr[offset];
    offset += ALIGN_SIZE(sizeof(AudioOutput) * (graph_properties.m_AudioOutputCount + 1), sizeof(TTriggerIndex*));

    graph->m_TriggerInputs = (TriggerInput*)&ptr[offset];
//    offset += ALIGN_SIZE(sizeof(TriggerInput) * (graph_properties.m_TriggerInputCount), 1);

    TNodeIndex node_offset = 0;
    TAudioInputOffset input_offset = 0;
    TAudioOutputOffset output_offset = 1; // Zero is reserved for unconnected inputs
    TParameterOffset parameters_offset = 0;
    TResourceOffset resources_offset = 0;
    TTriggerOffset triggers_offset = 0;
    TContextMemorySize context_memory_offset = 0;
    for (TNodeIndex i = 0; i < graph_description->m_NodeCount; ++i)
    {
        MakeNode(graph, graph_description, graph_runtime_settings, graph_description->m_NodeDescriptions[i], &node_offset, &input_offset, &output_offset, &parameters_offset, &resources_offset, &triggers_offset, &context_memory_offset);
    }

    for (TConnectionIndex i = 0; i < graph_description->m_ConnectionCount; ++i)
    {
        const NodeConnection* node_connection = &graph_description->m_NodeConnections[i];
        if (node_connection->m_OutputNodeIndex == EXTERNAL_NODE_INDEX)
        {
            Node* input_node = &graph->m_Nodes[node_connection->m_InputNodeIndex];
            TAudioInputOffset graph_input_index = input_node->m_AudioInputsOffset + node_connection->m_InputIndex;
            if(graph->m_AudioInputs[graph_input_index].m_AudioOutput != 0x0)
            {
                return 0x0;
            }
            graph->m_AudioInputs[graph_input_index].m_AudioOutput = graph_description->m_ExternalAudioInputs[node_connection->m_OutputIndex];
        }
        else
        {
            Node* input_node = &graph->m_Nodes[node_connection->m_InputNodeIndex];
            TAudioInputOffset graph_input_index = input_node->m_AudioInputsOffset + node_connection->m_InputIndex;
            if(graph->m_AudioInputs[graph_input_index].m_AudioOutput != 0x0)
            {
                return 0x0;
            }
            Node* output_node = &graph->m_Nodes[node_connection->m_OutputNodeIndex];
            graph->m_AudioInputs[graph_input_index].m_AudioOutput = &graph->m_AudioOutputs[output_node->m_AudioOutputsOffset + node_connection->m_OutputIndex];
        }
    }

    for (TNodeIndex i = 0; i < graph_description->m_NodeCount; ++i)
    {
        Node* node = &graph->m_Nodes[i];
        AudioOutput* render_outputs = &graph->m_AudioOutputs[node->m_AudioOutputsOffset];

        const NodeDescription* node_description = graph_description->m_NodeDescriptions[i];
        for (TAudioOutputIndex j = 0; j < node_description->m_AudioOutputCount; ++j)
        {
            if (!GetOutputChannelCount(graph_description, i, j, &render_outputs[j].m_ChannelCount))
            {
                return 0x0;
            }
        }
    }

    return graph;
}

}
