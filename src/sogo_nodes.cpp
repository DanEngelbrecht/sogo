#include "sogo_nodes.h"
#include <math.h>

namespace sogo {

///////////////////// SOGO SPLIT

static const TInputIndex SPLIT_INPUT_COUNT = 1;
static const TOutputIndex SPLIT_OUTPUT_COUNT = 2;
static const TResourceIndex SPLIT_RESOURCE_COUNT = 0;
static const TParameterIndex SPLIT_PARAMETER_COUNT = 0;
static const TTriggerIndex SPLIT_TRIGGER_COUNT = 0;

static bool RenderSplit(HGraph graph, HNode node, const RenderParameters* render_parameters)
{
    RenderOutput* input_data = render_parameters->m_RenderInputs[0].m_RenderOutput;
    if (input_data->m_Buffer == 0x0)
    {
        render_parameters->m_RenderOutputs[0].m_Buffer = 0x0;
        render_parameters->m_RenderOutputs[1].m_Buffer = 0x0;
        return true;
    }
    render_parameters->m_RenderOutputs[1].m_Buffer = render_parameters->m_AllocateBuffer(graph, node, input_data->m_ChannelCount, render_parameters->m_FrameCount);
    if (render_parameters->m_RenderOutputs[1].m_Buffer == 0x0)
    {
        return false;
    }

    const float* output1 = input_data->m_Buffer;
    float* output2 = render_parameters->m_RenderOutputs[1].m_Buffer;
    TSampleIndex sample_count = render_parameters->m_FrameCount * input_data->m_ChannelCount;
    while (sample_count--)
    {
        *output2++ = *output1++;
    }

    render_parameters->m_RenderOutputs[0].m_Buffer = input_data->m_Buffer;
    input_data->m_Buffer = 0x0;

    return true;
}

struct OutputDescription SplitNodeOutputDescriptions[SPLIT_OUTPUT_COUNT] =
{
    {OutputDescription::PASS_THROUGH, {0}},
    {OutputDescription::AS_INPUT, {0}}
};

const NodeDescription SplitNodeDescription =
{
    RenderSplit,
    0x0,
    SplitNodeOutputDescriptions,
    0x0,
    SPLIT_INPUT_COUNT,
    SPLIT_OUTPUT_COUNT,
    SPLIT_RESOURCE_COUNT,
    SPLIT_PARAMETER_COUNT,
    SPLIT_TRIGGER_COUNT
};


///////////////////// SOGO MERGE

enum SOGO_MERGE_PARAMETERS
{
    SOGO_MERGE_PARAMETER_COUNT
};

enum SOGO_MERGE_RESOURCES
{
    SOGO_MERGE_RESOURCE_COUNT
};

enum SOGO_MERGE_TRIGGERS
{
    SOGO_MERGE_TRIGGER_COUNT
};

enum SOGO_MERGE_INPUTS
{
    SOGO_MERGE_INPUT_1,
    SOGO_MERGE_INPUT_2,
    SOGO_MERGE_INPUT_COUNT
};

enum SOGO_MERGE_OUTPUTS
{
    SOGO_MERGE_OUTPUT,
    SOGO_MERGE_OUTPUT_COUNT
};

static bool RenderMerge(TFrameIndex frame_count, RenderOutput* input_data_1, RenderOutput* input_data_2)
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

    if (!RenderMerge(render_parameters->m_FrameCount, render_parameters->m_RenderInputs[0].m_RenderOutput, render_parameters->m_RenderInputs[1].m_RenderOutput))
    {
        return false;
    }
    render_parameters->m_RenderOutputs[0].m_Buffer = render_parameters->m_RenderInputs[0].m_RenderOutput->m_Buffer;
    render_parameters->m_RenderInputs[1].m_RenderOutput->m_Buffer = 0x0;
    return true;
}

struct OutputDescription MergeNodeOutputDescriptions[SOGO_MERGE_OUTPUT_COUNT] =
{
    {OutputDescription::PASS_THROUGH, {0}}
};

