#pragma once

#include "sogo.h"

namespace sogo {

typedef struct Access* HAccess;

typedef uint32_t TNodeNameHash;
typedef uint32_t TParameterNameHash;
typedef uint32_t TTriggerNameHash;
typedef uint32_t TAccessSize;

struct AccessDescription
{
    const GraphDescription*     m_GraphDescription;
    const GraphRuntimeSettings* m_GraphRuntimeSettings;
    const char**                m_NodeNames;
};

bool               GetAccessSize(const AccessDescription* access_description, TAccessSize& out_access_size);
HAccess            CreateAccess(void* mem, const AccessDescription* access_description);
TNodeNameHash      MakeNodeNameHash(const char* node_name);
TParameterNameHash MakeParameterHash(TNodeNameHash node_name_hash, const char* parameter_name);
bool               SetParameter(HAccess access, HGraph graph, TParameterNameHash parameter_hash, TParameter value);
TTriggerNameHash   MakeTriggerHash(TNodeNameHash node_name_hash, const char* trigger_name);
bool               Trigger(HAccess access, HGraph graph, TTriggerNameHash trigger_hash);

} // namespace sogo
