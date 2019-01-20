#pragma once

#include <stdint.h>

namespace sogo
{
    // Index is offset inside parent struct - index of node in graph, index of audio output in node etc
    // Count is how many of a something - number of triggers queued, number of frames to render
    // Offset is how far into a global array something is - offset to first audio input of a node in the graphs audio input array

    typedef uint16_t    TNodeIndex;
    typedef uint8_t     TParameterIndex;
    typedef uint8_t     TTriggerInputIndex;
    typedef uint8_t     TTriggerOutputIndex;
    typedef uint16_t    TTriggerCount;
    typedef uint8_t     TResourceIndex;
    typedef uint8_t     TAudioInputIndex;
    typedef uint8_t     TAudioOutputIndex;
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

    struct TriggerInput
    {
        TTriggerInputIndex* m_Buffer;
        TTriggerCount       m_Count;
    };

    struct TriggerOutput
    {
        TNodeIndex          m_Node;
        TTriggerInputIndex  m_Trigger;
    };

    struct Resource
    {
        void*       m_Data;
        uint32_t    m_Size;
    };

    typedef struct Graph* HGraph;
    typedef struct Node* HNode;

    typedef float* (*AllocateAudioBufferFunc)(HGraph graph, HNode node, TChannelIndex channel_count, TFrameIndex frame_count);

    // TODO: Need to sort out naming - description vs properties!
    struct GraphRuntimeSettings
    {
        TFrameRate m_FrameRate;
        TFrameIndex m_MaxBatchSize;
        TTriggerCount m_MaxTriggerEventCount;
    };

    struct RenderParameters
    {
        void*                   m_ContextMemory;
        AllocateAudioBufferFunc m_AllocateAudioBuffer;
        TFrameRate              m_FrameRate;
        TFrameIndex             m_FrameCount;
        AudioInput*             m_AudioInputs;
        AudioOutput*            m_AudioOutputs;
        float*                  m_Parameters;
        Resource*               m_Resources;
        TriggerInput*           m_TriggerInput;
        TriggerOutput*          m_TriggerOutputs;
    };

    struct NodeProperties
    {
        TContextMemorySize m_ContextMemorySize;
    };

    struct NodeDescription;

    typedef void (*GetNodePropertiesCallback)(const GraphRuntimeSettings* graph_runtime_settings, NodeProperties* out_node_properties);
    typedef void (*SetupNodeCallback)(const GraphRuntimeSettings* graph_runtime_settings, const NodeDescription* node_description, void* context_memory);
    typedef void (*RenderNodeCallback)(HGraph graph, HNode node, const RenderParameters* render_parameters);

    struct ParameterDescription
    {
        const char*     m_ParameterName;
        float           m_InitialValue;
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
            TAudioInputIndex    m_InputIndex;      // AS_INPUT
        };
    };

    struct NodeDescription
    {
        GetNodePropertiesCallback       m_GetNodePropertiesCallback;
        SetupNodeCallback               m_SetupNodeCallback;
        RenderNodeCallback              m_RenderCallback;
        const ParameterDescription*     m_Parameters;
        const AudioOutputDescription*   m_AudioOutputDescriptions;
        const TriggerDescription*       m_Triggers;
        TAudioInputIndex                m_AudioInputCount;
        TAudioOutputIndex               m_AudioOutputCount;
        TResourceIndex                  m_ResourceCount;
        TParameterIndex                 m_ParameterCount;
        TTriggerInputIndex              m_TriggerInputCount;
        TTriggerOutputIndex             m_TriggerOutputCount;
    };

    static const TNodeIndex EXTERNAL_NODE_INDEX = (TNodeIndex)-1;

    struct NodeAudioConnection
    {
        TNodeIndex          m_OutputNodeIndex;
        TAudioOutputIndex   m_OutputIndex;
        TNodeIndex          m_InputNodeIndex;
        TAudioInputIndex    m_InputIndex;
    };

    struct NodeTriggerConnection
    {
        TNodeIndex          m_OutputNodeIndex;
        TTriggerOutputIndex m_OutputTriggerIndex;
        TNodeIndex          m_InputNodeIndex;
        TTriggerInputIndex  m_InputTriggerIndex;
    };

    struct GraphDescription
    {
        TNodeIndex                      m_NodeCount;
        const NodeDescription**         m_NodeDescriptions;
        TConnectionIndex                m_AudioConnectionCount;
        const NodeAudioConnection*      m_NodeAudioConnections;
        AudioOutput**                   m_ExternalAudioInputs;
        TConnectionIndex                m_TriggerConnectionCount;
        const NodeTriggerConnection*    m_NodeTriggerConnections;
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
    AudioOutput* GetOutput(HGraph graph, TNodeIndex node_index, TAudioOutputIndex output_index);

    bool SetParameter(HGraph graph, TNodeIndex node_index, TParameterIndex parameter_index, float value);
    bool Trigger(HGraph graph, TNodeIndex node_index, TTriggerInputIndex trigger_index);
    bool SetResource(HGraph graph, TNodeIndex node_index, TResourceIndex resource_index, const Resource* resource);

    void RenderGraph(HGraph graph, TFrameIndex frame_count);
    AudioOutput* GetAudioOutput(HGraph graph, TNodeIndex node_index, TAudioOutputIndex output_index);
}
