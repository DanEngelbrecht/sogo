#include "sogo.h"

#include "../third-party/containers/src/jc/hashtable.h"
#include "../third-party/xxHash-1/xxhash.h"

#include <new>

#define ALIGN_SIZE(x, align)    (((x) + ((align) - 1)) & ~((align) - 1))

namespace sogo
{

struct Node
{
    RenderCallback m_Render;
    TParameterIndex m_ParametersOffset;
    TInputIndex m_InputsOffset;
    TOutputIndex m_OutputsOffset;
    TResourceIndex m_ResourcesOffset;
    TTriggerIndex m_TriggersOffset;
};

typedef jc::HashTable<TParameterNameHash, TParameterIndex> TParameterLookup;
typedef jc::HashTable<TTriggerNameHash, TParameterIndex> TTriggerLookup;

uint32_t GetParameterLookupSize(uint32_t num_elements)
{
    const uint32_t load_factor    = 100; // percent
    const uint32_t tablesize      = uint32_t(num_elements / (load_factor/100.0f));
    return TParameterLookup::CalcSize(tablesize);
}

uint32_t GetTriggerLookupSize(uint32_t num_elements)
{
    const uint32_t load_factor    = 100; // percent
    const uint32_t tablesize      = uint32_t(num_elements / (load_factor/100.0f));
    return TTriggerLookup::CalcSize(tablesize);
}

struct Graph {
    Graph(TFrameRate frame_rate,
        TNodeIndex node_count,
        TParameterIndex parameter_count,
        TResourceIndex resource_count,
        TTriggerIndex trigger_count,
        uint32_t sample_buffer_size,
        TInputIndex input_count,
        TOutputIndex output_count,
        void* parameter_lookup_data,
        float* parameters_data,
        Resource** resources_data,
        void* trigger_lookup_data,
        uint8_t* trigger_data,
        Node* node_data,
        float* scratch_buffer_data,
        RenderOutput* render_output_data,
        RenderInput* render_input_data);

    TParameterLookup m_ParameterLookup;
    TTriggerLookup m_TriggerLookup;
    float* m_Parameters;
    Resource** m_Resources;
    uint8_t* m_Triggers;
    TNodeIndex m_NodeCount;
    Node* m_Nodes;
    uint32_t m_ScratchBufferSize;
    float* m_ScratchBuffer;
    uint32_t m_ScratchUsedCount;
    TFrameRate m_FrameRate;
    RenderOutput* m_RenderOutputs;
    RenderInput* m_RenderInputs;
};

Graph::Graph(TFrameRate frame_rate,
    TNodeIndex node_count,
    TParameterIndex parameter_count,
    TResourceIndex resource_count,
    TTriggerIndex trigger_count,
    uint32_t sample_buffer_size,
    TInputIndex input_count,
    TOutputIndex output_count,
    void* parameter_lookup_data,
    float* parameters_data,
    Resource** resources_data,
    void* trigger_lookup_data,
    uint8_t* trigger_data,
    Node* node_data,
    float* scratch_buffer_data,
    RenderOutput* render_output_data,
    RenderInput* render_input_data)
    : m_ParameterLookup(parameter_count, parameter_lookup_data)
    , m_TriggerLookup(trigger_count, trigger_lookup_data)
    , m_Parameters(parameters_data)
    , m_Resources(resources_data)
    , m_Triggers(trigger_data)
    , m_NodeCount(node_count)
    , m_Nodes(node_data)
    , m_ScratchBufferSize(sample_buffer_size)
    , m_ScratchBuffer(scratch_buffer_data)
    , m_ScratchUsedCount(0)
    , m_FrameRate(frame_rate)
    , m_RenderOutputs(render_output_data)
    , m_RenderInputs(render_input_data)
{
    for (TInputIndex i = 0; i < input_count; ++i)
    {
        m_RenderInputs[i].m_RenderOutput = 0x0;
    }
    for (TOutputIndex i = 0; i < output_count; ++i)
    {
        m_RenderOutputs[0].m_Buffer = 0x0;
        m_RenderOutputs[0].m_ChannelCount = 0;
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

static TChannelCount GetOutputChannelCount(
    const GraphDescription* graph_description,
    TNodeIndex node_index,
    TOutputIndex output_index)
{
    const NodeDescription* node_description = graph_description->m_NodeDescriptions[node_index];
    const OutputDescription* output_description = &node_description->m_OutputDescriptions[output_index];
    switch (output_description->m_Mode)
    {
        case OutputDescription::FIXED:
            return output_description->m_ChannelCount;
        case OutputDescription::PASS_THROUGH:
        case OutputDescription::AS_INPUT:
        {
            for (TConnectionIndex i = 0; i < graph_description->m_ConnectionCount; ++i)
            {
                const NodeConnection* node_connection = &graph_description->m_NodeConnections[i];
                if (node_connection->m_InputNodeIndex == node_index &&
                    node_connection->m_InputIndex == output_description->m_InputIndex)
                {
                    if (node_connection->m_OutputNodeIndex == EXTERNAL_NODE_INDEX)
                    {
                        return graph_description->m_ExternalInputs[node_connection->m_OutputIndex]->m_ChannelCount;
                    }
                    return GetOutputChannelCount(
                        graph_description,
                        node_connection->m_OutputNodeIndex,
                        node_connection->m_OutputIndex);
                }
            }
            return 0;
        }
        default:
            return 0;
    }
}

static TChannelCount GetOutputChannelAllocationCount(
    const GraphDescription* graph_description,
    TNodeIndex node_index,
    TOutputIndex output_index)
{
    const NodeDescription* node_description = graph_description->m_NodeDescriptions[node_index];
    const OutputDescription* output_description = &node_description->m_OutputDescriptions[output_index];
    switch (output_description->m_Mode)
    {
        case OutputDescription::PASS_THROUGH:
            return 0;
        case OutputDescription::FIXED:
            return output_description->m_ChannelCount;
        case OutputDescription::AS_INPUT:
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
                        node_connection->m_OutputIndex);
                }
            }
            return 0;
        }
        default:
            return 0;
    }
}

