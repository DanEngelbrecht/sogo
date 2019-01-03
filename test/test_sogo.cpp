#include "../src/sogo_nodes.h"
#include "../src/sogo_utils.h"

#include <memory>

#define ALIGN_SIZE(x, align)    (((x) + ((align) - 1)) & ~((align) - 1))

typedef struct SCtx
{
} SCtx;

static SCtx* sogo_main_setup()
{
	return reinterpret_cast<SCtx*>( malloc( sizeof(SCtx) ) );
}

static void sogo_main_teardown(SCtx* ctx)
{
	free(ctx);
}

static void test_setup(SCtx* )
{
}

static void test_teardown(SCtx* )
{
}

static void sogo_create(SCtx* )
{
    sogo::GraphDescription GRAPH_DESCRIPTION =
    {
        0,
        0x0,
        0,
        0x0,
        0x0
    };

    static const uint32_t MAX_BATCH_SIZE = 128;
    size_t graph_size = 0;
    sogo::TSampleIndex out_scratch_buffer_sample_count;
    ASSERT_TRUE(sogo::GetGraphSize(MAX_BATCH_SIZE, &GRAPH_DESCRIPTION, graph_size, out_scratch_buffer_sample_count));
    graph_size = ALIGN_SIZE(graph_size, sizeof(float));
    size_t audio_buffer_size = out_scratch_buffer_sample_count * sizeof(float);
    void* graph_mem = malloc(graph_size + audio_buffer_size);
    ASSERT_NE(0x0, graph_mem);

    float* audio_buffer_mem = (float*)&((uint8_t*)graph_mem)[graph_size];

    sogo::HGraph graph = sogo::CreateGraph(
        graph_mem,
        audio_buffer_mem,
        44100,
        MAX_BATCH_SIZE,
        &GRAPH_DESCRIPTION);
    TEST_ASSERT_NE(0x0, graph);

    TEST_ASSERT_TRUE(RenderGraph(graph, 64));
    free(graph_mem);
}

