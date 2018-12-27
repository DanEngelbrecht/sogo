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
    sogo::GraphDescription empty_graph_description =
    {
        0,
        0x0,
        0,
        0x0,
        0x0
    };
    sogo::HGraph empty_graph = sogo::CreateGraph(44100, 128, &empty_graph_description);
    TEST_ASSERT_NE(0x0, empty_graph);
    TEST_ASSERT_TRUE(RenderGraph(empty_graph, 64));
    sogo::DisposeGraph(empty_graph);
}

static void sogo_simple_graph(SCtx* )
{
    static const uint32_t NODE_COUNT = 6;
    const sogo::NodeDescription* NODES[NODE_COUNT] =
    {
        &sogo::SineNodeDescription,
        &sogo::GainNodeDescription,
        &sogo::ToStereoNodeDescription,
        &sogo::SplitNodeDescription,
        &sogo::GainNodeDescription,
        &sogo::MergeNodeDescription
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

    sogo::HGraph graph = sogo::CreateGraph(
        44100,
        MAX_BATCH_SIZE,
        &GRAPH_DESCRIPTION);

    sogo::TParameterNameHash gain_parameter_hash = sogo::MakeParameterHash(1, "Gain");
    sogo::SetParameter(graph, gain_parameter_hash, 0.5f);

    for (uint32_t i = 0; i < 6; ++i)
    {
        ASSERT_TRUE(sogo::RenderGraph(graph, MAX_BATCH_SIZE));
        sogo::RenderOutput* render_output = sogo::GetOutput(graph, 5, 0);
        ASSERT_TRUE(render_output != 0x0);
        ASSERT_EQ(2, render_output->m_ChannelCount);
        ASSERT_TRUE(render_output->m_Buffer != 0x0);
    }
    sogo::DisposeGraph(graph);
}

static void sogo_merge_graphs(SCtx* )
{
    
}

TEST_BEGIN(sogo_test, sogo_main_setup, sogo_main_teardown, test_setup, test_teardown)
    TEST(sogo_create)
    TEST(sogo_simple_graph)
    TEST(sogo_merge_graphs)
TEST_END(sogo_test)

