#include "Core.h"

#include <QTimer>

Core::Core()
{

}

Core::~Core()
{

}


void QInvoke::call(const std::function<void()> &f)
{
    QTimer::singleShot(0, new QInvoke(f), SLOT(doInvoke()));
}