static void sogo_simple_graph(SCtx* )
{
    static const uint32_t NODE_COUNT = 6;
    const sogo::NodeDescription* NODES[NODE_COUNT] =
    {
        &sogo::DCNodeDescription,           // 0.5
        &sogo::GainNodeDescription,         // 0.5 * 0.5
        &sogo::ToStereoNodeDescription,     // 0.5 * 0.5
        &sogo::SplitNodeDescription,        // 0.5 * 0.5
        &sogo::GainNodeDescription,         // 0.5 * 0.5 * 2.0
        &sogo::MergeNodeDescription         // 0.5 * 0.5 * 2.0 + 0.5 * 0.5
    };

    const char* NODE_NAMES[NODE_COUNT] =
    {
        "DC",
        "DC-Gain",
        0x0,
        0x0,
        "Split-Gain",
        0x0
    };

    static const uint16_t CONNECTION_COUNT = 6;
    sogo::NodeConnection CONNECTIONS[CONNECTION_COUNT] =
    {
        { 0, 0, 1, 0 },
        { 1, 0, 2, 0 },
        { 2, 0, 3, 0 },
        { 3, 0, 4, 0 },
        { 4, 0, 5, 0 },
        { 3, 1, 5, 1 }
    };

    sogo::GraphDescription GRAPH_DESCRIPTION =
    {
        NODE_COUNT,
        NODES,
        CONNECTION_COUNT,
        CONNECTIONS,
        0x0
    };

    static const uint32_t MAX_BATCH_SIZE = 128;

    size_t graph_size = 0;
    sogo::TSampleIndex out_scratch_buffer_sample_count;
    ASSERT_TRUE(sogo::GetGraphSize(MAX_BATCH_SIZE, &GRAPH_DESCRIPTION, graph_size, out_scratch_buffer_sample_count));
    graph_size = ALIGN_SIZE(graph_size, sizeof(void*));

    size_t access_size = 0;
    struct sogo::AccessDescription ACCESS_DESCRIPTION =
    {
        &GRAPH_DESCRIPTION,
        NODE_NAMES
    };

    ASSERT_TRUE(sogo::GetAccessSize(&ACCESS_DESCRIPTION, &access_size));
    access_size = ALIGN_SIZE(access_size, sizeof(float));
    size_t audio_buffer_size = out_scratch_buffer_sample_count * sizeof(float);

    void* mem = malloc(graph_size + access_size + audio_buffer_size);
    ASSERT_NE(0x0, mem);

    void* graph_mem = mem;
    void* access_mem = (void*)&((uint8_t*)mem)[graph_size];
    float* audio_buffer_mem = (float*)&((uint8_t*)access_mem)[access_size];
    sogo::HGraph graph = sogo::CreateGraph(
        graph_mem,
        audio_buffer_mem,
        44100,
        MAX_BATCH_SIZE,
        &GRAPH_DESCRIPTION);
    ASSERT_NE(0x0, graph);

    sogo::HAccess access = sogo::CreateAccess(access_mem, &ACCESS_DESCRIPTION);
    ASSERT_NE(0x0, access);

    sogo::TParameterNameHash level_parameter_hash = sogo::MakeParameterHash(sogo::MakeNodeNameHash(NODE_NAMES[0]), "Level");
    ASSERT_TRUE(sogo::SetParameter(access, graph, level_parameter_hash, 0.5f));

    sogo::TParameterNameHash gain_parameter_hash = sogo::MakeParameterHash(sogo::MakeNodeNameHash(NODE_NAMES[1]), "Gain");
    ASSERT_TRUE(sogo::SetParameter(access, graph, gain_parameter_hash, 0.5f));

    sogo::TParameterNameHash gain2_parameter_hash = sogo::MakeParameterHash(sogo::MakeNodeNameHash(NODE_NAMES[4]), "Gain");
    ASSERT_TRUE(sogo::SetParameter(access, graph, gain2_parameter_hash, 2.f));

    for (uint32_t i = 0; i < 6; ++i)
    {
        ASSERT_TRUE(sogo::RenderGraph(graph, MAX_BATCH_SIZE));
        sogo::AudioOutput* render_output = sogo::GetAudioOutput(graph, 5, 0);

        ASSERT_TRUE(render_output != 0x0);
        ASSERT_EQ(2, render_output->m_ChannelCount);

        if (i > 0)
        {
            // Skip first batch since it has filtering on gain
            static const float expected_signal = ((0.5f * 0.5f) * 2.f) + (0.5f * 0.5f);
            for (sogo::TFrameIndex f = 0; f < MAX_BATCH_SIZE * 2; ++f)
            {
                ASSERT_EQ(expected_signal, render_output->m_Buffer[f]);
            }
        }

        ASSERT_TRUE(render_output->m_Buffer != 0x0);
    }
    free(mem);
}

