#ifndef CORE_H
#define CORE_H

#include "RenderManager.h"

class Core
{
  public:
    Core();
    ~Core();

    RenderManager *Renderer() { return &m_Renderer; }

  private:
    RenderManager m_Renderer;
};

// Utility class for invoking a lambda on the GUI thread.
// This is supported by QTimer::singleShot on Qt 5.4 but it's probably
// wise not to require a higher version that necessary.
#include <functional>

class QInvoke : public QObject
{
    Q_OBJECT
    QInvoke(const std::function<void()> &f) : func(f) {}
    std::function<void()> func;
public:
    static void call(const std::function<void()> &f);

protected slots:
    void doInvoke()
    {
        func();
        deleteLater();
    }
};


#endif // CORE_H
