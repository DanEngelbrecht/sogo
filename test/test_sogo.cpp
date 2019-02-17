#include "../src/sogo_nodes.h"
#include "../src/sogo_utils.h"
#include "../third-party/nadir/src/nadir.h"
#include "../third-party/bikeshed/src/bikeshed.h"

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
    static const sogo::TFrameRate FRAME_RATE = 44100;
    static const sogo::TFrameIndex MAX_BATCH_SIZE = 128;
    static const sogo::TFrameIndex BATCH_SIZE = 64;
    static const sogo::TTriggerCount MAX_TRIGGER_EVENT_COUNT = 32;
    sogo::GraphRuntimeSettings GRAPH_RUNTIME_SETTINGS =
    {
        FRAME_RATE,
        MAX_BATCH_SIZE,
        MAX_TRIGGER_EVENT_COUNT
    };
    sogo::GraphDescription GRAPH_DESCRIPTION =
    {
        0,
        0x0,
        0
    };

    sogo::GraphSize graph_size;
    ASSERT_TRUE(sogo::GetGraphSize(&GRAPH_DESCRIPTION, &GRAPH_RUNTIME_SETTINGS, &graph_size));

    size_t s = ALIGN_SIZE(graph_size.m_GraphSize, sizeof(float)) +
               ALIGN_SIZE(graph_size.m_ScratchBufferSize, sizeof(sogo::TTriggerSocketIndex)) +
               ALIGN_SIZE(graph_size.m_TriggerBufferSize, 1) +
               ALIGN_SIZE(graph_size.m_ContextMemorySize, 1);
    uint8_t* mem = (uint8_t*)malloc(s);
    ASSERT_NE(0x0, mem);
    sogo::GraphBuffers graph_buffers;
    graph_buffers.m_GraphMem = mem;
    graph_buffers.m_ScratchBufferMem = &mem[ALIGN_SIZE(graph_size.m_GraphSize, sizeof(float))];
    graph_buffers.m_TriggerBufferMem = &mem[ALIGN_SIZE(graph_size.m_GraphSize, sizeof(float)) + ALIGN_SIZE(graph_size.m_ScratchBufferSize, sizeof(sogo::TTriggerSocketIndex))];
    graph_buffers.m_ContextMem = &mem[ALIGN_SIZE(graph_size.m_GraphSize, sizeof(float)) + ALIGN_SIZE(graph_size.m_ScratchBufferSize, sizeof(sogo::TTriggerSocketIndex)) + ALIGN_SIZE(graph_size.m_TriggerBufferSize, 1)];

    sogo::HGraph graph = sogo::CreateGraph(&GRAPH_DESCRIPTION, &GRAPH_RUNTIME_SETTINGS, &graph_buffers);
    TEST_ASSERT_NE(0x0, graph);

    RenderGraph(graph, BATCH_SIZE);
    free(mem);
}

