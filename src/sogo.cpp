#include "sogo.h"

#include <string.h>

#ifdef _MSC_VER
    #include <malloc.h>
    #undef alloca
    #define alloca _malloca
#else
    #include <alloca.h>
#endif

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
    RenderCallback          m_Render;
    TParameterOffset        m_ParametersOffset;
    TAudioInputOffset       m_AudioInputsOffset;
    TAudioOutputOffset      m_AudioOutputsOffset;
    TResourceOffset         m_ResourcesOffset;
    TTriggerOffset          m_TriggerInputOffset;
    TTriggerOffset          m_TriggerOutputOffset;
    TNodeIndex              m_DependencyCount;
    TNodeIndex              m_DependencyOffset;
    TContextMemoryOffset    m_ContextMemoryOffset;
};

struct Graph {
    TParameter*         m_Parameters;
    Resource*           m_Resources;
    TTriggerCount       m_MaxTriggerEventCount;
    TTriggerInputIndex* m_Triggers;
    TNodeIndex          m_NodeCount;
    Node*               m_Nodes;
    uint32_t            m_ScratchBufferSize;
    float*              m_ScratchBuffer;
    uint32_t            m_ScratchUsedCount;
    uint8_t*            m_ContextMemory;
    TFrameRate          m_FrameRate;
    AudioOutput*        m_AudioOutputs;
    AudioInput*         m_AudioInputs;
    TriggerInput*       m_TriggerInputs;
    TriggerOutput*      m_TriggerOutputs;
    TNodeIndex*         m_Dependencies;
};

