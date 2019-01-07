#pragma once

#include <stdint.h>

namespace sogo
{
    typedef uint16_t TNodeIndex;
    typedef uint16_t TParameterIndex;
    typedef uint8_t  TTriggerIndex;
    typedef uint16_t TTriggerCount;
    typedef uint16_t TResourceIndex;
    typedef uint16_t TAudioInputIndex;
    typedef uint16_t TAudioOutputIndex;
    typedef uint16_t TChannelIndex;
    typedef uint16_t TConnectionIndex;
    typedef uint32_t TFrameIndex;
    typedef uint32_t TSampleIndex;
    typedef uint32_t TFrameRate;

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
        TTriggerIndex*  m_Buffer;
        TTriggerCount   m_Count;
    };

//    struct TriggerOutput
//    {
//        TriggerInput* m_TriggerInput;
//        TTriggerIndex m_TriggerIndex;
//    };

    struct Resource
    {
        void*       m_Data;
        uint32_t    m_Size;
    };

    typedef struct Graph* HGraph;
    typedef struct Node* HNode;

    typedef float* (*AllocateAudioBufferFunc)(HGraph graph, HNode node, TChannelIndex channel_count, TFrameIndex frame_count);

    // TODO: Need to sort out naming - description vs properties!
    struct GraphProperties
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
        Resource**              m_Resources;
        TriggerInput*           m_TriggerInput;
//        TTriggerOutput*         m_TriggerOutputs;
    };

    struct NodeProperties
    {
        size_t m_ContextMemorySize;
    };

    typedef void (*GetNodePropertiesCallback)(const GraphProperties* graph_properties, const NodeDescription* node_description);
    typedef void (*SetupNodeCallback)(const GraphProperties* graph_properties, const NodeDescription* node_description, void* context_memory);
    typedef bool (*RenderNodeCallback)(HGraph graph, HNode node, const RenderParameters* render_parameters);

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
        TTriggerCount                   m_TriggerCount;
    };

    static const TNodeIndex EXTERNAL_NODE_INDEX = (TNodeIndex)-1;

    struct NodeConnection
    {
        TNodeIndex          m_OutputNodeIndex;
        TAudioOutputIndex   m_OutputIndex;
        TNodeIndex          m_InputNodeIndex;
        TAudioInputIndex    m_InputIndex;
    };

    struct GraphDescription
    {
        const GraphProperties*  m_GraphProperties;
        TNodeIndex              m_NodeCount;
        const NodeDescription** m_NodeDescriptions;
        TConnectionIndex        m_ConnectionCount;
        const NodeConnection*   m_NodeConnections;
        AudioOutput**           m_ExternalAudioInputs;
    };

    struct GraphSize
    {
        size_t m_GraphSize;
        size_t m_TriggerBufferSize;
        size_t m_ScratchBufferSize;
        size_t m_ContextMemorySize;
    };

    struct GraphBuffers
    {
        void* m_GraphMem;           // Align to float
        void* m_ScratchBufferMem;   // Align to float
        void* m_TriggerBufferMem;   // Align to TTriggerIndex
        void* m_ContextMem;         // No need to align, 
    };

    bool GetGraphSize(const GraphDescription* graph_description, GraphSize& out_graph_size);
    HGraph CreateGraph(const GraphDescription* graph_description, const GraphBuffers* graph_buffers);

    bool SetParameter(HGraph graph, TNodeIndex node_index, TParameterIndex parameter_index, float value);
    bool Trigger(HGraph graph, TNodeIndex node_index, TTriggerIndex trigger_index);
    bool SetResource(HGraph graph, TNodeIndex node_index, TResourceIndex resource_index, Resource* resource);

    bool RenderGraph(HGraph graph, TFrameIndex frame_count);
    AudioOutput* GetAudioOutput(HGraph graph, TNodeIndex node_index, TAudioOutputIndex output_index);
}
