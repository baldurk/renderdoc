#pragma once

#include <map>
#include "core/resource_manager.h"
#include "gles_resources.h"

class WrappedGLES;

class GLESResourceManager : public ResourceManager<GLESResource, GLESResource, GLESResourceRecord>
{
public:
    GLESResourceManager(LogState state, Serialiser *serializer, WrappedGLES *gl)
        : ResourceManager(state, serializer)
        , m_GLES(gl)
    {
    }
    ~GLESResourceManager() {}

    ResourceId GetID(GLESResource res);

    bool Prepare_InitialState(GLESResource res, byte *blob);
    bool Serialise_InitialState(ResourceId resid, GLESResource res);

    ResourceId RegisterResource(GLESResource res)
    {
        ResourceId id = ResourceIDGen::GetNewUniqueID();
        m_CurrentResourceIds[res] = id;
        AddCurrentResource(id, res);
        return id;
    }

private:
    /* implementation of abstract methods */
    bool SerialisableResource(ResourceId id, GLESResourceRecord *record);

    bool ResourceTypeRelease(GLESResource res) { return true; }
    bool Force_InitialState(GLESResource res) { return false; };
    bool Need_InitialStateChunk(GLESResource res);
    bool Prepare_InitialState(GLESResource res);

    void Create_InitialState(ResourceId id, GLESResource live, bool hasData) { };
    void Apply_InitialState(GLESResource live, InitialContentData initial) { };

    /* end */
    WrappedGLES* m_GLES;

    std::map<GLESResource, ResourceId> m_CurrentResourceIds;

};

