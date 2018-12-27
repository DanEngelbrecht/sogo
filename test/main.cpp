#define JC_TEST_IMPLEMENTATION
#include "../third-party/containers/test/jc_test.h"
#include "test_sogo.cpp"

int main(int argc, const char** argv)
{
	(void)argc;
	(void)argv;

	RUN_ALL();
	return 0;
}

#if 0
TEST(dmSoundTest, SogoSimpleGraph)
{
    static const uint32_t NODE_COUNT = 6;
    const dmSogo::NodeDescription* NODES[NODE_COUNT] =
    {
        &dmSogo::SineNodeDescription,
        &dmSogo::GainNodeDescription,
        &dmSogo::ToStereoNodeDescription,
        &dmSogo::SplitNodeDescription,
        &dmSogo::GainNodeDescription,
        &dmSogo::MergeNodeDescription
    };

    static const uint16_t CONNECTION_COUNT = 6;
    dmSogo::NodeConnection CONNECTIONS[CONNECTION_COUNT] =
    {
        { 0, 0, 1, 0 },
        { 1, 0, 2, 0 },
        { 2, 0, 3, 0 },
        { 3, 0, 4, 0 },
        { 4, 0, 5, 0 },
        { 3, 1, 5, 1 }
    };

    dmSogo::GraphDescription GRAPH_DESCRIPTION =
    {
        NODE_COUNT,
        NODES,
        CONNECTION_COUNT,
        CONNECTIONS,
        0x0
    };

    static const uint32_t MAX_BATCH_SIZE = 128;

    dmSogo::HGraph graph = dmSogo::CreateGraph(
        44100,
        MAX_BATCH_SIZE,
        &GRAPH_DESCRIPTION);

    uint32_t gain_parameter_hash = dmSogo::MakeParameterHash(1, "Gain");
    dmSogo::SetParameter(graph, gain_parameter_hash, 0.5f);

    for (uint32_t i = 0; i < 6; ++i)
    {
        ASSERT_TRUE(dmSogo::RenderGraph(graph, MAX_BATCH_SIZE));
        dmSogo::RenderOutput* render_output = dmSogo::GetOutput(graph, 5, 0);
        ASSERT_TRUE(render_output != 0x0);
        ASSERT_EQ(2, render_output->m_ChannelCount);
        ASSERT_TRUE(render_output->m_Buffer != 0x0);
    }
    dmSogo::DisposeGraph(graph);
}

TEST(dmSoundTest, SogoMultiGraph)
{
    static const uint32_t GENERATOR_NODE_COUNT = 2;
    const dmSogo::NodeDescription* GENERATOR_NODES[GENERATOR_NODE_COUNT] =
    {
        &dmSogo::SineNodeDescription,
        &dmSogo::GainNodeDescription
    };

    static const uint16_t GENERATOR_CONNECTION_COUNT = 1;
    dmSogo::NodeConnection GENERATOR_CONNECTIONS[GENERATOR_CONNECTION_COUNT] =
    {
        { 0, 0, 1, 0 }
    };

    dmSogo::GraphDescription GENERATOR_GRAPH_DESCRIPTION =
    {
        GENERATOR_NODE_COUNT,
        GENERATOR_NODES,
        GENERATOR_CONNECTION_COUNT,
        GENERATOR_CONNECTIONS,
        0x0
    };

    static const uint32_t MAX_BATCH_SIZE = 128;

    dmSogo::HGraph generator1_graph = dmSogo::CreateGraph(
        44100,
        MAX_BATCH_SIZE,
        &GENERATOR_GRAPH_DESCRIPTION);

    dmSogo::HGraph generator2_graph = dmSogo::CreateGraph(
        44100,
        MAX_BATCH_SIZE,
        &GENERATOR_GRAPH_DESCRIPTION);

    dmSogo::OutputDescription generator1_output_descriptors[1];
    dmSogo::NodeDescription generator1_node_description;
    dmSogo::MakeSubGraphOutputDescriptor(generator1_graph, generator1_output_descriptors[0]);
    dmSogo::MakeSubGraphNodeDescriptor(generator1_node_description, generator1_output_descriptors);

    dmSogo::OutputDescription generator2_output_descriptors[1];
    dmSogo::NodeDescription generator2_node_description;
    dmSogo::MakeSubGraphOutputDescriptor(generator2_graph, generator2_output_descriptors[0]);
    dmSogo::MakeSubGraphNodeDescriptor(generator2_node_description, generator2_output_descriptors);

    dmSogo::Resource generator1_graph_resource =
    {
        dmSogo::SOGO_DATA_TYPE_GRAPH,
        generator1_graph,
        0
    };

    dmSogo::Resource generator2_graph_resource =
    {
        dmSogo::SOGO_DATA_TYPE_GRAPH,
        generator2_graph,
        0
    };

    static const uint32_t MIXER_NODE_COUNT = 4;
    const dmSogo::NodeDescription* MIXER_NODES[MIXER_NODE_COUNT] =
    {
        &generator1_node_description,
        &generator2_node_description,
        &dmSogo::MergeNodeDescription,
        &dmSogo::GainNodeDescription
    };

    static const uint16_t MIXER_CONNECTION_COUNT = 3;
    dmSogo::NodeConnection MIXER_CONNECTIONS[MIXER_CONNECTION_COUNT] =
    {
        { 0, 0, 2, 0 },
        { 1, 0, 2, 1 },
        { 2, 0, 3, 0 }
    };

    dmSogo::GraphDescription MIXER_GRAPH_DESCRIPTION =
    {
        MIXER_NODE_COUNT,
        MIXER_NODES,
        MIXER_CONNECTION_COUNT,
        MIXER_CONNECTIONS,
        0x0
    };

    dmSogo::HGraph mixer_graph = dmSogo::CreateGraph(
        44100,
        MAX_BATCH_SIZE,
        &MIXER_GRAPH_DESCRIPTION);

    dmSogo::SetResource(mixer_graph, 0, 0, &generator1_graph_resource);
    dmSogo::SetResource(mixer_graph, 1, 0, &generator2_graph_resource);

    for (uint32_t i = 0; i < 6; ++i)
    {
        ASSERT_TRUE(dmSogo::RenderGraph(mixer_graph, MAX_BATCH_SIZE));
        dmSogo::RenderOutput* render_output = dmSogo::GetOutput(mixer_graph, 1, 0);
        ASSERT_TRUE(render_output != 0x0);
        ASSERT_EQ(1, render_output->m_ChannelCount);
        ASSERT_TRUE(render_output->m_Buffer != 0x0);
    }
    dmSogo::DisposeGraph(mixer_graph);
    dmSogo::DisposeGraph(generator2_graph);
    dmSogo::DisposeGraph(generator1_graph);
}

#endif // 0
