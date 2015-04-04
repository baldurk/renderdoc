#ifndef CORE_H
#define CORE_H

#include "RenderManager.h"

class Core
{
  public:
    Core();
    ~Core();

    const RenderManager *Renderer() { return &m_Renderer; }

  private:
    RenderManager m_Renderer;
};

#endif // CORE_H
