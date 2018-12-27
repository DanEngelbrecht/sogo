#include "sogo_nodes.h"
#include <memory.h>
#include <math.h>

namespace sogo {

///////////////////// SOGO SPLIT

static bool RenderSplit(HGraph graph, HNode node, const RenderParameters* render_parameters)
{
    RenderOutput* input_data = render_parameters->m_RenderInputs[0].m_RenderOutput;
    if (input_data->m_Buffer == 0x0)
    {
        render_parameters->m_RenderOutputs[0].m_Buffer = 0x0;
        render_parameters->m_RenderOutputs[1].m_Buffer = 0x0;
        return true;
    }
    render_parameters->m_RenderOutputs[1].m_Buffer = AllocateBuffer(graph, node, input_data->m_ChannelCount, render_parameters->m_FrameCount);
    if (render_parameters->m_RenderOutputs[1].m_Buffer == 0x0)
    {
        return false;
    }
    memcpy(render_parameters->m_RenderOutputs[1].m_Buffer, input_data->m_Buffer, sizeof(float) * input_data->m_ChannelCount * render_parameters->m_FrameCount);
    render_parameters->m_RenderOutputs[0].m_Buffer = input_data->m_Buffer;
    input_data->m_Buffer = 0x0;

    return true;
}

struct OutputDescription SplitNodeOutputDescriptions[2] =
{
    {OutputDescription::PASS_THROUGH, {0}},
    {OutputDescription::AS_INPUT, {0}}
};

const NodeDescription SplitNodeDescription =
{
    RenderSplit,
    0x0,
    SplitNodeOutputDescriptions,
    1,
    2,
    0,
    0
};


///////////////////// SOGO MERGE

static bool RenderMerge(TFrameCount frame_count, RenderOutput* input_data_1, RenderOutput* input_data_2)
{
    if (input_data_1->m_ChannelCount != input_data_2->m_ChannelCount)
    {
        // Mixed channel count not yet supported
        return false;
    }
    for (uint32_t sample = 0; sample < frame_count * input_data_1->m_ChannelCount; ++sample)
    {
        input_data_1->m_Buffer[sample] += input_data_2->m_Buffer[sample];
    }
    return true;
}

static bool RenderMerge(HGraph , HNode , const RenderParameters* render_parameters)
{
    if (render_parameters->m_RenderInputs[0].m_RenderOutput->m_Buffer == 0x0)
    {
        if (render_parameters->m_RenderInputs[1].m_RenderOutput->m_Buffer == 0x0)
        {
            render_parameters->m_RenderInputs[0].m_RenderOutput->m_Buffer = 0x0;
            return true;
        }
        else
        {
            render_parameters->m_RenderOutputs[0].m_Buffer = render_parameters->m_RenderInputs[1].m_RenderOutput->m_Buffer;
            render_parameters->m_RenderInputs[1].m_RenderOutput->m_Buffer = 0x0;
            return true;
        }
    }
    else if (render_parameters->m_RenderInputs[1].m_RenderOutput->m_Buffer == 0x0)
    {
        render_parameters->m_RenderOutputs[0].m_Buffer = render_parameters->m_RenderInputs[1].m_RenderOutput->m_Buffer;
        render_parameters->m_RenderInputs[1].m_RenderOutput->m_Buffer = 0x0;
        return true;
    }

    if (!RenderMerge(render_parameters->m_FrameCount, render_parameters->m_RenderInputs[1].m_RenderOutput, render_parameters->m_RenderInputs[1].m_RenderOutput))
    {
        return false;
    }
    render_parameters->m_RenderOutputs[0].m_Buffer = render_parameters->m_RenderInputs[1].m_RenderOutput->m_Buffer;
    render_parameters->m_RenderInputs[1].m_RenderOutput->m_Buffer = 0x0;
    return true;
}

struct OutputDescription MergeNodeOutputDescriptions[1] =
{
    {OutputDescription::PASS_THROUGH, {0}}
};

const NodeDescription MergeNodeDescription =
{
    RenderMerge,
    0x0,
    MergeNodeOutputDescriptions,
    2,
    1,
    0,
    0
};


///////////////////// SOGO GAIN

enum SOGO_GAIN_PARAMETERS
{
    SOGO_GAIN_PARAMETER_GAIN_INDEX,
    SOGO_GAIN_PARAMETER_FILTERED_GAIN_INDEX,
    SOGO_GAIN_PARAMETER_COUNT
};

static const ParameterDescription GainParameters[SOGO_GAIN_PARAMETER_COUNT] = {
    {"Gain", 1.f},
    {0x0, 1.f}
};

static bool GainFlat(float* io_buffer, TChannelCount channel_count, TFrameCount frame_count, float gain)
{
    switch (channel_count)
    {
        case 1:
            while (frame_count-- > 0)
            {
                *io_buffer++ *= gain;
            }
            return true;
        case 2:
            while (frame_count-- > 0)
            {
                *io_buffer++ *= gain;
                *io_buffer++ *= gain;
            }
            return true;
        default:
            return false;
    }
}

static bool GainRamp(float* io_buffer, TChannelCount channel_count, TFrameCount frame_count, float gain, float gain_step)
{
    switch (channel_count)
    {
        case 1:
            while (frame_count-- > 0)
            {
                *io_buffer++ *= gain;
                gain += gain_step;
            }
            return true;
        case 2:
            while (frame_count-- > 0)
            {
                *io_buffer++ *= gain;
                *io_buffer++ *= gain;
                gain += gain_step;
            }
            return true;
        default:
            return false;
    }
}

static bool RenderGain(TFrameCount frame_count, RenderOutput* output_data, float gain, float& filtered_gain)
{
    static const float max_gain_step_per_frame = 1.0f / 32;

    if (gain == filtered_gain)
    {
        if (gain < 0.001f)
        {
            output_data->m_Buffer = 0x0;
            return true;
        }
        return GainFlat(output_data->m_Buffer, output_data->m_ChannelCount, frame_count, gain);
    }

    TFrameCount step_count = (TFrameCount)(floor(fabs(gain - filtered_gain) / max_gain_step_per_frame));
    TFrameCount ramp_frames = (step_count < frame_count) ? step_count : frame_count;
    float gain_step = gain > filtered_gain ? max_gain_step_per_frame : -max_gain_step_per_frame;

    if (!GainRamp(output_data->m_Buffer, output_data->m_ChannelCount, ramp_frames, filtered_gain, gain_step))
    {
        return false;
    }
    filtered_gain = gain;
    return GainFlat(&output_data->m_Buffer[output_data->m_ChannelCount * ramp_frames], output_data->m_ChannelCount, frame_count - ramp_frames, gain);
}

static bool RenderGain(HGraph , HNode , const RenderParameters* render_parameters)
{
    RenderOutput* input_data = render_parameters->m_RenderInputs[0].m_RenderOutput;
    float gain = render_parameters->m_Parameters[SOGO_GAIN_PARAMETER_GAIN_INDEX];
    float filtered_gain = render_parameters->m_Parameters[SOGO_GAIN_PARAMETER_FILTERED_GAIN_INDEX];
    if (input_data->m_Buffer == 0x0)
    {
        // TODO: Hmm, we still want to filter the gain, or can we just hack it and set filtered_gain to gain?
        // Assuming each render call has enough frames, yes, otherwise no. Hack it for now.
        filtered_gain = gain;
        render_parameters->m_RenderOutputs[0].m_Buffer = 0x0;
        return true;
    }

    render_parameters->m_RenderOutputs[0].m_Buffer = input_data->m_Buffer;
    input_data->m_Buffer = 0x0;
    if (!RenderGain(render_parameters->m_FrameCount, &render_parameters->m_RenderOutputs[0], gain, filtered_gain))
    {
        return false;
    }
    render_parameters->m_Parameters[SOGO_GAIN_PARAMETER_FILTERED_GAIN_INDEX] = filtered_gain;
    return true;
}

struct OutputDescription GainNodeOutputDescriptions[1] =
{
    {OutputDescription::PASS_THROUGH, {0}}
};

const NodeDescription GainNodeDescription =
{
    RenderGain,
    GainParameters,
    GainNodeOutputDescriptions,
    1,
    1,
    0,
    2
};





///////////////////// SOGO SINE

enum SOGO_SINE_PARAMETERS
{
    SOGO_SINE_PARAMETER_FREQUENCY_INDEX,
    SOGO_SINE_PARAMETER_FILTERED_FREQUENCY_INDEX,
    SOGO_SINE_PARAMETER_FILTERED_VALUE_INDEX,
    SOGO_SINE_PARAMETER_COUNT
};

static const ParameterDescription SineParameters[SOGO_SINE_PARAMETER_COUNT] = {
    {"Frequency", 4000.0f},
    {0x0, 4000.0f},
    {0x0, 0.0f}
};

static bool RenderSine(HGraph graph, HNode node, const RenderParameters* render_parameters)
{
    float frequency = render_parameters->m_Parameters[SOGO_SINE_PARAMETER_FREQUENCY_INDEX];
    float filtered_frequency = render_parameters->m_Parameters[SOGO_SINE_PARAMETER_FILTERED_FREQUENCY_INDEX];
    float value = render_parameters->m_Parameters[SOGO_SINE_PARAMETER_FILTERED_VALUE_INDEX];

    filtered_frequency = ((frequency * 15) + filtered_frequency) / 16;
    float step = ((2.f * 3.141592654f) * filtered_frequency) / render_parameters->m_FrameRate;
    TFrameCount frame_count = render_parameters->m_FrameCount;
    render_parameters->m_RenderOutputs[0].m_Buffer = AllocateBuffer(graph, node, 1, frame_count);
    float* io_buffer = render_parameters->m_RenderOutputs[0].m_Buffer;
    while (frame_count-- > 0)
    {
        float frame = sinf(value);
        *io_buffer++ = frame;
        value += step;
        if (value > (2.f * 3.141592654f))
        {
            value -= (2.f * 3.141592654f);
        }
    }
    render_parameters->m_Parameters[SOGO_SINE_PARAMETER_FILTERED_FREQUENCY_INDEX] = filtered_frequency;
    render_parameters->m_Parameters[SOGO_SINE_PARAMETER_FILTERED_VALUE_INDEX] = value;
    return true;
}

struct OutputDescription SineNodeOutputDescriptions[1] =
{
    {OutputDescription::FIXED, {1}}
};

const NodeDescription SineNodeDescription =
{
    RenderSine,
    SineParameters,
    SineNodeOutputDescriptions,
    0,
    1,
    0,
    3
};




///////////////////// SOGO MAKE_STEREO

static bool RenderToStereo(HGraph graph, HNode node, const RenderParameters* render_parameters)
{
    RenderOutput* input_data = render_parameters->m_RenderInputs[0].m_RenderOutput;
    if (input_data == 0x0)
    {
        render_parameters->m_RenderOutputs[0].m_Buffer = 0x0;
        return true;
    }
    if (input_data->m_ChannelCount == 2)
    {
        render_parameters->m_RenderOutputs[0].m_Buffer = input_data->m_Buffer;
        render_parameters->m_RenderOutputs[0].m_ChannelCount = input_data->m_ChannelCount;
        input_data->m_Buffer = 0x0;
        return true;
    }
    if (input_data->m_ChannelCount != 1)
    {
        return false;
    }

    float* mono_input = input_data->m_Buffer;

    TFrameCount frame_count = render_parameters->m_FrameCount;
    float* stereo_output = AllocateBuffer(graph, node, 2, frame_count);
    if (stereo_output == 0)
    {
        return false;
    }

    render_parameters->m_RenderOutputs[0].m_Buffer = stereo_output;
    render_parameters->m_RenderOutputs[0].m_ChannelCount = 2;

    while (frame_count--)
    {
        *stereo_output++ = *mono_input;
        *stereo_output++ = *mono_input++;
    }

    return true;
}

struct OutputDescription ToStereoNodeOutputDescriptions[1] =
{
    {OutputDescription::FIXED, {2}}
};

const NodeDescription ToStereoNodeDescription =
{
    RenderToStereo,
    0x0,
    ToStereoNodeOutputDescriptions,
    1,
    1,
    0,
    0
};




#if 0

///////////////////// SOGO SUBGRAPH

// !!! Decided to *not* support sub-graphs, the rendering engine should only
// work with "flat" graphs to reduce complexity.
// Authoring tools should handle this, just focus on designing data structures so
// it is easy to add/remove parts of a graph (which may be grouped in authoring tool)

// Currently just takes first output of last node, need to improve!
bool MakeSubGraphOutputDescriptor(sogo::HGraph graph, OutputDescription& output_description);
bool MakeSubGraphNodeDescriptor(sogo::NodeDescription& out_node_descriptor, const sogo::OutputDescription* output_descriptors);

