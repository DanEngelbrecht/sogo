#pragma once

#include <stdint.h>

namespace sogo
{
    typedef uint16_t TNodeIndex;
    typedef uint16_t TParameterIndex;
    typedef uint16_t TTriggerIndex;
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
        AudioOutput*   m_AudioOutput;
    };

    struct Resource
    {
        void*       m_Data;
        uint32_t    m_Size;
    };

    typedef struct Graph* HGraph;
    typedef struct Node* HNode;

    typedef float* (*AllocateAudioBufferFunc)(HGraph graph, HNode node, TChannelIndex channel_count, TFrameIndex frame_count);

    struct RenderParameters
    {
        AllocateAudioBufferFunc m_AllocateAudioBuffer;
        TFrameRate              m_FrameRate;
        TFrameIndex             m_FrameCount;
        AudioInput*             m_AudioInputs;
        AudioOutput*            m_AudioOutputs;
        float*                  m_Parameters;
        Resource**              m_Resources;
        uint8_t*                m_Triggers;
    };

    typedef bool (*RenderCallback)(HGraph graph, HNode node, const RenderParameters* render_parameters);

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
        RenderCallback                  m_RenderCallback;
        const ParameterDescription*     m_Parameters;
        const AudioOutputDescription*   m_AudioOutputDescriptions;
        const TriggerDescription*       m_Triggers;
        TAudioInputIndex                m_AudioInputCount;
        TAudioOutputIndex               m_AudioOutputCount;
        TResourceIndex                  m_ResourceCount;
        TParameterIndex                 m_ParameterCount;
        TTriggerIndex                   m_TriggerCount;
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
        TNodeIndex              m_NodeCount;
        const NodeDescription** m_NodeDescriptions;
        TConnectionIndex        m_ConnectionCount;
        const NodeConnection*   m_NodeConnections;
        AudioOutput**           m_ExternalAudioInputs;
    };

    bool GetGraphSize(TFrameIndex max_batch_size, const GraphDescription* graph_description, size_t& out_graph_size, TSampleIndex& out_scratch_buffer_sample_count);
    HGraph CreateGraph(void* graph_mem, float* scratch_buffer, TFrameRate frame_rate, TFrameIndex max_batch_size, const GraphDescription* graph_description);

    bool SetParameter(HGraph graph, TNodeIndex node_index, TParameterIndex parameter_index, float value);
    bool Trigger(HGraph graph, TNodeIndex node_index, TTriggerIndex trigger_index);
    bool SetResource(HGraph graph, TNodeIndex node_index, TResourceIndex resource_index, Resource* resource);

    bool RenderGraph(HGraph graph, TFrameIndex frame_count);
    AudioOutput* GetAudioOutput(HGraph graph, TNodeIndex node_index, TAudioOutputIndex output_index);
}
