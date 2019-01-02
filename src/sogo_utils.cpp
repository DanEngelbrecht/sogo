#include "sogo_utils.h"

#include "../third-party/containers/src/jc/hashtable.h"
#include "../third-party/xxHash-1/xxhash.h"

#include <new>

#define ALIGN_SIZE(x, align)    (((x) + ((align) - 1)) & ~((align) - 1))

namespace sogo {

struct ParameterTarget
{
    TNodeIndex m_NodeIndex;
    TParameterIndex m_ParameterIndex;
};

struct TriggerTarget
{
    TNodeIndex m_NodeIndex;
    TTriggerIndex m_TriggerIndex;
};

typedef jc::HashTable<TParameterNameHash, ParameterTarget> TParameterLookup;
typedef jc::HashTable<TTriggerNameHash, TriggerTarget> TTriggerLookup;

uint32_t GetParameterLookupSize(uint32_t num_elements)
{
    const uint32_t load_factor    = 100; // percent
    const uint32_t tablesize      = uint32_t(num_elements / (load_factor/100.0f));
    return TParameterLookup::CalcSize(tablesize);
}

uint32_t GetTriggerLookupSize(uint32_t num_elements)
{
    const uint32_t load_factor    = 100; // percent
    const uint32_t tablesize      = uint32_t(num_elements / (load_factor/100.0f));
    return TTriggerLookup::CalcSize(tablesize);
}

struct Access {
    Access(TParameterIndex named_parameter_count,
        TTriggerIndex named_trigger_count,
        void* parameter_lookup_data,
        void* trigger_lookup_data);

