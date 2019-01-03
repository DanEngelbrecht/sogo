#include "sogo.h"

#include <new>

#define ALIGN_SIZE(x, align)    (((x) + ((align) - 1)) & ~((align) - 1))

namespace sogo
{

struct Node
{
    RenderCallback      m_Render;
    TParameterIndex     m_ParametersOffset;
    TAudioInputIndex    m_AudioInputsOffset;
    TAudioOutputIndex   m_AudioOutputsOffset;
    TResourceIndex      m_ResourcesOffset;
    TTriggerIndex       m_TriggersOffset;
};

struct Graph {
    Graph(TFrameRate frame_rate,
        TNodeIndex node_count,
        TParameterIndex parameter_count,
        TResourceIndex resource_count,
        TTriggerIndex trigger_count,
        uint32_t sample_buffer_size,
        TAudioInputIndex audio_input_count,
        TAudioOutputIndex audio_output_count,
        float* parameters_data,
        Resource** resources_data,
        uint8_t* trigger_data,
        Node* node_data,
        float* scratch_buffer_data,
        AudioOutput* audio_output_data,
        AudioInput* audio_input_data);

    float* m_Parameters;
    Resource** m_Resources;
    uint8_t* m_Triggers;
    TNodeIndex m_NodeCount;
    Node* m_Nodes;
    uint32_t m_ScratchBufferSize;
    float* m_ScratchBuffer;
    uint32_t m_ScratchUsedCount;
    TFrameRate m_FrameRate;
    AudioOutput* m_AudioOutputs;
    AudioInput* m_AudioInputs;
};

Graph::Graph(
        TFrameRate frame_rate,
        TNodeIndex node_count,
        TParameterIndex ,
        TResourceIndex resource_count,
        TTriggerIndex trigger_count,
        uint32_t sample_buffer_size,
        TAudioInputIndex audio_input_count,
        TAudioOutputIndex audio_output_count,
        float* parameters_data,
        Resource** resources_data,
        uint8_t* trigger_data,
        Node* node_data,
        float* scratch_buffer_data,
        AudioOutput* audio_output_data,
        AudioInput* audio_input_data)
    : m_Parameters(parameters_data)
    , m_Resources(resources_data)
    , m_Triggers(trigger_data)
    , m_NodeCount(node_count)
    , m_Nodes(node_data)
    , m_ScratchBufferSize(sample_buffer_size)
    , m_ScratchBuffer(scratch_buffer_data)
    , m_ScratchUsedCount(0)
    , m_FrameRate(frame_rate)
    , m_AudioOutputs(audio_output_data)
    , m_AudioInputs(audio_input_data)
{
    for (TAudioInputIndex i = 0; i < audio_input_count; ++i)
    {
        m_AudioInputs[i].m_AudioOutput = 0x0;
    }
    for (TAudioOutputIndex i = 0; i < audio_output_count; ++i)
    {
        m_AudioOutputs[0].m_Buffer = 0x0;
        m_AudioOutputs[0].m_ChannelCount = 0;
    }
    for (TResourceIndex i = 0; i < resource_count; ++i)
    {
        m_Resources[i] = 0x0;
    }
    for (TTriggerIndex i = 0; i < trigger_count; ++i)
    {
        m_Triggers[i] = 0x0;
    }
}

static bool GetOutputChannelCount(
    const GraphDescription* graph_description,
    TNodeIndex node_index,
    TAudioOutputIndex output_index,
    TChannelIndex& out_channel_count)
{
    const NodeDescription* node_description = graph_description->m_NodeDescriptions[node_index];
    const AudioOutputDescription* output_description = &node_description->m_AudioOutputDescriptions[output_index];
    switch (output_description->m_Mode)
    {
        case AudioOutputDescription::FIXED:
            out_channel_count = output_description->m_ChannelCount;
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
                        out_channel_count = graph_description->m_ExternalAudioInputs[node_connection->m_OutputIndex]->m_ChannelCount;
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
    TChannelIndex& out_channel_count)
{
    const NodeDescription* node_description = graph_description->m_NodeDescriptions[node_index];
    const AudioOutputDescription* output_description = &node_description->m_AudioOutputDescriptions[output_index];
    switch (output_description->m_Mode)
    {
        case AudioOutputDescription::PASS_THROUGH:
            out_channel_count = 0;
            return true;
        case AudioOutputDescription::FIXED:
            out_channel_count = output_description->m_ChannelCount;
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
    TTriggerIndex m_TriggerCount;
    TAudioInputIndex m_AudioInputCount;
    TAudioOutputIndex m_AudioOutputCount;
    TSampleIndex m_SampleBufferSize;
};

static bool GetGraphProperties(
    TFrameIndex max_batch_size,
    const GraphDescription* graph_description,
    GraphProperties* graph_properties)
{
    TParameterIndex parameter_count = 0;
    TResourceIndex resource_count = 0;
    TTriggerIndex trigger_count = 0;
    TAudioInputIndex audio_input_count = 0;
    TAudioOutputIndex audio_output_count = 0;
    uint32_t generated_buffer_count = 0;

    for (TNodeIndex i = 0; i < graph_description->m_NodeCount; ++i)
    {
        const NodeDescription* node_description = graph_description->m_NodeDescriptions[i];
        parameter_count += node_description->m_ParameterCount;
        resource_count += node_description->m_ResourceCount;
        trigger_count += node_description->m_TriggerCount;
        audio_input_count += node_description->m_AudioInputCount;
        audio_output_count += node_description->m_AudioOutputCount;
        for (TAudioOutputIndex j = 0; j < node_description->m_AudioOutputCount; ++j)
        {
            TAudioOutputIndex buffer_count = 0;
            if (!GetOutputChannelAllocationCount(graph_description, i, j, buffer_count))
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

    uint32_t sample_buffer_size = generated_buffer_count * max_batch_size;

    graph_properties->m_ParameterCount = parameter_count;
    graph_properties->m_ResourceCount = resource_count;
    graph_properties->m_TriggerCount = trigger_count;
    graph_properties->m_AudioInputCount = audio_input_count;
    graph_properties->m_AudioOutputCount = audio_output_count;
    graph_properties->m_SampleBufferSize = sample_buffer_size;
    return true;
}

static size_t GetGraphSize(
    const GraphDescription* graph_description,
    const GraphProperties* graph_properties)
{
    size_t s = ALIGN_SIZE(sizeof(Graph), sizeof(void*)) +
        ALIGN_SIZE(sizeof(float) * graph_properties->m_ParameterCount, sizeof(Resource*)) +
        ALIGN_SIZE(sizeof(Resource*) * graph_properties->m_ResourceCount, sizeof(uint8_t)) +
        ALIGN_SIZE((sizeof(uint8_t) * graph_properties->m_TriggerCount), sizeof(RenderCallback)) +
        ALIGN_SIZE(sizeof(Node) * graph_description->m_NodeCount, sizeof(float)) +
        ALIGN_SIZE(sizeof(AudioOutput) * (graph_properties->m_AudioOutputCount + 1), sizeof(AudioOutput*)) +
        ALIGN_SIZE(sizeof(AudioInput) * graph_properties->m_AudioInputCount, 1);
    return s;
}

bool GetGraphSize(TFrameIndex max_batch_size, const GraphDescription* graph_description, size_t& out_graph_size, TSampleIndex& out_scratch_buffer_sample_count)
{
    GraphProperties graph_properties;
    if (!GetGraphProperties(max_batch_size, graph_description, &graph_properties))
    {
        return false;
    }
    out_graph_size = GetGraphSize(graph_description, &graph_properties);
    out_scratch_buffer_sample_count = graph_properties.m_SampleBufferSize;
    return true;
}

static bool ConnectInternal(HGraph graph, TNodeIndex input_node_index, TAudioInputIndex input_index, TNodeIndex output_node_index, TAudioOutputIndex output_index)
{
    Node* input_node = &graph->m_Nodes[input_node_index];
    TAudioInputIndex graph_input_index = input_node->m_AudioInputsOffset + input_index;
    if(graph->m_AudioInputs[graph_input_index].m_AudioOutput != 0x0)
    {
        return false;
    }
    Node* output_node = &graph->m_Nodes[output_node_index];
    graph->m_AudioInputs[graph_input_index].m_AudioOutput = &graph->m_AudioOutputs[output_node->m_AudioOutputsOffset + output_index];
    return true;
}

static bool ConnectExternal(HGraph graph, TNodeIndex input_node_index, TAudioInputIndex input_index, AudioOutput** external_inputs, TAudioOutputIndex output_index)
{
    Node* input_node = &graph->m_Nodes[input_node_index];
    TAudioInputIndex graph_input_index = input_node->m_AudioInputsOffset + input_index;
    if(graph->m_AudioInputs[graph_input_index].m_AudioOutput != 0x0)
    {
        return false;
    }
    graph->m_AudioInputs[graph_input_index].m_AudioOutput = external_inputs[output_index];
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

bool RenderGraph(HGraph graph, TFrameIndex frame_count)
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
        Node& node = nodes[i];

        render_parameters.m_AudioInputs = &graph->m_AudioInputs[node.m_AudioInputsOffset];
        render_parameters.m_AudioOutputs = &graph->m_AudioOutputs[node.m_AudioOutputsOffset];
        render_parameters.m_Parameters = &graph->m_Parameters[node.m_ParametersOffset];
        render_parameters.m_Resources = &graph->m_Resources[node.m_ResourcesOffset];
        render_parameters.m_Triggers = &graph->m_Triggers[node.m_TriggersOffset];

        if (!node.m_Render(graph, &node, &render_parameters))
        {
            return false;
        }

        ++i;
    }
    return true;
}

bool SetParameter(HGraph graph, TNodeIndex node_index, TParameterIndex parameter_index, float value)
{
    if (node_index >= graph->m_NodeCount)
    {
        return false;
    }
    Node* node = &graph->m_Nodes[node_index];
    TParameterIndex paramter_offset = node->m_ParametersOffset + parameter_index;
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
    TTriggerIndex trigger_offset = node->m_TriggersOffset + trigger_index;
    if (graph->m_Triggers[trigger_offset] == 255)
    {
        return false;
    }
    graph->m_Triggers[trigger_offset] += 1;
    return true;
}

AudioOutput* GetOutput(HGraph graph, TNodeIndex node_index, TAudioOutputIndex output_index)
{
    Node* node = &graph->m_Nodes[node_index];
    return &graph->m_AudioOutputs[node->m_AudioOutputsOffset + output_index];
}

static bool MakeNode(HGraph graph, const NodeDescription* node_description, TNodeIndex& node_offset, TAudioInputIndex& input_offset, TAudioOutputIndex& output_offset, TParameterIndex& parameters_offset, TResourceIndex& resources_offset, TTriggerIndex& triggers_offset)
{
    TNodeIndex node_index = node_offset;
    node_offset += 1;
    Node& node = graph->m_Nodes[node_index];

    node.m_Render = node_description->m_RenderCallback;
    node.m_ParametersOffset = parameters_offset;
    parameters_offset += node_description->m_ParameterCount;
    node.m_AudioInputsOffset = input_offset;
    input_offset += node_description->m_AudioInputCount;
    node.m_AudioOutputsOffset = output_offset;
    output_offset += node_description->m_AudioOutputCount;
    node.m_ResourcesOffset = resources_offset;
    resources_offset += node_description->m_ResourceCount;
    node.m_TriggersOffset = triggers_offset;
    triggers_offset += node_description->m_TriggerCount;

    for (TParameterIndex i = 0; i < node_description->m_ParameterCount; ++i)
    {
        const ParameterDescription* parameter_description = &node_description->m_Parameters[i];
        graph->m_Parameters[node.m_ParametersOffset + i] = parameter_description->m_InitialValue;
    }

    for (TTriggerIndex i = 0; i < node_description->m_TriggerCount; ++i)
    {
        const TriggerDescription* trigger_description = &node_description->m_Triggers[i];
        graph->m_Triggers[node.m_TriggersOffset + i] = 0;
    }
    return true;
}

bool SetResource(HGraph graph, TNodeIndex node_index, TResourceIndex resource_index, Resource* resource)
{
    Node* node = &graph->m_Nodes[node_index];
    graph->m_Resources[node->m_ResourcesOffset + resource_index] = resource;
    return true;
}

HGraph CreateGraph(
    void* graph_mem,
    float* scratch_buffer,
    TFrameRate frame_rate,
    TFrameIndex max_batch_size,
    const GraphDescription* graph_description)
{
    GraphProperties graph_properties;
    if (!GetGraphProperties(max_batch_size, graph_description, &graph_properties))
    {
        return 0x0;
    }

    uint8_t* ptr = (uint8_t*)graph_mem;
    size_t offset = ALIGN_SIZE(sizeof(Graph), sizeof(void*));

    float* parameters_data = (float*)&ptr[offset];
    offset += ALIGN_SIZE(sizeof(float) * graph_properties.m_ParameterCount, sizeof(Resource*));

    Resource** resources_data = (Resource**)&ptr[offset];
    offset += ALIGN_SIZE(sizeof(Resource*) * graph_properties.m_ResourceCount, sizeof(uint8_t));

    uint8_t* triggers_data = (uint8_t*)&ptr[offset];
    offset += ALIGN_SIZE(sizeof(uint8_t*) * graph_properties.m_TriggerCount, sizeof(RenderCallback));

    Node* node_data = (Node*)&ptr[offset];
    offset += ALIGN_SIZE(sizeof(Node) * graph_description->m_NodeCount, sizeof(float));
    
    AudioInput* audio_input_data = (AudioInput*)&ptr[offset];
    offset += ALIGN_SIZE(sizeof(AudioInput) * graph_properties.m_AudioInputCount, sizeof(AudioOutput*));

    AudioOutput* audio_output_data = (AudioOutput*)&ptr[offset];
    offset += ALIGN_SIZE(sizeof(AudioOutput) * (graph_properties.m_AudioOutputCount + 1), 1);

    HGraph graph = new (graph_mem) Graph(
        frame_rate,
        graph_description->m_NodeCount,
        graph_properties.m_ParameterCount,
        graph_properties.m_ResourceCount,
        graph_properties.m_TriggerCount,
        graph_properties.m_SampleBufferSize,
        graph_properties.m_AudioInputCount,
        graph_properties.m_AudioOutputCount + 1,
        parameters_data,
        resources_data,
        triggers_data,
        node_data,
        scratch_buffer,
        audio_output_data,
        audio_input_data);

    TNodeIndex node_offset = 0;
    TAudioInputIndex input_offset = 0;
    TAudioOutputIndex output_offset = 1; // Zero is reserved for unconnected inputs
    TParameterIndex parameters_offset = 0;
    TResourceIndex resources_offset = 0;
    TTriggerIndex triggers_offset = 0;
    for (TNodeIndex i = 0; i < graph_description->m_NodeCount; ++i)
    {
        if (!MakeNode(graph, graph_description->m_NodeDescriptions[i], node_offset, input_offset, output_offset, parameters_offset, resources_offset, triggers_offset))
        {
            return 0x0;
        }
    }

    for (TConnectionIndex i = 0; i < graph_description->m_ConnectionCount; ++i)
    {
        const NodeConnection* node_connection = &graph_description->m_NodeConnections[i];
        if (node_connection->m_OutputNodeIndex == EXTERNAL_NODE_INDEX)
        {
            if (!ConnectExternal(graph, node_connection->m_InputNodeIndex, node_connection->m_InputIndex, graph_description->m_ExternalAudioInputs, node_connection->m_OutputIndex))
            {
                return 0x0;
            }
        }
        else
        {
            if (node_connection->m_OutputNodeIndex >= node_connection->m_InputNodeIndex)
            {
                return 0x0;
            }
            if (!ConnectInternal(graph, node_connection->m_InputNodeIndex, node_connection->m_InputIndex, node_connection->m_OutputNodeIndex, node_connection->m_OutputIndex))
            {
                return 0x0;
            }
        }
    }

    for (TNodeIndex i = 0; i < graph_description->m_NodeCount; ++i)
    {
        Node* node = &graph->m_Nodes[i];
        AudioOutput* render_outputs = &graph->m_AudioOutputs[node->m_AudioOutputsOffset];

        const NodeDescription* node_description = graph_description->m_NodeDescriptions[i];
        for (uint16_t j = 0; j < node_description->m_AudioOutputCount; ++j)
        {
            if (!GetOutputChannelCount(graph_description, i, j, render_outputs[j].m_ChannelCount))
            {
                return 0x0;
            }
        }
    }

    return graph;
}

}