static void sogo_simple_graph(SCtx* )
{
    static const uint32_t NODE_COUNT = 6;
    static const sogo::TFrameRate FRAME_RATE = 44100;
    static const sogo::TFrameIndex MAX_BATCH_SIZE = 128;
    static const sogo::TTriggerCount MAX_TRIGGER_EVENT_COUNT = 32;
    sogo::GraphRuntimeSettings GRAPH_RUNTIME_SETTINGS =
    {
        FRAME_RATE,
        MAX_BATCH_SIZE,
        MAX_TRIGGER_EVENT_COUNT
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

    static const sogo::NodeAudioConnection NODE_AUDIO_CONNECTIONS[CONNECTION_COUNT] =
    {
        { 0, -1, 0 },
        { 0, -1, 0 },
        { 0, -1, 0 },
        { 0, -1, 0 },
        { 0, -1, 0 },
        { 1, -2, 1 }
    };

    const sogo::NodeDescription NODES[NODE_COUNT] =
    {
        {
            sogo::DCNodeDesc,           // 0.5
            0,
            0
        },
        {
            sogo::GainNodeDesc,         // 0.5 * 0.5
            1,
            0
        },
        {
            sogo::ToStereoNodeDesc,     // 0.5 * 0.5
            1,
            0
        },
        {
            sogo::SplitNodeDesc,        // 0.5 * 0.5
            1,
            0
        },
        {
            sogo::GainNodeDesc,         // 0.5 * 0.5 * 2.0
            1,
            0
        },
        {
            sogo::MergeNodeDesc,        // 0.5 * 0.5 * 2.0 + 0.5 * 0.5
            2,
            0
        }
    };

    sogo::GraphDescription GRAPH_DESCRIPTION =
    {
        NODE_COUNT,
        NODES,
        NODE_AUDIO_CONNECTIONS,
        0x0,
        0x0
    };

    sogo::GraphSize graph_size;
    ASSERT_TRUE(sogo::GetGraphSize(&GRAPH_DESCRIPTION, &GRAPH_RUNTIME_SETTINGS, &graph_size));

    size_t s = ALIGN_SIZE(graph_size.m_GraphSize, sizeof(float)) +
               ALIGN_SIZE(graph_size.m_ScratchBufferSize, sizeof(sogo::TTriggerSocketIndex)) +
               ALIGN_SIZE(graph_size.m_TriggerBufferSize, 1) +
               ALIGN_SIZE(graph_size.m_ContextMemorySize, 1);
    uint8_t* mem = (uint8_t*)malloc(s);
    ASSERT_NE(0x0, mem);
    sogo::GraphBuffers graph_buffers;
    graph_buffers.m_GraphMem = mem;
    graph_buffers.m_ScratchBufferMem = &mem[ALIGN_SIZE(graph_size.m_GraphSize, sizeof(float))];
    graph_buffers.m_TriggerBufferMem = &mem[ALIGN_SIZE(graph_size.m_GraphSize, sizeof(float)) + ALIGN_SIZE(graph_size.m_ScratchBufferSize, sizeof(sogo::TTriggerSocketIndex))];
    graph_buffers.m_ContextMem = &mem[ALIGN_SIZE(graph_size.m_GraphSize, sizeof(float)) + ALIGN_SIZE(graph_size.m_ScratchBufferSize, sizeof(sogo::TTriggerSocketIndex)) + ALIGN_SIZE(graph_size.m_TriggerBufferSize, 1)];

    sogo::HGraph graph = sogo::CreateGraph(&GRAPH_DESCRIPTION, &GRAPH_RUNTIME_SETTINGS, &graph_buffers);
    TEST_ASSERT_NE(0x0, graph);

    struct sogo::AccessDescription ACCESS_DESCRIPTION =
    {
        &GRAPH_DESCRIPTION,
        &GRAPH_RUNTIME_SETTINGS,
        NODE_NAMES
    };

    sogo::TAccessSize access_size = 0;
    ASSERT_TRUE(sogo::GetAccessSize(&ACCESS_DESCRIPTION, access_size));
    void* access_mem = malloc(access_size);

    sogo::HAccess access = sogo::CreateAccess(access_mem, &ACCESS_DESCRIPTION);
    ASSERT_NE(0x0, access);

    sogo::TParameterNameHash level_parameter_hash = sogo::MakeParameterHash(sogo::MakeNodeNameHash(NODE_NAMES[0]), "Level");
    ASSERT_TRUE(sogo::SetParameter(access, graph, level_parameter_hash, sogo::TParameter {0.5f}));

    sogo::TParameterNameHash gain_parameter_hash = sogo::MakeParameterHash(sogo::MakeNodeNameHash(NODE_NAMES[1]), "Gain");
    ASSERT_TRUE(sogo::SetParameter(access, graph, gain_parameter_hash, sogo::TParameter {0.5f}));

    sogo::TParameterNameHash gain2_parameter_hash = sogo::MakeParameterHash(sogo::MakeNodeNameHash(NODE_NAMES[4]), "Gain");
    ASSERT_TRUE(sogo::SetParameter(access, graph, gain2_parameter_hash, sogo::TParameter {2.f}));

    for (uint32_t i = 0; i < 6; ++i)
    {
        sogo::RenderGraph(graph, MAX_BATCH_SIZE);
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
    free(access_mem);
    free(mem);
}

static void sogo_merge_graphs(SCtx* )
{
    // This is nore of an authoring test, checking how easy it is to modify graphs
    static const sogo::TConnectionIndex GENERATOR_CONNECTION_COUNT = 1;
    static const sogo::NodeAudioConnection GENERATOR_CONNECTIONS[GENERATOR_CONNECTION_COUNT] = 
    {
        { 0, -1, 0 }
    };

    static const sogo::TNodeIndex NODE_COUNT_GENERATOR = 2;
    const sogo::NodeDescription NODES_GENERATOR[NODE_COUNT_GENERATOR] =
    {
        {
            sogo::DCNodeDesc,
            0,
            0
        },
        {
        sogo::GainNodeDesc,
            1,
            0
        }
    };

    sogo::GraphDescription GRAPH_DESCRIPTION_GENERATOR =
    {
        NODE_COUNT_GENERATOR,
        NODES_GENERATOR,
        GENERATOR_CONNECTIONS,
        0x0,
        0x0
    };

    static const sogo::TConnectionIndex MIXER_CONNECTION_COUNT = 1;
    static const sogo::NodeAudioConnection MIXER_CONNECTIONS[MIXER_CONNECTION_COUNT] = 
    {
        { 0, -1, 0 }
    };

    static const sogo::TNodeIndex NODE_COUNT_MIXER = 2;
    const sogo::NodeDescription NODES_MIXER[NODE_COUNT_MIXER] =
    {
        {
            sogo::MergeNodeDesc,
            0,
            0
        },
        {
        sogo::GainNodeDesc,
            1,
            0
        }
    };


    sogo::GraphDescription GRAPH_DESCRIPTION_MIXER =
    {
        NODE_COUNT_MIXER,
        NODES_MIXER,
        MIXER_CONNECTIONS,
        0x0,
        0x0
    };

    // Build graph of two generators going into a mixer
    static const sogo::TNodeIndex NODE_COUNT = NODE_COUNT_GENERATOR + NODE_COUNT_GENERATOR + NODE_COUNT_MIXER;
    sogo::NodeDescription NODES[NODE_COUNT];
    static const sogo::TConnectionIndex CONNECTION_COUNT = GENERATOR_CONNECTION_COUNT + GENERATOR_CONNECTION_COUNT + MIXER_CONNECTION_COUNT + 2;
    sogo::NodeAudioConnection CONNECTIONS[CONNECTION_COUNT]; 

    // Merge the three graphs
    sogo::TNodeIndex node_index = 0;

    for (sogo::TNodeIndex n = 0; n < NODE_COUNT_GENERATOR; ++n)
    {
        NODES[node_index++] = NODES_GENERATOR[n];
    }
    for (sogo::TNodeIndex n = 0; n < NODE_COUNT_GENERATOR; ++n)
    {
        NODES[node_index++] = NODES_GENERATOR[n];
    }
    for (sogo::TNodeIndex n = 0; n < NODE_COUNT_MIXER; ++n)
    {
        NODES[node_index++] = NODES_MIXER[n];
    }
    sogo::TConnectionIndex connection_index = 0;
    for (sogo::TConnectionIndex c = 0; c < GENERATOR_CONNECTION_COUNT; ++c)
    {
        CONNECTIONS[connection_index++] = GENERATOR_CONNECTIONS[c];
    }
    for (sogo::TConnectionIndex c = 0; c < GENERATOR_CONNECTION_COUNT; ++c)
    {
        CONNECTIONS[connection_index++] = GENERATOR_CONNECTIONS[c];
    }

    NODES[NODE_COUNT_GENERATOR + NODE_COUNT_GENERATOR].m_AudioConnectionCount = 2;
    CONNECTIONS[connection_index++] = { 0, -(NODE_COUNT_GENERATOR + 1), 0 };
    CONNECTIONS[connection_index++] = { 1, -(1), 0 };

    for (sogo::TConnectionIndex c = 0; c < MIXER_CONNECTION_COUNT; ++c)
    {
        CONNECTIONS[connection_index++] = MIXER_CONNECTIONS[c];
    }

    const sogo::GraphDescription GRAPH_DESCRIPTION = 
    {
        node_index,
        NODES,
        CONNECTIONS,
        0x0,
        0x0
    };

    ASSERT_EQ((NODE_COUNT_GENERATOR + NODE_COUNT_GENERATOR + NODE_COUNT_MIXER), GRAPH_DESCRIPTION.m_NodeCount);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_NodeDescriptions[0].m_NodeStaticDescription.m_GetNodeRuntimeDescCallback, sogo::DCNodeDesc.m_GetNodeRuntimeDescCallback);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_NodeDescriptions[1].m_NodeStaticDescription.m_GetNodeRuntimeDescCallback, sogo::GainNodeDesc.m_GetNodeRuntimeDescCallback);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_NodeDescriptions[2].m_NodeStaticDescription.m_GetNodeRuntimeDescCallback, sogo::DCNodeDesc.m_GetNodeRuntimeDescCallback);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_NodeDescriptions[3].m_NodeStaticDescription.m_GetNodeRuntimeDescCallback, sogo::GainNodeDesc.m_GetNodeRuntimeDescCallback);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_NodeDescriptions[4].m_NodeStaticDescription.m_GetNodeRuntimeDescCallback, sogo::MergeNodeDesc.m_GetNodeRuntimeDescCallback);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_NodeDescriptions[5].m_NodeStaticDescription.m_GetNodeRuntimeDescCallback, sogo::GainNodeDesc.m_GetNodeRuntimeDescCallback);

    ASSERT_EQ(GRAPH_DESCRIPTION.m_NodeDescriptions[0].m_AudioConnectionCount, 0);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_NodeDescriptions[1].m_AudioConnectionCount, 1);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_NodeDescriptions[2].m_AudioConnectionCount, 0);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_NodeDescriptions[3].m_AudioConnectionCount, 1);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_NodeDescriptions[4].m_AudioConnectionCount, 2);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_NodeDescriptions[5].m_AudioConnectionCount, 1);

    ASSERT_EQ(GRAPH_DESCRIPTION.m_AudioConnections[0].m_InputIndex, 0);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_AudioConnections[0].m_OutputNodeOffset, -1);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_AudioConnections[0].m_OutputIndex, 0);

    ASSERT_EQ(GRAPH_DESCRIPTION.m_AudioConnections[1].m_InputIndex, 0);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_AudioConnections[1].m_OutputNodeOffset, -1);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_AudioConnections[1].m_OutputIndex, 0);

    ASSERT_EQ(GRAPH_DESCRIPTION.m_AudioConnections[2].m_InputIndex, 0);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_AudioConnections[2].m_OutputNodeOffset, -3);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_AudioConnections[2].m_OutputIndex, 0);

    ASSERT_EQ(GRAPH_DESCRIPTION.m_AudioConnections[3].m_InputIndex, 1);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_AudioConnections[3].m_OutputNodeOffset, -1);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_AudioConnections[3].m_OutputIndex, 0);

    ASSERT_EQ(GRAPH_DESCRIPTION.m_AudioConnections[4].m_InputIndex, 0);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_AudioConnections[4].m_OutputNodeOffset, -1);
    ASSERT_EQ(GRAPH_DESCRIPTION.m_AudioConnections[4].m_OutputIndex, 0);
}