    TParameterLookup m_ParameterLookup;
    TTriggerLookup m_TriggerLookup;
};

Access::Access(TParameterIndex named_parameter_count,
        TTriggerIndex named_trigger_count,
        void* parameter_lookup_data,
        void* trigger_lookup_data)
    : m_ParameterLookup(named_parameter_count, parameter_lookup_data)
    , m_TriggerLookup(named_trigger_count, trigger_lookup_data)
{
    
}    
       
struct AccessProperties
{
    TParameterIndex m_NamedParameterCount;
    TTriggerIndex m_NamedTriggerCount;
};

static bool GetGraphProperties(
    const GraphDescription* graph_description,
    AccessProperties* out_access_properties)
{
    TParameterIndex named_parameter_count = 0;
    TTriggerIndex named_trigger_count = 0;
    for (TNodeIndex i = 0; i < graph_description->m_NodeCount; ++i)
    {
        const NodeDescription* node_description = graph_description->m_NodeDescriptions[i];
        for (TParameterIndex p = 0; p < node_description->m_ParameterCount; ++p)
        {
            if (node_description->m_Parameters[p].m_ParameterName != 0x0)
            {
                named_parameter_count += 1;
            }
        }
        for (TTriggerIndex t = 0; t < node_description->m_TriggerCount; ++t)
        {
            if (node_description->m_Triggers[t].m_TriggerName != 0x0)
            {
                named_trigger_count += 1;
            }
        }
    }
    out_access_properties->m_NamedParameterCount = named_parameter_count;
    out_access_properties->m_NamedTriggerCount = named_trigger_count;
    return true;
}

static size_t GetAccessSize(
    const GraphDescription* graph_description,
    const AccessProperties* access_properties)
{
    size_t s = ALIGN_SIZE(sizeof(Access), sizeof(void*)) +
    ALIGN_SIZE(GetParameterLookupSize(access_properties->m_NamedParameterCount), sizeof(void*)) +
    ALIGN_SIZE(GetTriggerLookupSize(access_properties->m_NamedTriggerCount), sizeof(float));
    return s;
}

bool GetAccessSize(
    const GraphDescription* graph_description,
    size_t& out_access_size)
{
    AccessProperties access_properties;
    if (!GetGraphProperties(graph_description, &access_properties))
    {
        return false;
    }
    out_access_size = GetAccessSize(graph_description, &access_properties);
    return true;
}

TParameterNameHash MakeParameterHash(TNodeIndex node_index, const char* parameter_name)
{
    XXH32_hash_t index_hash = XXH32(&node_index, sizeof(TNodeIndex), 0);
    XXH32_hash_t hash = XXH32(parameter_name, strlen(parameter_name), index_hash);
    return (TParameterNameHash)hash;
}

TTriggerNameHash MakeTriggerHash(TNodeIndex node_index, const char* trigger_name)
{
    XXH32_hash_t index_hash = XXH32(&node_index, sizeof(TNodeIndex), 0);
    XXH32_hash_t hash = XXH32(trigger_name, strlen(trigger_name), index_hash);
    return (TTriggerNameHash)hash;
}

static bool RegisterNamedParameter(HAccess access, TNodeIndex node_index, TParameterIndex parameter_index, const char* parameter_name)
{
    uint32_t node_key = MakeParameterHash(node_index, parameter_name);
    ParameterTarget target;
    target.m_NodeIndex = node_index;
    target.m_ParameterIndex = parameter_index;
    access->m_ParameterLookup.Put(node_key, target);
    return true;
}

static bool RegisterNamedTrigger(HAccess access, TNodeIndex node_index, TTriggerIndex trigger_index, const char* trigger_name)
{
    uint32_t node_key = MakeTriggerHash(node_index, trigger_name);
    TriggerTarget target;
    target.m_NodeIndex = node_index;
    target.m_TriggerIndex = trigger_index;
    access->m_TriggerLookup.Put(node_key, target);
    return true;
}

HAccess CreateAccess(
    void* access_mem,
    const GraphDescription* graph_description)
{
    AccessProperties access_properties;
    if (!GetGraphProperties(graph_description, &access_properties))
    {
        return 0x0;
    }

    uint8_t* ptr = (uint8_t*)access_mem;
    size_t offset = ALIGN_SIZE(sizeof(Access), sizeof(void*));

    void* parameter_lookup_data = &ptr[offset];
    offset += ALIGN_SIZE(GetParameterLookupSize(access_properties.m_NamedParameterCount), sizeof(void*));

    void* trigger_lookup_data = &ptr[offset];
    offset += ALIGN_SIZE(GetTriggerLookupSize(access_properties.m_NamedTriggerCount), sizeof(float));

    HAccess access = new (access_mem) Access(
        access_properties.m_NamedParameterCount,
        access_properties.m_NamedTriggerCount,
        parameter_lookup_data,
        trigger_lookup_data);

    TParameterIndex parameters_offset = 0;
    TTriggerIndex triggers_offset = 0;

    for (TNodeIndex node_index = 0; node_index < graph_description->m_NodeCount; ++node_index)
    {
        const NodeDescription* node_description = graph_description->m_NodeDescriptions[node_index];
        for (TParameterIndex i = 0; i < node_description->m_ParameterCount; ++i)
        {
            const ParameterDescription* parameter_description = &node_description->m_Parameters[i];
            if (parameter_description->m_ParameterName != 0x0)
            {
                if (!RegisterNamedParameter(access, node_index, i, parameter_description->m_ParameterName))
                {
                    return 0x0;
                }
            }
        }

        for (TTriggerIndex i = 0; i < node_description->m_TriggerCount; ++i)
        {
            const TriggerDescription* trigger_description = &node_description->m_Triggers[i];
            if (trigger_description->m_TriggerName != 0x0)
            {
                if (!RegisterNamedTrigger(access, node_index, i, trigger_description->m_TriggerName))
                {
                    return 0x0;
                }
            }
        }
    }
    return access;
}

bool SetParameter(HAccess access, HGraph graph, TParameterNameHash parameter_hash, float value)
{
    ParameterTarget* target = access->m_ParameterLookup.Get(parameter_hash);
    if (target == 0x0)
    {
        return false;
    }
    return SetParameter(graph, target->m_NodeIndex, target->m_ParameterIndex, value);
}

bool Trigger(HAccess access, HGraph graph, TTriggerNameHash trigger_hash)
{
    TriggerTarget* target = access->m_ParameterLookup.Get(trigger_hash);
    if (target == 0x0)
    {
        return false;
    }
    return Trigger(graph, target->m_NodeIndex, target->m_TriggerIndex);
}
}
