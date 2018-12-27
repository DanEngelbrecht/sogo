#pragma once

#include <stdint.h>

namespace sogo
{
    typedef uint16_t TNodeIndex;
    typedef uint16_t TParameterIndex;
    typedef uint16_t TResourceIndex;
    typedef uint16_t TInputIndex;
    typedef uint16_t TOutputIndex;
    typedef uint16_t TChannelCount;
    typedef uint16_t TConnectionIndex;
    typedef uint32_t TParameterNameHash;
    typedef uint32_t TFrameCount;
    typedef uint32_t TFrameRate;

    struct RenderOutput
    {
        float*          m_Buffer;
        TChannelCount   m_ChannelCount;
    };

    struct RenderInput
    {
        RenderOutput*   m_RenderOutput;
    };

    struct Resource
    {
        void*           m_Data;
        uint32_t        m_Size;
    };

    typedef struct Graph* HGraph;
    typedef struct Node* HNode;

    typedef float* (*AllocateBufferFunc)(HGraph graph, HNode node, TChannelCount channel_count, TFrameCount frame_count);

    struct RenderParameters
    {
        AllocateBufferFunc  m_AllocateBuffer;
        TFrameRate          m_FrameRate;
        TFrameCount         m_FrameCount;
        RenderInput*        m_RenderInputs;
        RenderOutput*       m_RenderOutputs;
        float*              m_Parameters;
        Resource**          m_Resources;
    };

    typedef bool (*RenderCallback)(HGraph graph, HNode node, const RenderParameters* render_parameters);

    struct ParameterDescription
    {
        const char*     m_ParameterName;
        float           m_InitialValue;
    };

    struct OutputDescription
    {
        enum AllocationMode
        {
            PASS_THROUGH,   // Reuse buffer from Input[m_InputIndex]
            FIXED,          // Allocates a buffer with m_ChannelCount channels
            AS_INPUT        // Allocates a buffer with with same channel count as Input[m_InputIndex] 
        };
        uint16_t m_Mode;
        union {
            TChannelCount   m_ChannelCount;    // FIXED
            TInputIndex     m_InputIndex;      // AS_INPUT
        };
    };

    struct NodeDescription
    {
        RenderCallback              m_RenderCallback;
        const ParameterDescription* m_Parameters;
        const OutputDescription*    m_OutputDescriptions;
        TInputIndex                 m_InputCount;
        TOutputIndex                m_OutputCount;
        TResourceIndex              m_ResourceCount;
        TParameterIndex             m_ParameterCount;
    };

    static const TNodeIndex EXTERNAL_NODE_INDEX = (TNodeIndex)-1;

    struct NodeConnection
    {
        TNodeIndex      m_OutputNodeIndex;
        TOutputIndex    m_OutputIndex;
        TNodeIndex      m_InputNodeIndex;
        TInputIndex     m_InputIndex;
    };

    struct GraphDescription
    {
        TNodeIndex              m_NodeCount;
        const NodeDescription** m_NodeDescriptions;
        TConnectionIndex        m_ConnectionCount;
        const NodeConnection*   m_NodeConnections;
        RenderOutput**          m_ExternalInputs;
    };

    size_t GetGraphSize(TFrameCount max_batch_size, const GraphDescription* graph_description);
    HGraph CreateGraph(void* mem, TFrameRate frame_rate, TFrameCount max_batch_size, const GraphDescription* graph_description);
    void DisposeGraph(HGraph graph);

    TParameterNameHash MakeParameterHash(TNodeIndex node_index, const char* parameter_name);
    bool SetParameter(HGraph graph, TParameterNameHash parameter_hash, float value);

    bool SetResource(HGraph graph, TNodeIndex node_index, TResourceIndex resource_index, Resource* resource);

    bool RenderGraph(HGraph graph, TFrameCount frame_count);
    RenderOutput* GetOutput(HGraph graph, TNodeIndex node_index, TOutputIndex output_index);
}