struct GraphProperties {
    TParameterIndex m_ParameterCount;
    TResourceIndex m_ResourceCount;
    TTriggerIndex m_TriggerCount;
    TInputIndex m_InputCount;
    TOutputIndex m_OutputCount;
    uint32_t m_SampleBufferSize;
};

static void GetGraphProperties(
    uint32_t max_batch_size,
    const GraphDescription* graph_description,
    GraphProperties* graph_properties)
{
    TParameterIndex parameter_count = 0;
    TResourceIndex resource_count = 0;
    TTriggerIndex trigger_count = 0;
    TInputIndex input_count = 0;
    TOutputIndex output_count = 0;
    uint32_t generated_buffer_count = 0;

    for (TNodeIndex i = 0; i < graph_description->m_NodeCount; ++i)
    {
        const NodeDescription* node_description = graph_description->m_NodeDescriptions[i];
        parameter_count += node_description->m_ParameterCount;
        resource_count += node_description->m_ResourceCount;
        trigger_count += node_description->m_TriggerCount;
        input_count += node_description->m_InputCount;
        output_count += node_description->m_OutputCount;
        for (TOutputIndex j = 0; j < node_description->m_OutputCount; ++j)
        {
            generated_buffer_count += GetOutputChannelAllocationCount(graph_description, i, j);
        }
    }

    uint32_t sample_buffer_size = generated_buffer_count * max_batch_size;

    graph_properties->m_ParameterCount = parameter_count;
    graph_properties->m_ResourceCount = resource_count;
    graph_properties->m_TriggerCount = trigger_count;
    graph_properties->m_InputCount = input_count;
    graph_properties->m_OutputCount = output_count;
    graph_properties->m_SampleBufferSize = sample_buffer_size;
}