static bool GetOutputChannelCount(
    const GraphDescription* graph_description,
    const GraphRuntimeSettings* graph_runtime_settings,
    TNodeIndex node_index,
    TAudioOutputIndex output_index,
    TChannelIndex* out_channel_count)
{
    const NodeDescription& node_description = graph_description->m_NodeDescriptions[node_index];
    const AudioOutputDescription* output_description = &node_description.m_NodeStaticDescription.m_AudioOutputDescriptions[output_index];
    switch (output_description->m_Mode)
    {
        case AudioOutputDescription::FIXED:
            *out_channel_count = output_description->m_ChannelCount;
            return true;
        case AudioOutputDescription::PASS_THROUGH:
        case AudioOutputDescription::AS_INPUT:
        {
            for (TConnectionIndex i = 0; i < node_description.m_AudioConnectionCount; ++i)
            {
                const NodeAudioConnection* node_connection = &node_description.m_AudioConnections[i];
                if (node_connection->m_InputIndex == output_description->m_InputIndex)
                {
                    if (node_connection->m_OutputConnection.m_OutputNodeIndex == EXTERNAL_NODE_INDEX)
                    {
                        *out_channel_count = graph_description->m_ExternalAudioInputs[node_connection->m_OutputConnection.m_OutputIndex]->m_ChannelCount;
                        return true;
                    }
                    return GetOutputChannelCount(
                        graph_description,
                        graph_runtime_settings,
                        node_connection->m_OutputConnection.m_OutputNodeIndex,
                        node_connection->m_OutputConnection.m_OutputIndex,
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
    const GraphRuntimeSettings* graph_runtime_settings,
    TNodeIndex node_index,
    TAudioOutputIndex output_index,
    TChannelIndex* out_channel_count)
{
    const NodeDescription& node_description = graph_description->m_NodeDescriptions[node_index];
    const AudioOutputDescription* output_description = &node_description.m_NodeStaticDescription.m_AudioOutputDescriptions[output_index];
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
            for (TConnectionIndex i = 0; i < node_description.m_AudioConnectionCount; ++i)
            {
                const NodeAudioConnection* node_connection = &node_description.m_AudioConnections[i];
                if (node_connection->m_InputIndex == output_description->m_InputIndex)
                {
                    return GetOutputChannelCount(
                        graph_description,
                        graph_runtime_settings,
                        node_connection->m_OutputConnection.m_OutputNodeIndex,
                        node_connection->m_OutputConnection.m_OutputIndex,
                        out_channel_count);
                }
            }
            return false;
        }
        default:
            return false;
    }
}

static TNodeIndex GetDependencies(const NodeDescription* node_description, TNodeIndex* out_dependecies)
{
    TConnectionIndex connection_count = node_description->m_AudioConnectionCount + node_description->m_TriggerConnectionCount;
    if (connection_count == 0)
    {
        return 0;
    }

    TNodeIndex* dependencies = out_dependecies ? out_dependecies : (TNodeIndex*)alloca(sizeof(TNodeIndex) * connection_count);   // TODO: UGH!

    TNodeIndex dependency_count = 0;
    for (TConnectionIndex connection_index = 0; connection_index < node_description->m_AudioConnectionCount; ++connection_index)
    {
        bool is_unique = true;
        TNodeIndex output_node_index = node_description->m_AudioConnections[connection_index].m_OutputConnection.m_OutputNodeIndex;
        for (TConnectionIndex prev = 0; prev < dependency_count; ++prev)
        {
            if (dependencies[prev] == output_node_index)
            {
                is_unique = false;
                break;
            }
        }

        if (is_unique)
        {
            dependencies[dependency_count] = output_node_index;
            dependency_count += 1;
        }
    }

    for (TConnectionIndex connection_index = 0; connection_index < node_description->m_TriggerConnectionCount; ++connection_index)
    {
        bool is_unique = true;
        TNodeIndex output_node_index = node_description->m_TriggerConnections[connection_index].m_OutputConnection.m_OutputNodeIndex;
        for (TConnectionIndex prev = 0; prev < connection_index; ++prev)
        {
            if (out_dependecies[prev] == output_node_index)
            {
                is_unique = false;
                break;
            }
        }

        if (is_unique)
        {
            if (out_dependecies)
            {
                out_dependecies[dependency_count] = output_node_index;
            }
            dependency_count += 1;
        }
    }
    return dependency_count;
}

struct GraphProperties {
    TParameterIndex m_ParameterCount;
    TResourceIndex m_ResourceCount;
    TAudioInputIndex m_AudioInputCount;
    TAudioOutputIndex m_AudioOutputCount;
    uint32_t m_GeneratedAudioBufferCount;
    TTriggerCount m_TriggerInputCount;
    TTriggerCount m_TriggerOutputCount;
    TNodeIndex m_DependencyCount;
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
    TTriggerCount trigger_output_count = 0;
    TAudioInputIndex audio_input_count = 0;
    TAudioOutputIndex audio_output_count = 0;
    TNodeIndex dependency_count = 0;
    uint32_t generated_buffer_count = 0;
    TContextMemorySize context_memory_size = 0;

    for (TNodeIndex node_index = 0; node_index < graph_description->m_NodeCount; ++node_index)
    {
        const NodeDescription& node_description = graph_description->m_NodeDescriptions[node_index];
        NodeRuntimeDescription node_runtime_description;
        node_description.m_NodeStaticDescription.m_GetNodeRuntimeDescCallback(graph_runtime_settings, &node_runtime_description);
        context_memory_size += node_runtime_description.m_ContextMemorySize;
        parameter_count += node_description.m_NodeStaticDescription.m_ParameterCount;
        resource_count += node_description.m_NodeStaticDescription.m_ResourceCount;
        audio_input_count += node_description.m_NodeStaticDescription.m_AudioInputCount;
        audio_output_count += node_description.m_NodeStaticDescription.m_AudioOutputCount;
        trigger_input_count += node_description.m_NodeStaticDescription.m_TriggerInputCount > 0 ? 1 : 0;
        trigger_output_count += node_description.m_NodeStaticDescription.m_TriggerOutputCount;
        for (TAudioOutputIndex audio_output_index = 0; audio_output_index < node_description.m_NodeStaticDescription.m_AudioOutputCount; ++audio_output_index)
        {
            TAudioOutputIndex buffer_count = 0;
            if (!GetOutputChannelAllocationCount(graph_description, graph_runtime_settings, node_index, audio_output_index, &buffer_count))
            {
                return false;
            }
            generated_buffer_count += buffer_count;
        }

        dependency_count += GetDependencies(&node_description, 0x0);
    }

    graph_properties->m_ContextMemorySize = context_memory_size;
    graph_properties->m_ParameterCount = parameter_count;
    graph_properties->m_ResourceCount = resource_count;
    graph_properties->m_AudioInputCount = audio_input_count;
    graph_properties->m_AudioOutputCount = audio_output_count;
    graph_properties->m_GeneratedAudioBufferCount = generated_buffer_count;
    graph_properties->m_TriggerInputCount = trigger_input_count;
    graph_properties->m_TriggerOutputCount = trigger_output_count;
    graph_properties->m_DependencyCount = dependency_count;
    return true;
}

static TGraphSize GetGraphSize(
    const GraphDescription* graph_description,
    const GraphProperties* graph_properties)
{
    TGraphSize s = ALIGN_SIZE(sizeof(Graph), sizeof(void*)) +
        ALIGN_SIZE(sizeof(TParameter) * graph_properties->m_ParameterCount, sizeof(Resource*)) +
        ALIGN_SIZE(sizeof(Resource) * graph_properties->m_ResourceCount, sizeof(RenderCallback)) +
        ALIGN_SIZE(sizeof(Node) * graph_description->m_NodeCount, sizeof(float*)) +
        ALIGN_SIZE(sizeof(AudioOutput) * (graph_properties->m_AudioOutputCount + 1), sizeof(AudioOutput*)) +
        ALIGN_SIZE(sizeof(AudioInput) * graph_properties->m_AudioInputCount, sizeof(TTriggerInputIndex*)) +
        ALIGN_SIZE(sizeof(TriggerInput) * graph_properties->m_TriggerInputCount, 1) +
        ALIGN_SIZE(sizeof(TriggerOutput) * graph_properties->m_TriggerOutputCount, sizeof(TNodeIndex)) +
        ALIGN_SIZE(sizeof(TNodeIndex) * graph_properties->m_DependencyCount, sizeof(1));
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
    out_graph_size->m_TriggerBufferSize = graph_properties.m_TriggerInputCount * graph_runtime_settings->m_MaxTriggerEventCount * sizeof(TTriggerInputIndex);
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

static void RenderNode(TNodeIndex node_index, RenderJob* render_jobs, uint8_t* is_node_rendered)
{
    if (is_node_rendered[node_index] != 0)
    {
        return;
    }
    RenderJob* render_job = &render_jobs[node_index];
    for (TNodeIndex dependency_index = 0; dependency_index < render_job->m_DependencyCount; ++dependency_index)
    {
        RenderNode(render_job->m_Dependencies[dependency_index], render_jobs, is_node_rendered);
    }

    render_job->m_RenderCallback(render_job->m_Graph, render_job->m_Node, &render_job->m_RenderParameters);

    is_node_rendered[node_index] = 1;
}

void RenderGraph(HGraph graph, TFrameIndex frame_count)
{
    TNodeIndex node_count = graph->m_NodeCount;

    RenderJob* render_jobs = (RenderJob*)alloca(sizeof(RenderJob) * node_count);
    uint8_t* is_node_rendered = (uint8_t*)alloca(sizeof(uint8_t) * node_count);
    for (TNodeIndex node_index = 0; node_index < node_count; ++node_index)
    {
        is_node_rendered[node_index] = 0;
    }
    GetRenderJobs(graph, frame_count, render_jobs);
    for (TNodeIndex node_index = 0; node_index < node_count; ++node_index)
    {
        RenderNode(node_index, render_jobs, is_node_rendered);
    }
}

void GetRenderJobs(HGraph graph, TFrameIndex frame_count, RenderJob* out_render_jobs)
{
    graph->m_ScratchUsedCount = 0;
    Node* nodes = graph->m_Nodes;
    TNodeIndex node_count = graph->m_NodeCount;

    TNodeIndex i = 0;
    while (i < node_count)
    {
        Node* node = &nodes[i];
        RenderJob& render_job = out_render_jobs[i];
        render_job.m_RenderCallback = node->m_Render;
        render_job.m_Graph = graph;
        render_job.m_Node = node;
        render_job.m_DependencyCount = node->m_DependencyCount;
        render_job.m_Dependencies = &graph->m_Dependencies[node->m_DependencyOffset];
        RenderParameters& render_parameters = render_job.m_RenderParameters;
        render_parameters.m_AllocateAudioBuffer = AllocateAudioBuffer;
        render_parameters.m_FrameRate = graph->m_FrameRate;
        render_parameters.m_FrameCount = frame_count;

        render_parameters.m_AudioInputs = &graph->m_AudioInputs[node->m_AudioInputsOffset];
        render_parameters.m_AudioOutputs = &graph->m_AudioOutputs[node->m_AudioOutputsOffset];
        render_parameters.m_Parameters = &graph->m_Parameters[node->m_ParametersOffset];
        render_parameters.m_Resources = &graph->m_Resources[node->m_ResourcesOffset];
        render_parameters.m_TriggerInput = &graph->m_TriggerInputs[node->m_TriggerInputOffset];
        render_parameters.m_TriggerOutputs = &graph->m_TriggerOutputs[node->m_TriggerOutputOffset];
        render_parameters.m_ContextMemory = &graph->m_ContextMemory[node->m_ContextMemoryOffset];
        ++i;
    }
}

bool SetParameter(HGraph graph, TNodeIndex node_index, TParameterIndex parameter_index, TParameter value)
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

bool Trigger(HGraph graph, TNodeIndex node_index, TTriggerInputIndex trigger_index)
{
    if (node_index >= graph->m_NodeCount)
    {
        return false;
    }
    Node* node = &graph->m_Nodes[node_index];
    TriggerInput* trigger_input = &graph->m_TriggerInputs[node->m_TriggerInputOffset];
    if (graph->m_MaxTriggerEventCount == trigger_input->m_Count)
    {
        return false;
    }

    trigger_input->m_Buffer[trigger_input->m_Count++] = trigger_index;
    return true;
}

AudioOutput* GetAudioOutput(HGraph graph, TNodeIndex node_index, TAudioOutputIndex output_index)
{
    Node* node = &graph->m_Nodes[node_index];
    return &graph->m_AudioOutputs[node->m_AudioOutputsOffset + output_index];
}

static HNode MakeNode(
    HGraph graph,
    const NodeDescription* node_description,
    const NodeRuntimeDescription* node_runtime_description,
    TNodeIndex* node_offset,
    TAudioInputOffset* input_offset,
    TAudioOutputOffset* output_offset,
    TParameterOffset* parameters_offset,
    TResourceOffset* resources_offset,
    TTriggerOffset* triggers_input_offset,
    TTriggerOffset* triggers_output_offset,
    TNodeIndex* dependenceny_offset,
    TContextMemorySize* context_memory_offset)
{
    TNodeIndex node_index = *node_offset;
    (*node_offset) += 1;
    Node* node = &graph->m_Nodes[node_index];

    node->m_Render = node_runtime_description->m_RenderCallback;
    node->m_ParametersOffset = *parameters_offset;
    *parameters_offset += node_description->m_NodeStaticDescription.m_ParameterCount;
    node->m_AudioInputsOffset = *input_offset;
    *input_offset += node_description->m_NodeStaticDescription.m_AudioInputCount;
    node->m_AudioOutputsOffset = *output_offset;
    *output_offset += node_description->m_NodeStaticDescription.m_AudioOutputCount;
    node->m_ResourcesOffset = *resources_offset;
    *resources_offset += node_description->m_NodeStaticDescription.m_ResourceCount;
    node->m_TriggerInputOffset = *triggers_input_offset;
    *triggers_input_offset += node_description->m_NodeStaticDescription.m_TriggerInputCount > 0 ? 1 : 0;
    node->m_TriggerOutputOffset = *triggers_output_offset;
    *triggers_output_offset += node_description->m_NodeStaticDescription.m_TriggerOutputCount;
    node->m_DependencyOffset = *dependenceny_offset;

    for (TParameterIndex i = 0; i < node_description->m_NodeStaticDescription.m_ParameterCount; ++i)
    {
        const ParameterDescription* parameter_description = &node_description->m_NodeStaticDescription.m_ParameterDescriptions[i];
        graph->m_Parameters[node->m_ParametersOffset + i] = parameter_description->m_InitialValue;
    }

    node->m_DependencyCount = GetDependencies(node_description, &graph->m_Dependencies[node->m_DependencyOffset]);

    *dependenceny_offset += node->m_DependencyCount;

    graph->m_TriggerInputs[node_index].m_Buffer = &graph->m_Triggers[node_index * graph->m_MaxTriggerEventCount];
    graph->m_TriggerInputs[node_index].m_Count = 0;

    node->m_ContextMemoryOffset = *context_memory_offset;
    *context_memory_offset += node_runtime_description->m_ContextMemorySize;
    return node;
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
    graph->m_Triggers = (TTriggerInputIndex*)graph_buffers->m_TriggerBufferMem;
    graph->m_ScratchBuffer = (float*)graph_buffers->m_ScratchBufferMem;
    graph->m_ContextMemory = (uint8_t*)graph_buffers->m_ContextMem;

    uint8_t* ptr = (uint8_t*)graph_buffers->m_GraphMem;
    uint32_t offset = ALIGN_SIZE(sizeof(Graph), sizeof(void*));

    graph->m_Parameters = (TParameter*)&ptr[offset];
    offset += ALIGN_SIZE(sizeof(TParameter) * graph_properties.m_ParameterCount, sizeof(void*));

    graph->m_Resources = (Resource*)&ptr[offset];
    offset += ALIGN_SIZE(sizeof(Resource) * graph_properties.m_ResourceCount, sizeof(RenderCallback));

    graph->m_Nodes = (Node*)&ptr[offset];
    offset += ALIGN_SIZE(sizeof(Node) * graph_description->m_NodeCount, sizeof(AudioOutput*));
    
    graph->m_AudioInputs = (AudioInput*)&ptr[offset];
    offset += ALIGN_SIZE(sizeof(AudioInput) * graph_properties.m_AudioInputCount, sizeof(float*));

    graph->m_AudioOutputs = (AudioOutput*)&ptr[offset];
    offset += ALIGN_SIZE(sizeof(AudioOutput) * (graph_properties.m_AudioOutputCount + 1), sizeof(TTriggerInputIndex*));

    graph->m_TriggerInputs = (TriggerInput*)&ptr[offset];
    offset += ALIGN_SIZE(sizeof(TriggerInput) * (graph_properties.m_TriggerInputCount), sizeof(TNodeIndex));

    graph->m_TriggerOutputs = (TriggerOutput*)&ptr[offset];
    offset += ALIGN_SIZE(sizeof(TriggerOutput) * (graph_properties.m_TriggerOutputCount), sizeof(TNodeIndex));

    graph->m_Dependencies = (TNodeIndex*)&ptr[offset];
    offset += ALIGN_SIZE(sizeof(TNodeIndex) * (graph_properties.m_DependencyCount), sizeof(1));

    TNodeIndex node_offset = 0;
    TAudioInputOffset input_offset = 0;
    TAudioOutputOffset output_offset = 1; // Zero is reserved for unconnected inputs
    TParameterOffset parameters_offset = 0;
    TResourceOffset resources_offset = 0;
    TTriggerOffset triggers_input_offset = 0;
    TTriggerOffset triggers_output_offset = 0;
    TNodeIndex dependenceny_offset = 0;
    TContextMemorySize context_memory_offset = 0;

    // Each node need to know how many trigger connections it has!
    for (TNodeIndex node_index = 0; node_index < graph_description->m_NodeCount; ++node_index)
    {
        const NodeDescription& node_description = graph_description->m_NodeDescriptions[node_index];
        NodeRuntimeDescription node_runtime_description;
        node_description.m_NodeStaticDescription.m_GetNodeRuntimeDescCallback(graph_runtime_settings, &node_runtime_description);
        MakeNode(
            graph,
            &node_description,
            &node_runtime_description,
            &node_offset,
            &input_offset,
            &output_offset,
            &parameters_offset,
            &resources_offset,
            &triggers_input_offset,
            &triggers_output_offset,
            &dependenceny_offset,
            &context_memory_offset);
    }

    for (TNodeIndex node_index = 0; node_index < graph_description->m_NodeCount; ++node_index)
    {
        const NodeDescription& node_description = graph_description->m_NodeDescriptions[node_index];
        for (TConnectionIndex i = 0; i < node_description.m_AudioConnectionCount; ++i)
        {
            const NodeAudioConnection* node_connection = &node_description.m_AudioConnections[i];
            Node* input_node = &graph->m_Nodes[node_index];
            TAudioInputOffset graph_input_index = input_node->m_AudioInputsOffset + node_connection->m_InputIndex;
            if(graph->m_AudioInputs[graph_input_index].m_AudioOutput != 0x0)
            {
                return 0x0;
            }
            if (node_connection->m_OutputConnection.m_OutputNodeIndex == EXTERNAL_NODE_INDEX)
            {
                graph->m_AudioInputs[graph_input_index].m_AudioOutput = graph_description->m_ExternalAudioInputs[node_connection->m_OutputConnection.m_OutputIndex];
            }
            else
            {
                Node* output_node = &graph->m_Nodes[node_connection->m_OutputConnection.m_OutputNodeIndex];
                graph->m_AudioInputs[graph_input_index].m_AudioOutput = &graph->m_AudioOutputs[output_node->m_AudioOutputsOffset + node_connection->m_OutputConnection.m_OutputIndex];
            }
        }

        for (TConnectionIndex i = 0; i < node_description.m_TriggerConnectionCount; ++i)
        {
            const NodeTriggerConnection* node_connection = &node_description.m_TriggerConnections[i];
            Node* output_node = &graph->m_Nodes[node_connection->m_OutputConnection.m_OutputNodeIndex];
            TTriggerOffset graph_output_index = output_node->m_TriggerOutputOffset + node_connection->m_OutputConnection.m_OutputTriggerIndex;
            graph->m_TriggerOutputs[graph_output_index].m_InputNode = node_index;
            graph->m_TriggerOutputs[graph_output_index].m_Trigger = node_connection->m_InputTriggerIndex;
        }
    }

    for (TNodeIndex node_index = 0; node_index < graph_description->m_NodeCount; ++node_index)
    {
        Node* node = &graph->m_Nodes[node_index];
        AudioOutput* render_outputs = &graph->m_AudioOutputs[node->m_AudioOutputsOffset];

        const NodeDescription& node_description = graph_description->m_NodeDescriptions[node_index];
        for (TAudioOutputIndex j = 0; j < node_description.m_NodeStaticDescription.m_AudioOutputCount; ++j)
        {
            if (!GetOutputChannelCount(graph_description, graph_runtime_settings, node_index, j, &render_outputs[j].m_ChannelCount))
            {
                return 0x0;
            }
        }
    }

    for (TNodeIndex node_index = 0; node_index < graph_description->m_NodeCount; ++node_index)
    {
        Node* node = &graph->m_Nodes[node_index];
        const NodeDescription& node_description = graph_description->m_NodeDescriptions[node_index];
        NodeRuntimeDescription node_runtime_description;
        node_description.m_NodeStaticDescription.m_GetNodeRuntimeDescCallback(graph_runtime_settings, &node_runtime_description);
        if (node_runtime_description.m_InitCallback)
        {
            node_runtime_description.m_InitCallback(graph, node, graph_runtime_settings, &graph->m_ContextMemory[node->m_ContextMemoryOffset]);
        }
    }

    return graph;
}

}