static void sogo_with_bikeshed(SCtx* )
{
    static const uint32_t NODE_COUNT = 6;
    static const sogo::TFrameRate FRAME_RATE = 44100;
    static const sogo::TFrameIndex MAX_BATCH_SIZE = 128;
    static const sogo::TTriggerCount MAX_TRIGGER_EVENT_COUNT = 32;
    sogo::GraphRuntimeSettings GRAPH_RUNTIME_SETTINGS =
    {
        FRAME_RATE,
        MAX_BATCH_SIZE,
        MAX_TRIGGER_EVENT_COUNT
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

    static const sogo::NodeAudioConnection NODE_AUDIO_CONNECTIONS[CONNECTION_COUNT] =
    {
        { 0, -1, 0 },
        { 0, -1, 0 },
        { 0, -1, 0 },
        { 0, -1, 0 },
        { 0, -1, 0 },
        { 1, -2, 1 }
    };

    const sogo::NodeDescription NODES[NODE_COUNT] =
    {
        {
            sogo::DCNodeDesc,           // 0.5
            0,
            0
        },
        {
            sogo::GainNodeDesc,         // 0.5 * 0.5
            1,
            0
        },
        {
            sogo::ToStereoNodeDesc,     // 0.5 * 0.5
            1,
            0
        },
        {
            sogo::SplitNodeDesc,        // 0.5 * 0.5
            1,
            0
        },
        {
            sogo::GainNodeDesc,         // 0.5 * 0.5 * 2.0
            1,
            0
        },
        {
            sogo::MergeNodeDesc,        // 0.5 * 0.5 * 2.0 + 0.5 * 0.5
            2,
            0
        }
    };

    sogo::GraphDescription GRAPH_DESCRIPTION =
    {
        NODE_COUNT,
        NODES,
        NODE_AUDIO_CONNECTIONS,
        0x0,
        0x0
    };

    sogo::GraphSize graph_size;
    ASSERT_TRUE(sogo::GetGraphSize(&GRAPH_DESCRIPTION, &GRAPH_RUNTIME_SETTINGS, &graph_size));

    size_t s = ALIGN_SIZE(graph_size.m_GraphSize, sizeof(float)) +
               ALIGN_SIZE(graph_size.m_ScratchBufferSize, sizeof(sogo::TTriggerSocketIndex)) +
               ALIGN_SIZE(graph_size.m_TriggerBufferSize, 1) +
               ALIGN_SIZE(graph_size.m_ContextMemorySize, 1);
    uint8_t* mem = (uint8_t*)malloc(s);
    ASSERT_NE(0x0, mem);
    sogo::GraphBuffers graph_buffers;
    graph_buffers.m_GraphMem = mem;
    graph_buffers.m_ScratchBufferMem = &mem[ALIGN_SIZE(graph_size.m_GraphSize, sizeof(float))];
    graph_buffers.m_TriggerBufferMem = &mem[ALIGN_SIZE(graph_size.m_GraphSize, sizeof(float)) + ALIGN_SIZE(graph_size.m_ScratchBufferSize, sizeof(sogo::TTriggerSocketIndex))];
    graph_buffers.m_ContextMem = &mem[ALIGN_SIZE(graph_size.m_GraphSize, sizeof(float)) + ALIGN_SIZE(graph_size.m_ScratchBufferSize, sizeof(sogo::TTriggerSocketIndex)) + ALIGN_SIZE(graph_size.m_TriggerBufferSize, 1)];

    sogo::HGraph graph = sogo::CreateGraph(&GRAPH_DESCRIPTION, &GRAPH_RUNTIME_SETTINGS, &graph_buffers);
    TEST_ASSERT_NE(0x0, graph);

    sogo::RenderJob render_jobs[NODE_COUNT];

    sogo::GetRenderJobs(graph, MAX_BATCH_SIZE, render_jobs);

    struct Worker
    {
        static bikeshed::TaskResult Render(bikeshed::HShed , bikeshed::TTaskID , void* context_data)
        {
            sogo::RenderJob* render_job = (sogo::RenderJob*)context_data;
            render_job->m_RenderCallback(render_job->m_Graph, render_job->m_Node, &render_job->m_RenderParameters);
            return bikeshed::TASK_RESULT_COMPLETE;
        }
    };

    bikeshed::TTaskID task_ids[NODE_COUNT];
    bikeshed::TaskFunc task_funcs[NODE_COUNT] =
    {
        Worker::Render,
        Worker::Render,
        Worker::Render,
        Worker::Render,
        Worker::Render,
        Worker::Render
    };
    void * task_context_data[NODE_COUNT] =
    {
        &render_jobs[0],
        &render_jobs[1],
        &render_jobs[2],
        &render_jobs[3],
        &render_jobs[4],
        &render_jobs[5]
    };

    bikeshed::HShed shed = bikeshed::CreateShed(malloc(bikeshed::GetShedSize(NODE_COUNT, NODE_COUNT)), NODE_COUNT, NODE_COUNT, 0);

    bikeshed::CreateTasks(shed, NODE_COUNT, task_funcs, task_context_data, task_ids);

    // A bit sad that we need to rebuild this task job list each batch even if
    // the graph does not change
    // IDEA: CreateSnapshot of TaskManager state and then just have a reset?
    bikeshed::TTaskID child_tasks[NODE_COUNT];
    for (sogo::TNodeIndex node_index = 0; node_index < NODE_COUNT; ++node_index)
    {
        uint16_t child_task_count = render_jobs[node_index].m_DependencyCount;
        if (child_task_count == 0)
        {
            bikeshed::ReadyTasks(shed, 1, &task_ids[node_index]);
        }
        else
        {
            for (sogo::TNodeIndex i = 0; i < child_task_count; ++i)
            {
                child_tasks[i] = task_ids[render_jobs[node_index].m_Dependencies[i]];
            }
            bikeshed::AddTaskDependencies(shed, task_ids[node_index], child_task_count, child_tasks);
        }
    }

    bikeshed::TTaskID next_ready_task = 0;
    while(true)
    {
        if (next_ready_task != 0)
        {
            bikeshed::ExecuteAndResolveTask(shed, next_ready_task, &next_ready_task);
            continue;
        }
        if (!bikeshed::ExecuteOneTask(shed, &next_ready_task))
        {
            // Nothing left to do, break!
            break;
        }
    }
    free(shed);
    free(graph);
}

TEST_BEGIN(sogo_test, sogo_main_setup, sogo_main_teardown, test_setup, test_teardown)
    TEST(sogo_create)
    TEST(sogo_simple_graph)
    TEST(sogo_merge_graphs)
    TEST(sogo_with_bikeshed)
TEST_END(sogo_test)
