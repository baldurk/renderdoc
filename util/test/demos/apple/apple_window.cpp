/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022-2024 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "apple_window.h"

#include "official/metal-cpp.h"

static NS::Application *pSharedApplication = NULL;
MyAppDelegate *AppleWindow::pAppDelegate = NULL;

class MyAppDelegate : public NS::ApplicationDelegate
{
public:
  ~MyAppDelegate();

  NS::Menu *createMenuBar();

  virtual void applicationWillFinishLaunching(NS::Notification *pNotification) override;
  virtual void applicationDidFinishLaunching(NS::Notification *pNotification) override;
  virtual bool applicationShouldTerminateAfterLastWindowClosed(NS::Application *pSender) override;

  void CreateWindow(int width, int height, const char *title);
  NS::View *GetContentView();
  NS::Window *GetWindow();

private:
  NS::Window *_pWindow;
};

MyAppDelegate::~MyAppDelegate()
{
  _pWindow->release();
}

NS::Menu *MyAppDelegate::createMenuBar()
{
  using NS::StringEncoding::UTF8StringEncoding;

  NS::Menu *pMainMenu = NS::Menu::alloc()->init();
  NS::MenuItem *pAppMenuItem = NS::MenuItem::alloc()->init();
  NS::Menu *pAppMenu = NS::Menu::alloc()->init(NS::String::string("Appname", UTF8StringEncoding));

  NS::String *appName = NS::RunningApplication::currentApplication()->localizedName();
  NS::String *quitItemName =
      NS::String::string("Quit ", UTF8StringEncoding)->stringByAppendingString(appName);
  SEL quitCb =
      NS::MenuItem::registerActionCallback("appQuit", [](void *, SEL, const NS::Object *pSender) {
        NS::Application *pApp = NS::Application::sharedApplication();
        pApp->terminate(pSender);
      });

  NS::MenuItem *pAppQuitItem =
      pAppMenu->addItem(quitItemName, quitCb, NS::String::string("q", UTF8StringEncoding));
  pAppQuitItem->setKeyEquivalentModifierMask(NS::EventModifierFlagCommand);
  pAppMenuItem->setSubmenu(pAppMenu);

  NS::MenuItem *pWindowMenuItem = NS::MenuItem::alloc()->init();
  NS::Menu *pWindowMenu = NS::Menu::alloc()->init(NS::String::string("Window", UTF8StringEncoding));

  SEL closeWindowCb =
      NS::MenuItem::registerActionCallback("windowClose", [](void *, SEL, const NS::Object *) {
        NS::Application *pApp = NS::Application::sharedApplication();
        pApp->windows()->object<NS::Window>(0)->close();
      });
  NS::MenuItem *pCloseWindowItem =
      pWindowMenu->addItem(NS::String::string("Close Window", UTF8StringEncoding), closeWindowCb,
                           NS::String::string("w", UTF8StringEncoding));
  pCloseWindowItem->setKeyEquivalentModifierMask(NS::EventModifierFlagCommand);

  pWindowMenuItem->setSubmenu(pWindowMenu);

  pMainMenu->addItem(pAppMenuItem);
  pMainMenu->addItem(pWindowMenuItem);

  pAppMenuItem->release();
  pWindowMenuItem->release();
  pAppMenu->release();
  pWindowMenu->release();

  return pMainMenu->autorelease();
}

void MyAppDelegate::applicationWillFinishLaunching(NS::Notification *pNotification)
{
  NS::Menu *pMenu = createMenuBar();
  NS::Application *pApp = (NS::Application *)pNotification->object();
  pApp->setMainMenu(pMenu);
  pApp->setActivationPolicy(NS::ActivationPolicy::ActivationPolicyRegular);
}

void MyAppDelegate::applicationDidFinishLaunching(NS::Notification *pNotification)
{
  NS::Application *pApp = (NS::Application *)pNotification->object();
  pApp->activateIgnoringOtherApps(true);
}

bool MyAppDelegate::applicationShouldTerminateAfterLastWindowClosed(NS::Application *pSender)
{
  return true;
}

void MyAppDelegate::CreateWindow(int width, int height, const char *title)
{
  CGRect frame = CGRectMake(100.0f, 100.0f, width, height);

  _pWindow = NS::Window::alloc()->init(frame, NS::WindowStyleMaskClosable | NS::WindowStyleMaskTitled,
                                       NS::BackingStoreBuffered, false);

  NS::View *view = _pWindow->contentView();
  view->setWantsLayer(true);
  view->setLayer((CA::Layer *)CA::MetalLayer::layer());

  _pWindow->setTitle(NS::String::string(title, NS::StringEncoding::UTF8StringEncoding));
  _pWindow->makeKeyAndOrderFront(NULL);
}

NS::View *MyAppDelegate::GetContentView()
{
  return _pWindow->contentView();
}

NS::Window *MyAppDelegate::GetWindow()
{
  return _pWindow;
}

AppleWindow::~AppleWindow()
{
  pAppDelegate = NULL;
}

AppleWindow::AppleWindow(int width, int height, const char *title) : GraphicsWindow(title)
{
  pAppDelegate->CreateWindow(width, height, title);
  view = pAppDelegate->GetContentView();
}

bool AppleWindow::Init()
{
  pSharedApplication = NS::Application::sharedApplication();
  pAppDelegate = new MyAppDelegate();
  pSharedApplication->setDelegate(pAppDelegate);

  if(!NS::RunningApplication::currentApplication()->finishedLaunching())
    pSharedApplication->finishLaunching();

  return true;
}

void AppleWindow::Resize(int width, int height)
{
  TEST_ERROR("Resize is not implemented");
}

extern "C" void *const NSDefaultRunLoopMode;

bool AppleWindow::Update()
{
  NS::AutoreleasePool *pAutoreleasePool = NS::AutoreleasePool::alloc()->init();
  while(true)
  {
    NS::Event *event = pSharedApplication->nextEventMatchingMask(
        (int)NS::EventMaskAny, NS::Date::distantPast(), (NS::String *)NSDefaultRunLoopMode, true);
    if(event == NULL)
      break;
    pSharedApplication->sendEvent(event);
  }
  pAutoreleasePool->release();

  return pAppDelegate->GetWindow()->visible();
}