static size_t GetGraphSize(
    const GraphDescription* graph_description,
    const GraphProperties* graph_properties)
{
    size_t s = ALIGN_SIZE(sizeof(Graph), sizeof(void*)) +
        ALIGN_SIZE(GetParameterLookupSize(graph_properties->m_ParameterCount), sizeof(void*)) +
        ALIGN_SIZE(GetTriggerLookupSize(graph_properties->m_TriggerCount), sizeof(float)) +
        ALIGN_SIZE(sizeof(float) * graph_properties->m_ParameterCount, sizeof(Resource*)) +
        ALIGN_SIZE(sizeof(Resource*) * graph_properties->m_ResourceCount, sizeof(uint8_t)) +
        ALIGN_SIZE((sizeof(uint8_t) * graph_properties->m_TriggerCount), sizeof(RenderCallback)) +
        ALIGN_SIZE(sizeof(Node) * graph_description->m_NodeCount, sizeof(float)) +
        ALIGN_SIZE(sizeof(float) * graph_properties->m_SampleBufferSize, sizeof(float*)) +
        ALIGN_SIZE(sizeof(RenderOutput) * (graph_properties->m_OutputCount + 1), sizeof(RenderOutput*)) +
        ALIGN_SIZE(sizeof(RenderInput) * graph_properties->m_InputCount, 1);
    return s;
}

size_t GetGraphSize(TFrameCount max_batch_size, const GraphDescription* graph_description)
{
    GraphProperties graph_properties;
    GetGraphProperties(max_batch_size, graph_description, &graph_properties);
    size_t graph_size = GetGraphSize(graph_description, &graph_properties);
    return graph_size;
}

void DisposeGraph(HGraph graph)
{
    if (graph != 0x0)
    {
        graph->~Graph();
    }
}

TParameterNameHash MakeParameterHash(TNodeIndex node_index, const char* parameter_name)
{
    XXH32_hash_t index_hash = XXH32(&node_index, sizeof(TNodeIndex), 0);
    XXH32_hash_t hash = XXH32(parameter_name, strlen(parameter_name), index_hash);
    return (TParameterNameHash)hash;
}

TTriggerNameHash MakeTriggerHash(TNodeIndex node_index, const char* trigger_name)
{
    XXH32_hash_t index_hash = XXH32(&node_index, sizeof(TNodeIndex), 0);
    XXH32_hash_t hash = XXH32(trigger_name, strlen(trigger_name), index_hash);
    return (TTriggerNameHash)hash;
}

static bool ConnectInternal(HGraph graph, TNodeIndex input_node_index, TInputIndex input_index, TNodeIndex output_node_index, TOutputIndex output_index)
{
    Node* input_node = &graph->m_Nodes[input_node_index];
    TInputIndex graph_input_index = input_node->m_InputsOffset + input_index;
    if(graph->m_RenderInputs[graph_input_index].m_RenderOutput != 0x0)
    {
        return false;
    }
    Node* output_node = &graph->m_Nodes[output_node_index];
    graph->m_RenderInputs[graph_input_index].m_RenderOutput = &graph->m_RenderOutputs[output_node->m_OutputsOffset + output_index];
    return true;
}

static bool ConnectExternal(HGraph graph, TNodeIndex input_node_index, TInputIndex input_index, RenderOutput** external_inputs, TOutputIndex output_index)
{
    Node* input_node = &graph->m_Nodes[input_node_index];
    TInputIndex graph_input_index = input_node->m_InputsOffset + input_index;
    if(graph->m_RenderInputs[graph_input_index].m_RenderOutput != 0x0)
    {
        return false;
    }
    graph->m_RenderInputs[graph_input_index].m_RenderOutput = external_inputs[output_index];
    return true;
}

static float* AllocateBuffer(HGraph graph, HNode , TChannelCount channel_count, TFrameCount frame_count)
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

