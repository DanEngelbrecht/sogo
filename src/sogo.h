#pragma once

#include <stdint.h>

namespace sogo
{
    // Index is offset inside parent struct - index of node in graph, index of audio output in node etc
    // Count is how many of a something - number of triggers queued, number of frames to render
    // Offset is how far into a global array something is - offset to first audio input of a node in the graphs audio input array

    typedef uint16_t    TNodeIndex;
    typedef int16_t     TNodeOffset;
    typedef uint8_t     TParameterIndex;
    typedef uint8_t     TTriggerSocketIndex;
    typedef uint16_t    TTriggerCount;
    typedef uint8_t     TResourceIndex;
    typedef uint8_t     TAudioSocketIndex;
    typedef uint8_t     TChannelIndex;
    typedef uint8_t     TConnectionIndex;
    typedef uint32_t    TFrameIndex;
    typedef uint32_t    TSampleIndex;
    typedef uint32_t    TFrameRate;
    typedef uint32_t    TContextMemorySize;
    typedef uint32_t    TGraphSize;
    typedef uint32_t    TTriggerBufferSize;
    typedef uint32_t    TScratchBufferSize;
    typedef uint32_t    TContextMemorySize;

    struct AudioOutput
    {
        float*          m_Buffer;
        TChannelIndex   m_ChannelCount;
    };

    struct AudioInput
    {
        AudioOutput*    m_AudioOutput;
    };

    struct TriggerOutput
    {
        TNodeIndex          m_InputNode;
        TTriggerSocketIndex  m_Trigger;
    };

    struct TriggerInput
    {
        TTriggerSocketIndex* m_Buffer;
        TTriggerCount       m_Count;
    };

    struct Resource
    {
        void*       m_Data;
        uint32_t    m_Size;
    };

    typedef struct Graph* HGraph;
    typedef struct Node* HNode;

    typedef float* (*AllocateAudioBufferFunc)(HGraph graph, HNode node, TChannelIndex channel_count, TFrameIndex frame_count);

    struct GraphRuntimeSettings
    {
        TFrameRate m_FrameRate;
        TFrameIndex m_MaxBatchSize;
        TTriggerCount m_MaxTriggerEventCount;
    };

    union TParameter
    {
        float m_Float;
        int32_t m_Int;
    };

    struct RenderParameters;
    typedef void (*RenderCallback)(HGraph graph, HNode node, const RenderParameters* render_parameters);

    struct RenderParameters
    {
        void*                   m_ContextMemory;
        AllocateAudioBufferFunc m_AllocateAudioBuffer;
        TFrameRate              m_FrameRate;
        TFrameIndex             m_FrameCount;
        AudioInput*             m_AudioInputs;
        AudioOutput*            m_AudioOutputs;
        TParameter*             m_Parameters;
        Resource*               m_Resources;
        TriggerInput*           m_TriggerInput;
        TriggerOutput*          m_TriggerOutputs;
    };

    struct ParameterDescription
    {
        const char*     m_ParameterName;
        TParameter      m_InitialValue;
    };

    struct TriggerDescription
    {
        const char*     m_TriggerName;
    };

    struct AudioOutputDescription
    {
        enum AllocationMode
        {
            PASS_THROUGH,   // Reuse buffer from Input[m_InputIndex]
            FIXED,          // Allocates a buffer with m_ChannelCount channels
            AS_INPUT        // Allocates a buffer with with same channel count as Input[m_InputIndex] 
        };
        uint16_t m_Mode;
        union {
            TChannelIndex       m_ChannelCount;    // FIXED
            TAudioSocketIndex    m_InputIndex;      // AS_INPUT
        };
    };

    typedef void (*InitCallback)(HGraph graph, HNode node, const GraphRuntimeSettings* graph_runtime_settings, void* context_memory);

    struct NodeRuntimeDescription
    {
        InitCallback                    m_InitCallback;
        RenderCallback                  m_RenderCallback;
        TContextMemorySize              m_ContextMemorySize;
    };

