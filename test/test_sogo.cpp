#include "../src/sogo_nodes.h"

#include <memory>

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
    size_t graph_size = sogo::GetGraphSize(MAX_BATCH_SIZE, &GRAPH_DESCRIPTION);
    void* graph_mem = malloc(graph_size);
    ASSERT_NE(0x0, graph_mem);

    sogo::HGraph graph = sogo::CreateGraph(
        graph_mem,
        44100,
        MAX_BATCH_SIZE,
        &GRAPH_DESCRIPTION);

    TEST_ASSERT_NE(0x0, graph);
    TEST_ASSERT_TRUE(RenderGraph(graph, 64));
    sogo::DisposeGraph(graph);
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

    size_t graph_size = sogo::GetGraphSize(MAX_BATCH_SIZE, &GRAPH_DESCRIPTION);
    void* graph_mem = malloc(graph_size);
    ASSERT_NE(0x0, graph_mem);

    sogo::HGraph graph = sogo::CreateGraph(
        graph_mem,
        44100,
        MAX_BATCH_SIZE,
        &GRAPH_DESCRIPTION);

    sogo::TParameterNameHash level_parameter_hash = sogo::MakeParameterHash(0, "Level");
    ASSERT_TRUE(sogo::SetParameter(graph, level_parameter_hash, 0.5f));

    sogo::TParameterNameHash gain_parameter_hash = sogo::MakeParameterHash(1, "Gain");
    ASSERT_TRUE(sogo::SetParameter(graph, gain_parameter_hash, 0.5f));

    sogo::TParameterNameHash gain2_parameter_hash = sogo::MakeParameterHash(4, "Gain");
    ASSERT_TRUE(sogo::SetParameter(graph, gain2_parameter_hash, 2.f));

    for (uint32_t i = 0; i < 6; ++i)
    {
        ASSERT_TRUE(sogo::RenderGraph(graph, MAX_BATCH_SIZE));
        sogo::RenderOutput* render_output = sogo::GetOutput(graph, 5, 0);

        ASSERT_TRUE(render_output != 0x0);
        ASSERT_EQ(2, render_output->m_ChannelCount);

        if (i > 0)
        {
            // Skip first batch since it has filtering on gain
            static const float expected_signal = ((0.5f * 0.5f) * 2.f) + (0.5f * 0.5f);
            for (sogo::TFrameCount f = 0; f < MAX_BATCH_SIZE * 2; ++f)
            {
                ASSERT_EQ(expected_signal, render_output->m_Buffer[f]);
            }
        }

        ASSERT_TRUE(render_output->m_Buffer != 0x0);
    }
    sogo::DisposeGraph(graph);
    free(graph_mem);
}

static void sogo_merge_graphs(SCtx* )
{
    
}

TEST_BEGIN(sogo_test, sogo_main_setup, sogo_main_teardown, test_setup, test_teardown)
    TEST(sogo_create)
    TEST(sogo_simple_graph)
    TEST(sogo_merge_graphs)
TEST_END(sogo_test)

