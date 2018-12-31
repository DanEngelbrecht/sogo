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
    TParameterIndex m_NameParameterCount;
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
    out_access_properties->m_NameParameterCount = named_parameter_count;
    out_access_properties->m_NamedTriggerCount = named_trigger_count;
    return true;
}

bool GetAccessSize(
    const GraphDescription* graph_description,
    size_t& out_access_size)
{

}

}
