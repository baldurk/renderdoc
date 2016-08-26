#pragma once

#include "core/resource_manager.h"

struct GLESResource
{
  GLESResource()
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
    GLESResourceRecord(ResourceId id)
        : ResourceRecord(id, true)
    {
    }
};
