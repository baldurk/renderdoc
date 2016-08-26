#include "gles_manager.h"
#include "gles_resources.h"

ResourceId GLESResourceManager::GetID(GLESResource res)
{
    auto it = m_CurrentResourceIds.find(res);
    if (it != m_CurrentResourceIds.end())
        return it->second;
    return ResourceId();
}

bool GLESResourceManager::SerialisableResource(ResourceId id, GLESResourceRecord *record)
{
/*
  if(id == m_GL->GetContextResourceID())
    return false;
  return true;
*/
    return true;
}

bool GLESResourceManager::Need_InitialStateChunk(GLESResource res)
{
  return false;
}

bool GLESResourceManager::Prepare_InitialState(GLESResource res, byte *blob)
{
    return true;
}

bool GLESResourceManager::Prepare_InitialState(GLESResource res)
{
    return true;
}

bool GLESResourceManager::Serialise_InitialState(ResourceId resid, GLESResource res)
{
    ResourceId Id = ResourceId();

    if(m_State >= WRITING)
    {
        Id = GetID(res);

        m_pSerialiser->Serialise("Id", Id);
    }
    else
    {
        m_pSerialiser->Serialise("Id", Id);
    }

    return true;
}
