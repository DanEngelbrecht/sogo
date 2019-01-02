#pragma once

#include "sogo.h"

namespace sogo {
    typedef struct Access* HAccess;

    typedef uint32_t TParameterNameHash;
    typedef uint32_t TTriggerNameHash;

    bool GetAccessSize(const GraphDescription* graph_description, size_t& out_access_size);
    HAccess CreateAccess(void* mem, const GraphDescription* graph_description);

    TParameterNameHash MakeParameterHash(TNodeIndex node_index, const char* parameter_name);
    bool SetParameter(HAccess access, HGraph graph, TParameterNameHash parameter_hash, float value);

    TTriggerNameHash MakeTriggerHash(TNodeIndex node_index, const char* trigger_name);
    bool Trigger(HAccess access, HGraph graph, TTriggerNameHash trigger_hash);
}