const NodeDescription MergeNodeDescription =
{
    RenderMerge,
    0x0,
    MergeNodeOutputDescriptions,
    0x0,
    SOGO_MERGE_INPUT_COUNT,
    SOGO_MERGE_OUTPUT_COUNT,
    SOGO_MERGE_RESOURCE_COUNT,
    SOGO_MERGE_PARAMETER_COUNT,
    SOGO_MERGE_TRIGGER_COUNT
};


///////////////////// SOGO GAIN

enum SOGO_GAIN_PARAMETERS
{
    SOGO_GAIN_PARAMETER_GAIN_INDEX,
    SOGO_GAIN_PARAMETER_FILTERED_GAIN_INDEX,
    SOGO_GAIN_PARAMETER_COUNT
};

enum SOGO_GAIN_RESOURCES
{
    SOGO_GAIN_RESOURCE_COUNT
};

enum SOGO_GAIN_TRIGGERS
{
    SOGO_GAIN_TRIGGER_COUNT
};

enum SOGO_GAIN_INPUTS
{
    SOGO_GAIN_INPUT,
    SOGO_GAIN_INPUT_COUNT
};

enum SOGO_GAIN_OUTPUTS
{
    SOGO_GAIN_OUTPUT,
    SOGO_GAIN_OUTPUT_COUNT
};

static bool GainFlat(float* io_buffer, TChannelIndex channel_count, TFrameIndex frame_count, float gain)
{
    switch (channel_count)
    {
        case 1:
            while (frame_count--)
            {
                *io_buffer++ *= gain;
            }
            return true;
        case 2:
            while (frame_count--)
            {
                *io_buffer++ *= gain;
                *io_buffer++ *= gain;
            }
            return true;
        default:
            return false;
    }
}

