#include "sogo_nodes.h"
#include <math.h>

namespace sogo {

///////////////////// SOGO SPLIT

enum SOGO_SPLIT_PARAMETERS
{
    SOGO_SPLIT_PARAMETER_COUNT
};

enum SOGO_SPLIT_RESOURCES
{
    SOGO_SPLIT_RESOURCE_COUNT
};

enum SOGO_SPLIT_TRIGGER_INPUTS
{
    SOGO_SPLIT_TRIGGER_INPUT_COUNT
};

enum SOGO_SPLIT_TRIGGER_OUTPUTS
{
    SOGO_SPLIT_TRIGGER_OUTPUT_COUNT
};

enum SOGO_SPLIT_AUDIO_INPUTS
{
    SOGO_SPLIT_AUDIO_INPUT,
    SOGO_SPLIT_AUDIO_INPUT_COUNT
};

enum SOGO_SPLIT_AUDIO_OUTPUTS
{
    SOGO_SPLIT_AUDIO_OUTPUT_1,
    SOGO_SPLIT_AUDIO_OUTPUT_2,
    SOGO_SPLIT_AUDIO_OUTPUT_COUNT
};

static void RenderSplit(HGraph graph, HNode node, const RenderParameters* render_parameters)
{
    AudioOutput* input_data = render_parameters->m_AudioInputs[0].m_AudioOutput;
    if (input_data->m_Buffer == 0x0)
    {
        render_parameters->m_AudioOutputs[0].m_Buffer = 0x0;
        render_parameters->m_AudioOutputs[1].m_Buffer = 0x0;
        return;
    }
    render_parameters->m_AudioOutputs[1].m_Buffer = render_parameters->m_AllocateAudioBuffer(graph, node, input_data->m_ChannelCount, render_parameters->m_FrameCount);

    const float* output1      = input_data->m_Buffer;
    float*       output2      = render_parameters->m_AudioOutputs[1].m_Buffer;
    TSampleIndex sample_count = render_parameters->m_FrameCount * input_data->m_ChannelCount;
    while (sample_count--)
    {
        *output2++ = *output1++;
    }

    render_parameters->m_AudioOutputs[0].m_Buffer = input_data->m_Buffer;
    input_data->m_Buffer                          = 0x0;
}

static struct AudioOutputDescription SplitNodeAudioOutputDescriptions[SOGO_SPLIT_AUDIO_OUTPUT_COUNT] = {
    { AudioOutputDescription::PASS_THROUGH, { 0 } },
    { AudioOutputDescription::AS_INPUT, { 0 } }
};

static void SplitNodeGetNodeRuntimeDescCallback(const GraphRuntimeSettings*, NodeRuntimeDescription* out_node_runtime_desc)
{
    out_node_runtime_desc->m_InitCallback      = 0;
    out_node_runtime_desc->m_RenderCallback    = RenderSplit;
    out_node_runtime_desc->m_ContextMemorySize = 0;
}

const NodeStaticDescription SplitNodeDesc = {
    SplitNodeGetNodeRuntimeDescCallback,
    0x0,
    SplitNodeAudioOutputDescriptions,
    0x0,
    SOGO_SPLIT_AUDIO_INPUT_COUNT,
    SOGO_SPLIT_AUDIO_OUTPUT_COUNT,
    SOGO_SPLIT_RESOURCE_COUNT,
    SOGO_SPLIT_PARAMETER_COUNT,
    SOGO_SPLIT_TRIGGER_INPUT_COUNT,
    SOGO_SPLIT_TRIGGER_OUTPUT_COUNT
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

enum SOGO_MERGE_TRIGGER_INPUTS
{
    SOGO_MERGE_TRIGGER_INPUT_COUNT
};

enum SOGO_MERGE_TRIGGER_OUTPUTS
{
    SOGO_MERGE_TRIGGER_OUTPUT_COUNT
};

enum SOGO_MERGE_AUDIO_INPUTS
{
    SOGO_MERGE_AUDIO_INPUT_1,
    SOGO_MERGE_AUDIO_INPUT_2,
    SOGO_MERGE_AUDIO_INPUT_COUNT
};

enum SOGO_MERGE_AUDIO_OUTPUTS
{
    SOGO_MERGE_AUDIO_OUTPUT,
    SOGO_MERGE_AUDIO_OUTPUT_COUNT
};

static bool RenderMerge(TFrameIndex frame_count, AudioOutput* input_data_1, AudioOutput* input_data_2)
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

static void RenderMerge(HGraph, HNode, const RenderParameters* render_parameters)
{
    if (render_parameters->m_AudioInputs[0].m_AudioOutput->m_Buffer == 0x0)
    {
        if (render_parameters->m_AudioInputs[1].m_AudioOutput->m_Buffer == 0x0)
        {
            render_parameters->m_AudioInputs[0].m_AudioOutput->m_Buffer = 0x0;
            return;
        }
        else
        {
            render_parameters->m_AudioOutputs[0].m_Buffer               = render_parameters->m_AudioInputs[1].m_AudioOutput->m_Buffer;
            render_parameters->m_AudioInputs[1].m_AudioOutput->m_Buffer = 0x0;
            return;
        }
    }
    else if (render_parameters->m_AudioInputs[1].m_AudioOutput->m_Buffer == 0x0)
    {
        render_parameters->m_AudioOutputs[0].m_Buffer               = render_parameters->m_AudioInputs[1].m_AudioOutput->m_Buffer;
        render_parameters->m_AudioInputs[1].m_AudioOutput->m_Buffer = 0x0;
        return;
    }

    if (!RenderMerge(render_parameters->m_FrameCount, render_parameters->m_AudioInputs[0].m_AudioOutput, render_parameters->m_AudioInputs[1].m_AudioOutput))
    {
        render_parameters->m_AudioOutputs[0].m_Buffer = 0x0;
        return;
    }
    render_parameters->m_AudioOutputs[0].m_Buffer               = render_parameters->m_AudioInputs[0].m_AudioOutput->m_Buffer;
    render_parameters->m_AudioInputs[1].m_AudioOutput->m_Buffer = 0x0;
}

static struct AudioOutputDescription MergeNodeAudioOutputDescriptions[SOGO_MERGE_AUDIO_OUTPUT_COUNT] = {
    { AudioOutputDescription::PASS_THROUGH, { 0 } }
};

static void MergeNodeGetNodeRuntimeDescCallback(const GraphRuntimeSettings*, NodeRuntimeDescription* out_node_runtime_desc)
{
    out_node_runtime_desc->m_InitCallback      = 0;
    out_node_runtime_desc->m_RenderCallback    = RenderMerge;
    out_node_runtime_desc->m_ContextMemorySize = 0;
}

const NodeStaticDescription MergeNodeDesc = {
    MergeNodeGetNodeRuntimeDescCallback,
    0x0,
    MergeNodeAudioOutputDescriptions,
    0x0,
    SOGO_MERGE_AUDIO_INPUT_COUNT,
    SOGO_MERGE_AUDIO_OUTPUT_COUNT,
    SOGO_MERGE_RESOURCE_COUNT,
    SOGO_MERGE_PARAMETER_COUNT,
    SOGO_MERGE_TRIGGER_INPUT_COUNT,
    SOGO_MERGE_TRIGGER_OUTPUT_COUNT
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

enum SOGO_GAIN_TRIGGER_INPUTS
{
    SOGO_GAIN_TRIGGER_INPUT_COUNT
};

enum SOGO_GAIN_TRIGGER_OUTPUTS
{
    SOGO_GAIN_TRIGGER_OUTPUT_COUNT
};

enum SOGO_GAIN_AUDIO_INPUTS
{
    SOGO_GAIN_AUDIO_INPUT,
    SOGO_GAIN_AUDIO_INPUT_COUNT
};

enum SOGO_GAIN_AUDIO_OUTPUTS
{
    SOGO_GAIN_AUDIO_OUTPUT,
    SOGO_GAIN_AUDIO_OUTPUT_COUNT
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

static bool RenderGain(TFrameIndex frame_count, AudioOutput* output_data, float gain, float& filtered_gain)
{
    static const float max_gain_step_per_frame = 1.0f / 32;

    if (fabs(gain - filtered_gain) < 0.001f)
    {
        filtered_gain = gain;
        if (filtered_gain < 0.001f)
        {
            output_data->m_Buffer = 0x0;
            return true;
        }
        return GainFlat(output_data->m_Buffer, output_data->m_ChannelCount, frame_count, gain);
    }

    TFrameIndex step_count  = (TFrameIndex)(floor(fabs(gain - filtered_gain) / max_gain_step_per_frame));
    TFrameIndex ramp_frames = (step_count < frame_count) ? step_count : frame_count;
    float       gain_step   = gain > filtered_gain ? max_gain_step_per_frame : -max_gain_step_per_frame;

    if (!GainRamp(output_data->m_Buffer, output_data->m_ChannelCount, ramp_frames, filtered_gain, gain_step))
    {
        return false;
    }
    filtered_gain = gain;
    return GainFlat(&output_data->m_Buffer[output_data->m_ChannelCount * ramp_frames], output_data->m_ChannelCount, frame_count - ramp_frames, gain);
}

static void RenderGain(HGraph, HNode, const RenderParameters* render_parameters)
{
    AudioOutput* input_data    = render_parameters->m_AudioInputs[SOGO_GAIN_AUDIO_INPUT].m_AudioOutput;
    float        gain          = render_parameters->m_Parameters[SOGO_GAIN_PARAMETER_GAIN_INDEX].m_Float;
    float        filtered_gain = render_parameters->m_Parameters[SOGO_GAIN_PARAMETER_FILTERED_GAIN_INDEX].m_Float;
    if (input_data->m_Buffer == 0x0)
    {
        // TODO: Hmm, we still want to filter the gain, or can we just hack it and set filtered_gain to gain?
        // Assuming each render call has enough frames, yes, otherwise no. Hack it for now.
        filtered_gain                                 = gain;
        render_parameters->m_AudioOutputs[0].m_Buffer = 0x0;
        return;
    }

    render_parameters->m_AudioOutputs[SOGO_GAIN_AUDIO_OUTPUT].m_Buffer = input_data->m_Buffer;
    input_data->m_Buffer                                               = 0x0;
    if (!RenderGain(render_parameters->m_FrameCount, &render_parameters->m_AudioOutputs[SOGO_GAIN_AUDIO_OUTPUT], gain, filtered_gain))
    {
        // TODO: Hmm, we still want to filter the gain, or can we just hack it and set filtered_gain to gain?
        // Assuming each render call has enough frames, yes, otherwise no. Hack it for now.
        filtered_gain                                 = gain;
        render_parameters->m_AudioOutputs[0].m_Buffer = 0x0;
        return;
    }
    render_parameters->m_Parameters[SOGO_GAIN_PARAMETER_FILTERED_GAIN_INDEX].m_Float = filtered_gain;
}

static const ParameterDescription GainParameters[SOGO_GAIN_PARAMETER_COUNT] = {
    { "Gain", 1.f },
    { 0x0, 1.f }
};

static struct AudioOutputDescription GainNodeAudioOutputDescriptions[SOGO_GAIN_AUDIO_OUTPUT_COUNT] = {
    { AudioOutputDescription::PASS_THROUGH, { 0 } }
};

static void GainNodeGetNodeRuntimeDescCallback(const GraphRuntimeSettings*, NodeRuntimeDescription* out_node_runtime_desc)
{
    out_node_runtime_desc->m_InitCallback      = 0;
    out_node_runtime_desc->m_RenderCallback    = RenderGain;
    out_node_runtime_desc->m_ContextMemorySize = 0;
}

const NodeStaticDescription GainNodeDesc = {
    GainNodeGetNodeRuntimeDescCallback,
    GainParameters,
    GainNodeAudioOutputDescriptions,
    0x0,
    SOGO_GAIN_AUDIO_INPUT_COUNT,
    SOGO_GAIN_AUDIO_OUTPUT_COUNT,
    SOGO_GAIN_RESOURCE_COUNT,
    SOGO_GAIN_PARAMETER_COUNT,
    SOGO_GAIN_TRIGGER_INPUT_COUNT,
    SOGO_GAIN_TRIGGER_OUTPUT_COUNT
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

enum SOGO_SINE_TRIGGER_INPUTS
{
    SOGO_SINE_TRIGGER_START_INDEX,
    SOGO_SINE_TRIGGER_STOP_INDEX,
    SOGO_SINE_TRIGGER_INPUT_COUNT
};

enum SOGO_SINE_TRIGGER_OUTPUTS
{
    SOGO_SINE_TRIGGER_OUTPUT_COUNT
};

enum SOGO_SINE_AUDIO_INPUTS
{
    SOGO_SINE_AUDIO_INPUT_COUNT
};

enum SOGO_SINE_AUDIO_OUTPUTS
{
    SOGO_SINE_AUDIO_OUTPUT,
    SOGO_SINE_AUDIO_OUTPUT_COUNT
};

static void RenderSine(HGraph graph, HNode node, const RenderParameters* render_parameters)
{
    float frequency          = render_parameters->m_Parameters[SOGO_SINE_PARAMETER_FREQUENCY_INDEX].m_Float;
    float filtered_frequency = render_parameters->m_Parameters[SOGO_SINE_PARAMETER_FILTERED_FREQUENCY_INDEX].m_Float;
    float value              = render_parameters->m_Parameters[SOGO_SINE_PARAMETER_FILTERED_VALUE_INDEX].m_Float;

    filtered_frequency                            = ((frequency * 15) + filtered_frequency) / 16;
    float       step                              = ((2.f * 3.141592654f) * filtered_frequency) / render_parameters->m_FrameRate;
    TFrameIndex frame_count                       = render_parameters->m_FrameCount;
    render_parameters->m_AudioOutputs[0].m_Buffer = render_parameters->m_AllocateAudioBuffer(graph, node, 1, frame_count);
    float* io_buffer                              = render_parameters->m_AudioOutputs[0].m_Buffer;
    while (frame_count--)
    {
        float frame  = sinf(value);
        *io_buffer++ = frame;
        value += step;
        if (value > (2.f * 3.141592654f))
        {
            value -= (2.f * 3.141592654f);
        }
    }
    render_parameters->m_Parameters[SOGO_SINE_PARAMETER_FILTERED_FREQUENCY_INDEX].m_Float = filtered_frequency;
    render_parameters->m_Parameters[SOGO_SINE_PARAMETER_FILTERED_VALUE_INDEX].m_Float     = value;
}

static const ParameterDescription SineParameters[SOGO_SINE_PARAMETER_COUNT] = {
    { "Frequency", 4000.0f },
    { 0x0, 4000.0f },
    { 0x0, 0.0f }
};

static const TriggerDescription SineTriggers[SOGO_SINE_TRIGGER_INPUT_COUNT] = {
    { "Start" },
    { "Stop" }
};

static struct AudioOutputDescription SineNodeAudioOutputDescriptions[SOGO_SINE_AUDIO_OUTPUT_COUNT] = {
    { AudioOutputDescription::FIXED, { 1 } }
};

static void SineNodeGetNodeRuntimeDescCallback(const GraphRuntimeSettings*, NodeRuntimeDescription* out_node_runtime_desc)
{
    out_node_runtime_desc->m_InitCallback      = 0;
    out_node_runtime_desc->m_RenderCallback    = RenderSine;
    out_node_runtime_desc->m_ContextMemorySize = 0;
}

const NodeStaticDescription SineNodeDesc = {
    SineNodeGetNodeRuntimeDescCallback,
    SineParameters,
    SineNodeAudioOutputDescriptions,
    0x0,
    SOGO_SINE_AUDIO_INPUT_COUNT,
    SOGO_SINE_AUDIO_OUTPUT_COUNT,
    SOGO_SINE_RESOURCE_COUNT,
    SOGO_SINE_PARAMETER_COUNT,
    SOGO_SINE_TRIGGER_INPUT_COUNT,
    SOGO_SINE_TRIGGER_OUTPUT_COUNT
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

enum SOGO_TOSTEREO_TRIGGER_INPUTS
{
    SOGO_TOSTEREO_TRIGGER_INPUT_COUNT
};

enum SOGO_TOSTEREO_TRIGGER_OUTPUTS
{
    SOGO_TOSTEREO_TRIGGER_OUTPUT_COUNT
};

enum SOGO_TOSTEREO_AUDIO_INPUTS
{
    SOGO_TOSTEREO_AUDIO_INPUT,
    SOGO_TOSTEREO_AUDIO_INPUT_COUNT
};

enum SOGO_TOSTEREO_AUDIO_OUTPUTS
{
    SOGO_TOSTEREO_AUDIO_OUTPUT,
    SOGO_TOSTEREO_AUDIO_OUTPUT_COUNT
};

static void RenderToStereo(HGraph graph, HNode node, const RenderParameters* render_parameters)
{
    AudioOutput* input_data = render_parameters->m_AudioInputs[SOGO_TOSTEREO_AUDIO_INPUT].m_AudioOutput;
    if (input_data == 0x0)
    {
        render_parameters->m_AudioOutputs[SOGO_TOSTEREO_AUDIO_OUTPUT].m_Buffer = 0x0;
        return;
    }
    if (input_data->m_ChannelCount == 2)
    {
        render_parameters->m_AudioOutputs[SOGO_TOSTEREO_AUDIO_OUTPUT].m_Buffer       = input_data->m_Buffer;
        render_parameters->m_AudioOutputs[SOGO_TOSTEREO_AUDIO_OUTPUT].m_ChannelCount = input_data->m_ChannelCount;
        input_data->m_Buffer                                                         = 0x0;
        return;
    }
    if (input_data->m_ChannelCount != 1)
    {
        render_parameters->m_AudioOutputs[SOGO_TOSTEREO_AUDIO_OUTPUT].m_Buffer = 0x0;
        return;
    }

    float* mono_input = input_data->m_Buffer;

    TFrameIndex frame_count   = render_parameters->m_FrameCount;
    float*      stereo_output = render_parameters->m_AllocateAudioBuffer(graph, node, 2, frame_count);

    render_parameters->m_AudioOutputs[SOGO_TOSTEREO_AUDIO_OUTPUT].m_Buffer       = stereo_output;
    render_parameters->m_AudioOutputs[SOGO_TOSTEREO_AUDIO_OUTPUT].m_ChannelCount = 2;

    while (frame_count--)
    {
        *stereo_output++ = *mono_input;
        *stereo_output++ = *mono_input++;
    }
}

static struct AudioOutputDescription ToStereoNodeAudioOutputDescriptions[SOGO_TOSTEREO_AUDIO_OUTPUT_COUNT] = {
    { AudioOutputDescription::FIXED, { 2 } }
};

static void ToStereoNodeGetNodeRuntimeDescCallback(const GraphRuntimeSettings*, NodeRuntimeDescription* out_node_runtime_desc)
{
    out_node_runtime_desc->m_InitCallback      = 0;
    out_node_runtime_desc->m_RenderCallback    = RenderToStereo;
    out_node_runtime_desc->m_ContextMemorySize = 0;
}

const NodeStaticDescription ToStereoNodeDesc = {
    ToStereoNodeGetNodeRuntimeDescCallback,
    0x0,
    ToStereoNodeAudioOutputDescriptions,
    0x0,
    SOGO_TOSTEREO_AUDIO_INPUT_COUNT,
    SOGO_TOSTEREO_AUDIO_OUTPUT_COUNT,
    SOGO_TOSTEREO_RESOURCE_COUNT,
    SOGO_TOSTEREO_PARAMETER_COUNT,
    SOGO_TOSTEREO_TRIGGER_INPUT_COUNT,
    SOGO_TOSTEREO_TRIGGER_OUTPUT_COUNT
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

enum SOGO_DC_TRIGGER_INPUTS
{
    SOGO_DC_TRIGGER_INPUT_COUNT
};

enum SOGO_DC_TRIGGER_OUTPUTS
{
    SOGO_DC_TRIGGER_OUTPUT_COUNT
};

enum SOGO_DC_AUDIO_INPUTS
{
    SOGO_DC_AUDIO_INPUT_COUNT
};

enum SOGO_DC_AUDIO_OUTPUTS
{
    SOGO_DC_AUDIO_OUTPUT,
    SOGO_DC_AUDIO_OUTPUT_COUNT
};

static void RenderDC(HGraph graph, HNode node, const RenderParameters* render_parameters)
{
    float level = render_parameters->m_Parameters[SOGO_DC_PARAMETER_LEVEL_INDEX].m_Float;

    uint32_t frame_count                          = render_parameters->m_FrameCount;
    render_parameters->m_AudioOutputs[0].m_Buffer = render_parameters->m_AllocateAudioBuffer(graph, node, 1, frame_count);
    float* io_buffer                              = render_parameters->m_AudioOutputs[SOGO_DC_AUDIO_OUTPUT].m_Buffer;
    while (frame_count--)
    {
        *io_buffer++ = level;
    }
}

static const ParameterDescription DCParameters[SOGO_DC_PARAMETER_COUNT] = {
    { "Level", 1.0f }
};

static struct AudioOutputDescription DCNodeAudioOutputDescriptions[SOGO_DC_AUDIO_OUTPUT_COUNT] = {
    { AudioOutputDescription::FIXED, { 1 } }
};

static void DCNodeGetNodeRuntimeDescCallback(const GraphRuntimeSettings*, NodeRuntimeDescription* out_node_runtime_desc)
{
    out_node_runtime_desc->m_InitCallback      = 0;
    out_node_runtime_desc->m_RenderCallback    = RenderDC;
    out_node_runtime_desc->m_ContextMemorySize = 0;
}

const NodeStaticDescription DCNodeDesc = {
    DCNodeGetNodeRuntimeDescCallback,
    DCParameters,
    DCNodeAudioOutputDescriptions,
    0x0,
    SOGO_DC_AUDIO_INPUT_COUNT,
    SOGO_DC_AUDIO_OUTPUT_COUNT,
    SOGO_DC_RESOURCE_COUNT,
    SOGO_DC_PARAMETER_COUNT,
    SOGO_DC_TRIGGER_INPUT_COUNT,
    SOGO_DC_TRIGGER_OUTPUT_COUNT
};

#if 0

enum SOGO_SEQUENTIAL_TRIGGER_OUTPUT_TRIGGERS
{
    SOGO_SEQUENTIAL_TRIGGER_TRIGGER_OUT,
    SOGO_SEQUENTIAL_TRIGGER_TRIGGER_OUTUT_COUNT
};

static void RenderSequentialTrigger(HGraph graph, HNode , const RenderParameters* render_parameters)
{
    // TODO
    bool trigger = false;   // Some logic to not trigger all the time
    if (trigger)
    {
        TriggerOutput* out = &render_parameters->m_TriggerOutputs[SOGO_SEQUENTIAL_TRIGGER_TRIGGER_OUT];
        Trigger(graph, out->m_Node, out->m_Trigger);
    }
}

static struct TriggerDescription SequentialTriggerTriggerDescriptions[SOGO_SEQUENTIAL_TRIGGER_TRIGGER_OUTUT_COUNT] =
{
    "Out"
};

const NodeDescription SequentialTriggerDescription =
{
    0x0,
    0x0,
    RenderSequentialTrigger,
    0x0,
    0x0,
    SequentialTriggerTriggerDescriptions,
    0,
    0,
    0,
    0,
    0,
    SOGO_SEQUENTIAL_TRIGGER_TRIGGER_OUTUT_COUNT
};

#endif

} // namespace sogo