static void sogo_merge_graphs(SCtx* )
{
    // This is nore of an authoring test, checking how easy it is to modify graphs

    static const sogo::TNodeIndex NODE_COUNT_GENERATOR = 2;
    const sogo::NodeDescription* NODES_GENERATOR[NODE_COUNT_GENERATOR] =
    {
        &sogo::DCNodeDescription,
        &sogo::GainNodeDescription
    };

    static const sogo::TConnectionIndex CONNECTION_COUNT_GENERATOR = 1;
    sogo::NodeConnection CONNECTIONS_GENERATOR[CONNECTION_COUNT_GENERATOR] =
    {
        { 0, 0, 1, 0 }
    };

    sogo::GraphDescription GRAPH_DESCRIPTION_GENERATOR =
    {
        NODE_COUNT_GENERATOR,
        NODES_GENERATOR,
        CONNECTION_COUNT_GENERATOR,
        CONNECTIONS_GENERATOR,
        0x0
    };


    static const sogo::TNodeIndex NODE_COUNT_MIXER = 2;
    const sogo::NodeDescription* NODES_MIXER[NODE_COUNT_MIXER] =
    {
        &sogo::MergeNodeDescription,
        &sogo::GainNodeDescription
    };

    static const sogo::TConnectionIndex CONNECTION_COUNT_MIXER = 1;
    sogo::NodeConnection CONNECTIONS_MIXER[CONNECTION_COUNT_MIXER] =
    {
        { 0, 0, 1, 0 }
    };

    sogo::GraphDescription GRAPH_DESCRIPTION_MIXER =
    {
        NODE_COUNT_MIXER,
        NODES_MIXER,
        CONNECTION_COUNT_MIXER,
        CONNECTIONS_MIXER,
        0x0
    };


    // Build graph of two generators going into a mixer
    static const sogo::TNodeIndex NODE_COUNT = NODE_COUNT_GENERATOR + NODE_COUNT_GENERATOR + NODE_COUNT_MIXER;
    const sogo::NodeDescription* NODES[NODE_COUNT];
    static const sogo::TConnectionIndex CONNECTION_COUNT = CONNECTION_COUNT_GENERATOR + CONNECTION_COUNT_GENERATOR + CONNECTION_COUNT_MIXER + 1 + 1;
    sogo::NodeConnection CONNECTIONS[CONNECTION_COUNT];

    // Merge the three graphs
    sogo::TNodeIndex node_index = 0;
    sogo::TNodeIndex node_base = 0;
    sogo::TConnectionIndex connection_index = 0;

    for (sogo::TNodeIndex n = 0; n < NODE_COUNT_GENERATOR; ++n)
    {
        NODES[node_index++] = NODES_GENERATOR[n];
    }
    for (sogo::TConnectionIndex c = 0; c < CONNECTION_COUNT_GENERATOR; ++c)
    {
        CONNECTIONS[connection_index].m_InputNodeIndex = CONNECTIONS_GENERATOR[c].m_InputNodeIndex + node_base;
        CONNECTIONS[connection_index].m_InputIndex = CONNECTIONS_GENERATOR[c].m_InputIndex;
        CONNECTIONS[connection_index].m_OutputNodeIndex = CONNECTIONS_GENERATOR[c].m_OutputNodeIndex + node_base;
        CONNECTIONS[connection_index++].m_OutputIndex = CONNECTIONS_GENERATOR[c].m_OutputIndex;
    }
    node_base += NODE_COUNT_GENERATOR;

    for (sogo::TNodeIndex n = 0; n < NODE_COUNT_GENERATOR; ++n)
    {
        NODES[node_index++] = NODES_GENERATOR[n];
    }
    for (sogo::TConnectionIndex c = 0; c < CONNECTION_COUNT_GENERATOR; ++c)
    {
        CONNECTIONS[connection_index].m_InputNodeIndex = CONNECTIONS_GENERATOR[c].m_InputNodeIndex + node_base;
        CONNECTIONS[connection_index].m_InputIndex = CONNECTIONS_GENERATOR[c].m_InputIndex;
        CONNECTIONS[connection_index].m_OutputNodeIndex = CONNECTIONS_GENERATOR[c].m_OutputNodeIndex + node_base;
        CONNECTIONS[connection_index++].m_OutputIndex = CONNECTIONS_GENERATOR[c].m_OutputIndex;
    }
    node_base += NODE_COUNT_GENERATOR;

    for (sogo::TNodeIndex n = 0; n < NODE_COUNT_MIXER; ++n)
    {
        NODES[node_index++] = NODES_MIXER[n];
    }
    for (sogo::TConnectionIndex c = 0; c < CONNECTION_COUNT_MIXER; ++c)
    {
        CONNECTIONS[connection_index].m_InputNodeIndex = CONNECTIONS_MIXER[c].m_InputNodeIndex + node_base;
        CONNECTIONS[connection_index].m_InputIndex = CONNECTIONS_MIXER[c].m_InputIndex;
        CONNECTIONS[connection_index].m_OutputNodeIndex = CONNECTIONS_MIXER[c].m_OutputNodeIndex + node_base;
        CONNECTIONS[connection_index++].m_OutputIndex = CONNECTIONS_MIXER[c].m_OutputIndex;
    }

    // Hook up the three graphs
    CONNECTIONS[connection_index].m_InputNodeIndex = NODE_COUNT_GENERATOR + NODE_COUNT_GENERATOR + 0;
    CONNECTIONS[connection_index].m_InputIndex = 0;
    CONNECTIONS[connection_index].m_OutputNodeIndex = 1;
    CONNECTIONS[connection_index++].m_OutputIndex = 0;

    CONNECTIONS[connection_index].m_InputNodeIndex = NODE_COUNT_GENERATOR + NODE_COUNT_GENERATOR + 0;
    CONNECTIONS[connection_index].m_InputIndex = 1;
    CONNECTIONS[connection_index].m_OutputNodeIndex = NODE_COUNT_GENERATOR + 1;
    CONNECTIONS[connection_index++].m_OutputIndex = 0;

    static const sogo::TNodeIndex REMOVE_START_INDEX = NODE_COUNT_GENERATOR;
    for (sogo::TNodeIndex n = REMOVE_START_INDEX; n < node_index - NODE_COUNT_GENERATOR; ++n)
    {
        NODES[n] = NODES[n + NODE_COUNT_GENERATOR];
    }
    sogo::TConnectionIndex w = 0;
    sogo::TConnectionIndex r = 0;
    while (r < connection_index)
    {
        if ((CONNECTIONS[r].m_InputNodeIndex >= REMOVE_START_INDEX && CONNECTIONS[r].m_InputNodeIndex < REMOVE_START_INDEX + NODE_COUNT_GENERATOR) ||
            (CONNECTIONS[r].m_OutputNodeIndex >= REMOVE_START_INDEX && CONNECTIONS[r].m_OutputNodeIndex < REMOVE_START_INDEX + NODE_COUNT_GENERATOR))
        {
            ++r;
            continue;
        }
        CONNECTIONS[w] = CONNECTIONS[r];
        if (CONNECTIONS[w].m_InputNodeIndex >= REMOVE_START_INDEX + NODE_COUNT_GENERATOR)
        {
            CONNECTIONS[w].m_InputNodeIndex -= NODE_COUNT_GENERATOR;
        }
        if (CONNECTIONS[w].m_OutputNodeIndex >= REMOVE_START_INDEX + NODE_COUNT_GENERATOR)
        {
            CONNECTIONS[w].m_OutputNodeIndex -= NODE_COUNT_GENERATOR;
        }
        ++w;
        ++r;
    }

    node_index -= NODE_COUNT_GENERATOR;
    connection_index = w;

    sogo::GraphDescription GRAPH_DESCRIPTION =
    {
        node_index,
        NODES,
        connection_index,
        CONNECTIONS,
        0x0
    };

    ASSERT_EQ((NODE_COUNT_GENERATOR + NODE_COUNT_MIXER), GRAPH_DESCRIPTION.m_NodeCount);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_NodeDescriptions[0], &sogo::DCNodeDescription);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_NodeDescriptions[1], &sogo::GainNodeDescription);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_NodeDescriptions[2], &sogo::MergeNodeDescription);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_NodeDescriptions[3], &sogo::GainNodeDescription);

    ASSERT_EQ(CONNECTION_COUNT_GENERATOR + CONNECTION_COUNT_MIXER + 1, GRAPH_DESCRIPTION.m_ConnectionCount);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_NodeConnections[0].m_InputNodeIndex, 1);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_NodeConnections[0].m_InputIndex, 0);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_NodeConnections[0].m_OutputNodeIndex, 0);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_NodeConnections[0].m_OutputIndex, 0);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_NodeConnections[1].m_InputNodeIndex, 3);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_NodeConnections[1].m_InputIndex, 0);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_NodeConnections[1].m_OutputNodeIndex, 2);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_NodeConnections[1].m_OutputIndex, 0);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_NodeConnections[2].m_InputNodeIndex, 2);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_NodeConnections[2].m_InputIndex, 0);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_NodeConnections[2].m_OutputNodeIndex, 1);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_NodeConnections[2].m_OutputIndex, 0);
}

TEST_BEGIN(sogo_test, sogo_main_setup, sogo_main_teardown, test_setup, test_teardown)
    TEST(sogo_create)
    TEST(sogo_simple_graph)
    TEST(sogo_merge_graphs)
TEST_END(sogo_test)
