#pragma once

#include "core/resource_manager.h"


enum NullInitialiser
{
  MakeNullResource
};

struct GLESResource
{
  GLESResource()
  {
  }

  GLESResource(NullInitialiser)
  {
  }


  bool operator==(const GLESResource &o) const
  {
    return false;
  }

  bool operator!=(const GLESResource &o) const { return !(*this == o); }
  bool operator<(const GLESResource &o) const
  {
    return false;
  }
};

class GLESResourceRecord : public ResourceRecord
{
public:
    static const NullInitialiser NullResource = MakeNullResource;


    GLESResourceRecord(ResourceId id)
        : ResourceRecord(id, true)
    {
    }
};