    typedef void (*GetNodeRuntimeDescription)(const GraphRuntimeSettings* graph_runtime_settings, NodeRuntimeDescription* out_node_runtime_desc);

    struct NodeStaticDescription
    {
        GetNodeRuntimeDescription       m_GetNodeRuntimeDescCallback;
        const ParameterDescription*     m_ParameterDescriptions;
        const AudioOutputDescription*   m_AudioOutputDescriptions;
        const TriggerDescription*       m_Triggers;
        TAudioSocketIndex                m_AudioInputCount;
        TAudioSocketIndex               m_AudioOutputCount;
        TResourceIndex                  m_ResourceCount;
        TParameterIndex                 m_ParameterCount;
        TTriggerSocketIndex              m_TriggerInputCount;
        TTriggerSocketIndex             m_TriggerOutputCount;
    };

    static const TNodeOffset EXTERNAL_NODE_OFFSET = 0;

    struct NodeAudioOutputConnection
    {
        TNodeOffset         m_OutputOffset;
        TAudioSocketIndex   m_OutputIndex;
    };

    struct NodeAudioConnection
    {
        TAudioSocketIndex           m_InputIndex;
        NodeAudioOutputConnection   m_OutputConnection;
    };

    struct NodeTriggerOutputConnection
    {
        TNodeOffset         m_OutputOffset;
        TTriggerSocketIndex m_OutputTriggerIndex;
    };

    struct NodeTriggerConnection
    {
        TTriggerSocketIndex         m_InputTriggerIndex;
        NodeTriggerOutputConnection m_OutputConnection;
    };

    struct NodeDescription
    {
        NodeStaticDescription           m_NodeStaticDescription;
        TConnectionIndex                m_AudioConnectionCount;
        const NodeAudioConnection*      m_AudioConnections;
        TConnectionIndex                m_TriggerConnectionCount;
        const NodeTriggerConnection*    m_TriggerConnections;
    };

    struct GraphDescription
    {
        TNodeIndex                      m_NodeCount;
        const NodeDescription*          m_NodeDescriptions;
        AudioOutput**                   m_ExternalAudioInputs;
    };

    struct GraphSize
    {
        TGraphSize m_GraphSize;
        TScratchBufferSize m_ScratchBufferSize;
        TTriggerBufferSize m_TriggerBufferSize;
        TContextMemorySize m_ContextMemorySize;
    };

    struct GraphBuffers
    {
        void* m_GraphMem;           // Align to float
        void* m_ScratchBufferMem;   // Align to float
        void* m_TriggerBufferMem;   // Align to TTriggerIndex
        void* m_ContextMem;         // No need to align, 
    };

    bool GetGraphSize(const GraphDescription* graph_description, const GraphRuntimeSettings* graph_runtime_settings, GraphSize* out_graph_size);
    HGraph CreateGraph(const GraphDescription* graph_description, const GraphRuntimeSettings* graph_runtime_settings, const GraphBuffers* graph_buffers);
    AudioOutput* GetOutput(HGraph graph, TNodeIndex node_index, TAudioSocketIndex output_index);

    struct RenderJob
    {
        RenderParameters        m_RenderParameters;
        RenderCallback          m_RenderCallback;
        HGraph                  m_Graph;
        HNode                   m_Node;
        TNodeIndex              m_DependencyCount;
        TNodeIndex*             m_Dependencies;
    };

    void GetRenderJobs(HGraph graph, TFrameIndex frame_count, RenderJob* out_render_jobs);

    bool SetParameter(HGraph graph, TNodeIndex node_index, TParameterIndex parameter_index, TParameter value);
    bool Trigger(HGraph graph, TNodeIndex node_index, TTriggerSocketIndex trigger_index);
    bool SetResource(HGraph graph, TNodeIndex node_index, TResourceIndex resource_index, const Resource* resource);

    void RenderGraph(HGraph graph, TFrameIndex frame_count);
    AudioOutput* GetAudioOutput(HGraph graph, TNodeIndex node_index, TAudioSocketIndex output_index);
}