bool RenderGraph(HGraph graph, TFrameCount frame_count)
{
    graph->m_ScratchUsedCount = 0;
    Node* nodes = graph->m_Nodes;
    TNodeIndex node_count = graph->m_NodeCount;

    RenderParameters render_parameters;
    render_parameters.m_AllocateBuffer = AllocateBuffer;
    render_parameters.m_FrameRate = graph->m_FrameRate;
    render_parameters.m_FrameCount = frame_count;

    TNodeIndex i = 0;
    while (i < node_count)
    {
        Node& node = nodes[i];

        render_parameters.m_RenderInputs = &graph->m_RenderInputs[node.m_InputsOffset];
        render_parameters.m_RenderOutputs = &graph->m_RenderOutputs[node.m_OutputsOffset];
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

bool SetParameter(HGraph graph, TParameterNameHash parameter_hash, float value)
{
    TParameterIndex* index = graph->m_ParameterLookup.Get(parameter_hash);
    if (index == 0x0)
    {
        return false;
    }
    graph->m_Parameters[*index] = value;
    return true;
}

bool Trigger(HGraph graph, TTriggerNameHash trigger_hash)
{
    TTriggerIndex* index = graph->m_TriggerLookup.Get(trigger_hash);
    if (index == 0x0)
    {
        return false;
    }
    if (graph->m_Triggers[*index] == 255)
    {
        return false;
    }
    graph->m_Triggers[*index] += 1;
    return true;
}

RenderOutput* GetOutput(HGraph graph, TNodeIndex node_index, TOutputIndex output_index)
{
    Node* node = &graph->m_Nodes[node_index];
    return &graph->m_RenderOutputs[node->m_OutputsOffset + output_index];
}

static void RegisterNamedParameter(HGraph graph, TNodeIndex node_index, TParameterIndex parameter_index, const char* parameter_name)
{
    uint32_t node_key = MakeParameterHash(node_index, parameter_name);
    graph->m_ParameterLookup.Put(node_key, parameter_index);
}

static void RegisterNamedTrigger(HGraph graph, TNodeIndex node_index, TTriggerIndex trigger_index, const char* trigger_name)
{
    uint32_t node_key = MakeTriggerHash(node_index, trigger_name);
    graph->m_TriggerLookup.Put(node_key, trigger_index);
}

static void MakeNode(HGraph graph, const NodeDescription* node_description, TNodeIndex& node_offset, TInputIndex& input_offset, TOutputIndex& output_offset, TParameterIndex& parameters_offset, TResourceIndex& resources_offset, TTriggerIndex& triggers_offset)
{
    TNodeIndex node_index = node_offset;
    node_offset += 1;
    Node& node = graph->m_Nodes[node_index];

    node.m_Render = node_description->m_RenderCallback;
    node.m_ParametersOffset = parameters_offset;
    parameters_offset += node_description->m_ParameterCount;
    node.m_InputsOffset = input_offset;
    input_offset += node_description->m_InputCount;
    node.m_OutputsOffset = output_offset;
    output_offset += node_description->m_OutputCount;
    node.m_ResourcesOffset = resources_offset;
    resources_offset += node_description->m_ResourceCount;
    node.m_TriggersOffset = triggers_offset;
    triggers_offset += node_description->m_TriggerCount;

    for (TParameterIndex i = 0; i < node_description->m_ParameterCount; ++i)
    {
        const ParameterDescription* parameter_description = &node_description->m_Parameters[i];
        graph->m_Parameters[node.m_ParametersOffset + i] = parameter_description->m_InitialValue;
        if (parameter_description->m_ParameterName != 0x0)
        {
            RegisterNamedParameter(graph, node_index, node.m_ParametersOffset + i, parameter_description->m_ParameterName);
        }
    }

    for (TTriggerIndex i = 0; i < node_description->m_TriggerCount; ++i)
    {
        const TriggerDescription* trigger_description = &node_description->m_Triggers[i];
        if (trigger_description->m_TriggerName != 0x0)
        {
            RegisterNamedTrigger(graph, node_index, node.m_TriggersOffset + i, trigger_description->m_TriggerName);
        }
    }
}

bool SetResource(HGraph graph, TNodeIndex node_index, TResourceIndex resource_index, Resource* resource)
{
    Node* node = &graph->m_Nodes[node_index];
    graph->m_Resources[node->m_ResourcesOffset + resource_index] = resource;
    return true;
}

HGraph CreateGraph(void* mem,
    TFrameRate frame_rate,
    TFrameCount max_batch_size,
    const GraphDescription* graph_description)
{
    GraphProperties graph_properties;
    GetGraphProperties(max_batch_size, graph_description, &graph_properties);

    uint8_t* ptr = (uint8_t*)mem;
    size_t offset = ALIGN_SIZE(sizeof(Graph), sizeof(void*));

    void* parameter_lookup_data = &ptr[offset];
    offset += ALIGN_SIZE(GetParameterLookupSize(graph_properties.m_ParameterCount), sizeof(void*));

    void* trigger_lookup_data = &ptr[offset];
    offset += ALIGN_SIZE(GetTriggerLookupSize(graph_properties.m_TriggerCount), sizeof(float));

    float* parameters_data = (float*)&ptr[offset];
    offset += ALIGN_SIZE(sizeof(float) * graph_properties.m_ParameterCount, sizeof(Resource*));

    Resource** resources_data = (Resource**)&ptr[offset];
    offset += ALIGN_SIZE(sizeof(Resource*) * graph_properties.m_ResourceCount, sizeof(uint8_t));

    uint8_t* triggers_data = (uint8_t*)&ptr[offset];
    offset += ALIGN_SIZE(sizeof(uint8_t*) * graph_properties.m_TriggerCount, sizeof(RenderCallback));

    Node* node_data = (Node*)&ptr[offset];
    offset += ALIGN_SIZE(sizeof(Node) * graph_description->m_NodeCount, sizeof(float));
    
    float* scratch_buffer_data = (float*)&ptr[offset];
    offset += ALIGN_SIZE(sizeof(float) * graph_properties.m_SampleBufferSize, sizeof(float*));
    
    RenderInput* render_input_data = (RenderInput*)&ptr[offset];
    offset += ALIGN_SIZE(sizeof(RenderInput) * graph_properties.m_InputCount, sizeof(RenderOutput*));

    RenderOutput* render_output_data = (RenderOutput*)&ptr[offset];
    offset += ALIGN_SIZE(sizeof(RenderOutput) * (graph_properties.m_OutputCount + 1), 1);

    HGraph graph = new (mem) Graph(
        frame_rate,
        graph_description->m_NodeCount,
        graph_properties.m_ParameterCount,
        graph_properties.m_ResourceCount,
        graph_properties.m_TriggerCount,
        graph_properties.m_SampleBufferSize,
        graph_properties.m_InputCount,
        graph_properties.m_OutputCount + 1,
        parameter_lookup_data,
        parameters_data,
        resources_data,
        trigger_lookup_data,
        triggers_data,
        node_data,
        scratch_buffer_data,
        render_output_data,
        render_input_data);

    TNodeIndex node_offset = 0;
    TInputIndex input_offset = 0;
    TOutputIndex output_offset = 1; // Zero is reserved for unconnected inputs
    TParameterIndex parameters_offset = 0;
    TResourceIndex resources_offset = 0;
    TTriggerIndex triggers_offset = 0;
    for (TNodeIndex i = 0; i < graph_description->m_NodeCount; ++i)
    {
        MakeNode(graph, graph_description->m_NodeDescriptions[i], node_offset, input_offset, output_offset, parameters_offset, resources_offset, triggers_offset);
    }

    for (TConnectionIndex i = 0; i < graph_description->m_ConnectionCount; ++i)
    {
        const NodeConnection* node_connection = &graph_description->m_NodeConnections[i];
        if (node_connection->m_OutputNodeIndex == EXTERNAL_NODE_INDEX)
        {
            ConnectExternal(graph, node_connection->m_InputNodeIndex, node_connection->m_InputIndex, graph_description->m_ExternalInputs, node_connection->m_OutputIndex);
        }
        else
        {
            ConnectInternal(graph, node_connection->m_InputNodeIndex, node_connection->m_InputIndex, node_connection->m_OutputNodeIndex, node_connection->m_OutputIndex);
        }
    }

    for (TNodeIndex i = 0; i < graph_description->m_NodeCount; ++i)
    {
        Node* node = &graph->m_Nodes[i];
        RenderOutput* render_outputs = &graph->m_RenderOutputs[node->m_OutputsOffset];

        const NodeDescription* node_description = graph_description->m_NodeDescriptions[i];
        for (uint16_t j = 0; j < node_description->m_OutputCount; ++j)
        {
            render_outputs[j].m_ChannelCount = GetOutputChannelCount(graph_description, i, j);
        }
    }

    return graph;
}

}