static bool GainRamp(float* io_buffer, TChannelIndex channel_count, TFrameIndex frame_count, float& gain, float gain_step)
{
    switch (channel_count)
    {
        case 1:
            while (frame_count--)
            {
                *io_buffer++ *= gain;
                gain += gain_step;
            }
            return true;
        case 2:
            while (frame_count--)
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

static bool RenderGain(TFrameIndex frame_count, RenderOutput* output_data, float gain, float& filtered_gain)
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

    TFrameIndex step_count = (TFrameIndex)(floor(fabs(gain - filtered_gain) / max_gain_step_per_frame));
    TFrameIndex ramp_frames = (step_count < frame_count) ? step_count : frame_count;
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
    RenderOutput* input_data = render_parameters->m_RenderInputs[SOGO_GAIN_INPUT].m_RenderOutput;
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

    render_parameters->m_RenderOutputs[SOGO_GAIN_OUTPUT].m_Buffer = input_data->m_Buffer;
    input_data->m_Buffer = 0x0;
    if (!RenderGain(render_parameters->m_FrameCount, &render_parameters->m_RenderOutputs[SOGO_GAIN_OUTPUT], gain, filtered_gain))
    {
        return false;
    }
    render_parameters->m_Parameters[SOGO_GAIN_PARAMETER_FILTERED_GAIN_INDEX] = filtered_gain;
    return true;
}

static const ParameterDescription GainParameters[SOGO_GAIN_PARAMETER_COUNT] = {
    {"Gain", 1.f},
    {0x0, 1.f}
};

struct OutputDescription GainNodeOutputDescriptions[SOGO_GAIN_OUTPUT_COUNT] =
{
    {OutputDescription::PASS_THROUGH, {0}}
};

const NodeDescription GainNodeDescription =
{
    RenderGain,
    GainParameters,
    GainNodeOutputDescriptions,
    0x0,
    SOGO_GAIN_INPUT_COUNT,
    SOGO_GAIN_OUTPUT_COUNT,
    SOGO_GAIN_RESOURCE_COUNT,
    SOGO_GAIN_PARAMETER_COUNT,
    SOGO_GAIN_TRIGGER_COUNT
};





///////////////////// SOGO SINE

enum SOGO_SINE_PARAMETERS
{
    SOGO_SINE_PARAMETER_FREQUENCY_INDEX,
    SOGO_SINE_PARAMETER_FILTERED_FREQUENCY_INDEX,
    SOGO_SINE_PARAMETER_FILTERED_VALUE_INDEX,
    SOGO_SINE_PARAMETER_COUNT
};

enum SOGO_SINE_RESOURCES
{
    SOGO_SINE_RESOURCE_COUNT
};

enum SOGO_SINE_TRIGGERS
{
    SOGO_SINE_TRIGGER_START_INDEX,
    SOGO_SINE_TRIGGER_STOP_INDEX,
    SOGO_SINE_TRIGGER_COUNT
};

enum SOGO_SINE_INPUTS
{
    SOGO_SINE_INPUT_COUNT
};

enum SOGO_SINE_OUTPUTS
{
    SOGO_SINE_OUTPUT,
    SOGO_SINE_OUTPUT_COUNT
};

static bool RenderSine(HGraph graph, HNode node, const RenderParameters* render_parameters)
{
    float frequency = render_parameters->m_Parameters[SOGO_SINE_PARAMETER_FREQUENCY_INDEX];
    float filtered_frequency = render_parameters->m_Parameters[SOGO_SINE_PARAMETER_FILTERED_FREQUENCY_INDEX];
    float value = render_parameters->m_Parameters[SOGO_SINE_PARAMETER_FILTERED_VALUE_INDEX];

    filtered_frequency = ((frequency * 15) + filtered_frequency) / 16;
    float step = ((2.f * 3.141592654f) * filtered_frequency) / render_parameters->m_FrameRate;
    TFrameIndex frame_count = render_parameters->m_FrameCount;
    render_parameters->m_RenderOutputs[0].m_Buffer = render_parameters->m_AllocateBuffer(graph, node, 1, frame_count);
    float* io_buffer = render_parameters->m_RenderOutputs[0].m_Buffer;
    while (frame_count--)
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

static const ParameterDescription SineParameters[SOGO_SINE_PARAMETER_COUNT] = {
    {"Frequency", 4000.0f},
    {0x0, 4000.0f},
    {0x0, 0.0f}
};

static const TriggerDescription SineTriggers[SOGO_SINE_TRIGGER_COUNT] = {
    {"Start"},
    {"Stop"}
};

struct OutputDescription SineNodeOutputDescriptions[SOGO_SINE_OUTPUT_COUNT] =
{
    {OutputDescription::FIXED, {1}}
};

const NodeDescription SineNodeDescription =
{
    RenderSine,
    SineParameters,
    SineNodeOutputDescriptions,
    SineTriggers,
    SOGO_SINE_INPUT_COUNT,
    SOGO_SINE_OUTPUT_COUNT,
    SOGO_SINE_RESOURCE_COUNT,
    SOGO_SINE_PARAMETER_COUNT,
    SOGO_SINE_TRIGGER_COUNT
};




///////////////////// SOGO MAKE_STEREO

enum SOGO_TOSTEREO_PARAMETERS
{
    SOGO_TOSTEREO_PARAMETER_COUNT
};

enum SOGO_TOSTEREO_RESOURCES
{
    SOGO_TOSTEREO_RESOURCE_COUNT
};

enum SOGO_TOSTEREO_TRIGGERS
{
    SOGO_TOSTEREO_TRIGGER_COUNT
};

enum SOGO_TOSTEREO_INPUTS
{
    SOGO_TOSTEREO_INPUT,
    SOGO_TOSTEREO_INPUT_COUNT
};

enum SOGO_TOSTEREO_OUTPUTS
{
    SOGO_TOSTEREO_OUTPUT,
    SOGO_TOSTEREO_OUTPUT_COUNT
};


static bool RenderToStereo(HGraph graph, HNode node, const RenderParameters* render_parameters)
{
    RenderOutput* input_data = render_parameters->m_RenderInputs[SOGO_TOSTEREO_INPUT].m_RenderOutput;
    if (input_data == 0x0)
    {
        render_parameters->m_RenderOutputs[SOGO_TOSTEREO_OUTPUT].m_Buffer = 0x0;
        return true;
    }
    if (input_data->m_ChannelCount == 2)
    {
        render_parameters->m_RenderOutputs[SOGO_TOSTEREO_OUTPUT].m_Buffer = input_data->m_Buffer;
        render_parameters->m_RenderOutputs[SOGO_TOSTEREO_OUTPUT].m_ChannelCount = input_data->m_ChannelCount;
        input_data->m_Buffer = 0x0;
        return true;
    }
    if (input_data->m_ChannelCount != 1)
    {
        return false;
    }

    float* mono_input = input_data->m_Buffer;

    TFrameIndex frame_count = render_parameters->m_FrameCount;
    float* stereo_output = render_parameters->m_AllocateBuffer(graph, node, 2, frame_count);
    if (stereo_output == 0)
    {
        return false;
    }

    render_parameters->m_RenderOutputs[SOGO_TOSTEREO_OUTPUT].m_Buffer = stereo_output;
    render_parameters->m_RenderOutputs[SOGO_TOSTEREO_OUTPUT].m_ChannelCount = 2;

    while (frame_count--)
    {
        *stereo_output++ = *mono_input;
        *stereo_output++ = *mono_input++;
    }

    return true;
}

struct OutputDescription ToStereoNodeOutputDescriptions[SOGO_TOSTEREO_OUTPUT_COUNT] =
{
    {OutputDescription::FIXED, {2}}
};

const NodeDescription ToStereoNodeDescription =
{
    RenderToStereo,
    0x0,
    ToStereoNodeOutputDescriptions,
    0x0,
    SOGO_TOSTEREO_INPUT_COUNT,
    SOGO_TOSTEREO_OUTPUT_COUNT,
    SOGO_TOSTEREO_RESOURCE_COUNT,
    SOGO_TOSTEREO_PARAMETER_COUNT,
    SOGO_TOSTEREO_TRIGGER_COUNT
};

///////////////////// SOGO DC

enum SOGO_DC_PARAMETERS
{
    SOGO_DC_PARAMETER_LEVEL_INDEX,
    SOGO_DC_PARAMETER_COUNT
};

enum SOGO_DC_RESOURCES
{
    SOGO_DC_RESOURCE_COUNT
};

enum SOGO_DC_TRIGGERS
{
    SOGO_DC_TRIGGER_COUNT
};

enum SOGO_DC_INPUTS
{
    SOGO_DC_INPUT_COUNT
};

enum SOGO_DC_OUTPUTS
{
    SOGO_DC_OUTPUT,
    SOGO_DC_OUTPUT_COUNT
};

static bool RenderDC(HGraph graph, HNode node, const RenderParameters* render_parameters)
{
    float level = render_parameters->m_Parameters[SOGO_DC_PARAMETER_LEVEL_INDEX];

    uint32_t frame_count = render_parameters->m_FrameCount;
    render_parameters->m_RenderOutputs[0].m_Buffer = render_parameters->m_AllocateBuffer(graph, node, 1, frame_count);
    float* io_buffer = render_parameters->m_RenderOutputs[SOGO_DC_OUTPUT].m_Buffer;
    while (frame_count--)
    {
        *io_buffer++ = level;
    }
    return true;
}

static const ParameterDescription DCParameters[SOGO_DC_PARAMETER_COUNT] = {
    {"Level", 1.0f}
};

struct OutputDescription DCNodeOutputDescriptions[SOGO_DC_OUTPUT_COUNT] =
{
    {OutputDescription::FIXED, {1}}
};

const NodeDescription DCNodeDescription =
{
    RenderDC,
    DCParameters,
    DCNodeOutputDescriptions,
    0x0,
    SOGO_DC_INPUT_COUNT,
    SOGO_DC_OUTPUT_COUNT,
    SOGO_DC_RESOURCE_COUNT,
    SOGO_DC_PARAMETER_COUNT,
    SOGO_DC_TRIGGER_COUNT
};




}