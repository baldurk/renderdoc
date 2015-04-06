#include "TextureViewer.h"
#include "ui_TextureViewer.h"

#include "Code/Core.h"

#if defined(__linux__)
#include <QX11Info>
#include <X11/Xlib.h>
#include <GL/glx.h>
#endif

TextureViewer::TextureViewer(Core *core, QWidget *parent) :
  QFrame(parent),
  ui(new Ui::TextureViewer),
  m_Core(core)
{
  ui->setupUi(this);

  m_Core->AddLogViewer(this);

  ui->framerender->SetOutput(NULL);
  m_Output = NULL;
}

TextureViewer::~TextureViewer()
{
  m_Core->RemoveLogViewer(this);
  delete ui;
}

void TextureViewer::OnLogfileLoaded()
{
#if defined(WIN32)
  HWND wnd = (HWND)ui->framerender->winId();
#elif defined(__linux__)
  Display *display = QX11Info::display();
  GLXDrawable drawable = (GLXDrawable)ui->framerender->winId();

  void *displayAndDrawable[2] = { (void *)display, (void *)drawable };
  void *wnd = displayAndDrawable;
#else
  #error "Unknown platform"
#endif

  m_Core->Renderer()->BlockInvoke([wnd,this](IReplayRenderer *r) {
    m_Output = r->CreateOutput(wnd);
    ui->framerender->SetOutput(m_Output);

    OutputConfig c = { eOutputType_TexDisplay };
    m_Output->SetOutputConfig(c);
  });
}

void TextureViewer::OnLogfileClosed()
{
  m_Output = NULL;
  ui->framerender->SetOutput(NULL);
}

void TextureViewer::OnEventSelected(uint32_t frameID, uint32_t eventID)
{
  m_Core->Renderer()->AsyncInvoke([this](IReplayRenderer *) {
    TextureDisplay d;
    if(m_Core->APIProps().pipelineType == ePipelineState_D3D11)
      d.texid = m_Core->CurD3D11PipelineState.m_OM.RenderTargets[0].Resource;
    else
      d.texid = m_Core->CurGLPipelineState.m_FB.m_DrawFBO.Color[0];
    d.mip = 0;
    d.sampleIdx = ~0U;
    d.overlay = eTexOverlay_None;
    d.CustomShader = ResourceId();
    d.HDRMul = -1.0f;
    d.linearDisplayAsGamma = true;
    d.FlipY = false;
    d.rangemin = 0.0f;
    d.rangemax = 1.0f;
    d.scale = -1.0f;
    d.offx = 0.0f;
    d.offy = 0.0f;
    d.sliceFace = 0;
    d.rawoutput = false;
    d.lightBackgroundColour = d.darkBackgroundColour =
        FloatVector(0.0f, 0.0f, 0.0f, 0.0f);
    d.Red = d.Green = d.Blue = true;
    d.Alpha = false;
    m_Output->SetTextureDisplay(d);

    GUIInvoke::call([this]() { ui->framerender->update(); });
  });
}