    Resource* CreateSubGraphResource(
        uint32_t frame_rate,
        uint32_t max_batch_size,
        const GraphDescription* graph_description);

    // TODO How do we handle dynamic number of output descriptors?
    bool MakeSubGraphNodeDescriptor(const GraphDescription* graph_description, Resource* resource, sogo::NodeDescription& out_node_descriptor);


// TODO
// 
//  Should we handle multiple inputs and outputs for a sub-graph?
//      YES: So we can make fex a mixer sub-graph
//  How do we do sub-graph inputs?
//      Is "External Inputs" that exists enough? They supply an output and which input to connect it to by setting input node to 'node-count'
//      YES, should work just fine
//
//      Can we make connections that mark the output as external by setting the output node to 'node-count'?
//      How will the render function know which and how many outputs to fetch from the sub-graph and apply to the outputs?
//      POSSIBLY: The sub-graph resource contains that information via MakeSubGraphResource which contains
//      Array of node index+output index that should be sent to output_render_buffers

// Using this we could allow for a UI where you add sub-graphs and any free input or output of a sub-graph
// can be connected to any other input/output *outside* of the sub-graph. Don't think it should be allowed to
// reconnect the sub-graphs internal connections just to avoid confusion.

static bool RenderSubGraph(HGraph graph, HNode node, const RenderParameters* render_parameters)
{
    if (resources[0] == 0x0)
    {
        return false;
    }
    HGraph sub_graph = (HGraph)resources[0]->m_Data;
    if (!RenderGraph(sub_graph, frame_count))
    {
        return false;
    }
    RenderOutput* render_output = GetOutput(sub_graph, sub_graph->m_NodeCount - 1, 0);
    output_render_buffers->m_Buffer = render_output->m_Buffer;
    render_output->m_Buffer = 0x0;
    return true;
}

bool MakeSubGraphOutputDescriptor(HGraph graph, OutputDescription& output_description)
{
    output_description.m_Mode = OutputDescription::FIXED;
    output_description.m_ChannelCount = GetOutput(graph, graph->m_NodeCount - 1, 0)->m_ChannelCount;
    return true;
}

bool MakeSubGraphNodeDescriptor(NodeDescription& out_node_descriptor, const OutputDescription* output_descriptors)
{
    out_node_descriptor.m_RenderCallback = RenderSubGraph;
    out_node_descriptor.m_Parameters = 0x0;
    out_node_descriptor.m_OutputDescriptions = output_descriptors;
    out_node_descriptor.m_InputCount = 0;
    out_node_descriptor.m_OutputCount = 1;
    out_node_descriptor.m_ResourceCount = 1;
    out_node_descriptor.m_ParameterCount = 0;
    return true;
}


// TODO: This is experimental, I'm not sure we should tuck in a sub-graph
// into this structure. More likely we build that on top of this.
// A bit sad that we need to manage connections etc outside but
// on the ofther hand the sub-graph won't need to handle the complexity
//
// We have ability to get output from a graph, we most likely need to
// provide ability to provide input if we want to build complex graphs

struct SubGraphResource
{
    HGraph m_Graph;
    uint16_t m_OutputNodeIndex;
    uint16_t m_OutputIndex;
};

static bool RenderSubGraph(HGraph graph, HNode node, const RenderParameters* render_parameters)
{
    SubGraphResource* sub_graph_resource = (SubGraphResource*)resources[0].m_Data;
    HGraph sub_graph = sub_graph_resource->m_Graph;
    if (!RenderGraph(sub_graph, frame_count))
    {
        return false;
    }

    RenderOutput* sub_graph_output = GetOutput(sub_graph, sub_graph_resource->m_OutputNodeIndex, sub_graph_resource->m_OutputIndex);
    if (sub_graph_output == 0x0)
    {
        return false;
    }

    render_outputs->m_Buffer = sub_graph_output->m_Buffer;
    render_outputs->m_ChannelCount = sub_graph_output->m_ChannelCount;

    return true;
}

struct OutputDescription SubGraphOutputDescriptions[1] =
{
    {OutputDescription::FIXED, {2}}
};

struct NodeDescription SubGraphNodeDescription =
{
    RenderSubGraph,
    0x0,
    SubGraphOutputDescriptions,
    0,
    1,
    1,
    0
};
#endif








#if 0

static const uint32_t SOGO_MIXER_CHANNEL_COUNT = 8;
static const char* SOGO_MIXER_PARAMETER_MASTER_GAIN_NAME = "SogoMixer::MasterGain";
static const uint32_t SOGO_MIXER_PARAMETER_MASTER_GAIN_INDEX = 0;
static const uint32_t SOGO_MIXER_PARAMETER_FILTERED_MASTER_GAIN_INDEX = 1;

static const uint32_t SOGO_MIXER_PARAMETER_CHANNEL_GAIN_INDEX_START = 0;
static const uint32_t SOGO_MIXER_PARAMETER_CHANNEL_FILTERED_GAIN_INDEX_START = SOGO_MIXER_PARAMETER_CHANNEL_GAIN_INDEX_START + SOGO_MIXER_CHANNEL_COUNT;

static bool RenderMixer(HGraph graph, HNode node, const RenderParameters* render_parameters)
{
    RenderData* input_data[SOGO_MIXER_CHANNEL_COUNT] = 
    {
        &graph->m_RenderData[input_indexes[0]],
        &graph->m_RenderData[input_indexes[1]],
        &graph->m_RenderData[input_indexes[2]],
        &graph->m_RenderData[input_indexes[3]],
        &graph->m_RenderData[input_indexes[4]],
        &graph->m_RenderData[input_indexes[5]],
        &graph->m_RenderData[input_indexes[6]],
        &graph->m_RenderData[input_indexes[7]]
    };
    RenderData* output_data = &graph->m_RenderData[output_indexes[0]];

    for (uint32_t i = 0; i < SOGO_MIXER_CHANNEL_COUNT; ++i)
    {
        float gain = graph->m_Parameters[node.m_Parameters[SOGO_MIXER_PARAMETER_CHANNEL_GAIN_INDEX_START + i]];
        float& filtered_gain = graph->m_Parameters[node.m_Parameters[SOGO_MIXER_PARAMETER_CHANNEL_FILTERED_GAIN_INDEX_START + 1]];
        if (!RenderGain(frame_count, input_data[i], gain, filtered_gain))
        {
            return false;
        }
    }

    output_data->m_Buffer = input_data[0]->m_Buffer;
    output_data->m_ChannelCount = input_data[0]->m_ChannelCount;
    input_data[0]->m_Buffer = 0x0;

    for (uint32_t i = 1; i < SOGO_MIXER_CHANNEL_COUNT; ++i)
    {
        if (!RenderMerge(frame_count, output_data, input_data[i]))
        {
            return false;
        }
    }

    float gain = graph->m_Parameters[node.m_Parameters[SOGO_MIXER_PARAMETER_MASTER_GAIN_INDEX]];
    float& filtered_gain = graph->m_Parameters[node.m_Parameters[SOGO_MIXER_PARAMETER_FILTERED_MASTER_GAIN_INDEX]];

    if (!RenderGain(frame_count, output_data, gain, filtered_gain))
    {
        return false;
    }
    return true;
}

TNodeID MakeSogoMixer(HGraph graph)
{
    TNodeID index = graph->m_Nodes.Size();
    graph->m_Nodes.SetSize(index + 1);
    Node& node = graph->m_Nodes[index];
    node.m_Parameters[SOGO_MIXER_PARAMETER_MASTER_GAIN_INDEX] = RegisterNamedParameter(graph, MakeParameterHash(index, SOGO_MIXER_PARAMETER_MASTER_GAIN_NAME), 1.0f);
    node.m_Parameters[SOGO_MIXER_PARAMETER_FILTERED_MASTER_GAIN_INDEX] = RegisterAnonymousParameter(graph, 0.0f);

    for (uint32_t i = 0; i < SOGO_MIXER_CHANNEL_COUNT; ++i)
    {
        node.m_Parameters[SOGO_MIXER_PARAMETER_CHANNEL_GAIN_INDEX_START + i] = RegisterAnonymousParameter(graph, 0.0f);
        node.m_Parameters[SOGO_MIXER_PARAMETER_CHANNEL_FILTERED_GAIN_INDEX_START + i] = RegisterAnonymousParameter(graph, 0.0f);
        node.m_Inputs[i] = inputs[i];
    }

    node.m_Render = RenderMixer;
    node.m_InputCount = SOGO_MIXER_CHANNEL_COUNT;
    node.m_OutputCount = 1;
    return index;
}

#endif
}