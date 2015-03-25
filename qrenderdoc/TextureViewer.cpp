#include "TextureViewer.h"
#include "ui_TextureViewer.h"

#include "renderdoc_replay.h"

ReplayOutput *out = NULL;
TextureDisplay d;

TextureViewer::TextureViewer(QWidget *parent) :
  QFrame(parent),
  ui(new Ui::TextureViewer)
{
  ui->setupUi(this);

  ReplayRenderer *renderer = NULL;

  float progress = 0.0f;
  RENDERDOC_CreateReplayRenderer("T:\\renderdoc\\archive_renderdoc_captures\\deferred_plusplus.rdc", &progress, &renderer);

  rdctype::array<FetchTexture> texs;
  ReplayRenderer_GetTextures(renderer, &texs);

  for(int32_t i=0; i < texs.count; i++)
  {
    if(texs[i].creationFlags & eTextureCreate_SwapBuffer)
    {
      d.texid = texs[i].ID;
      d.mip = 0;
      d.sampleIdx = ~0U;
      d.overlay = eTexOverlay_None;
      d.CustomShader = ResourceId();
      d.HDRMul = -1.0f;
      d.linearDisplayAsGamma = true;
      d.FlipY = false;
      d.rangemin = 0.0f;
      d.rangemax = 1.0f;
      d.scale = 1.0f;
      d.offx = 0.0f;
      d.offy = 0.0f;
      d.sliceFace = 0;
      d.rawoutput = false;
      d.lightBackgroundColour = d.darkBackgroundColour =
        FloatVector(0.0f, 0.0f, 0.0f, 0.0f);
      d.Red = d.Green = d.Blue = true;
      d.Alpha = false;
      break;
    }
  }

  HWND wnd = (HWND)ui->framerender->winId();

  out = ReplayRenderer_CreateOutput(renderer, wnd);

  OutputConfig c = { eOutputType_TexDisplay };

  ReplayOutput_SetOutputConfig(out, c);
  ReplayOutput_SetTextureDisplay(out, d);

  ReplayRenderer_SetFrameEvent(renderer, 0, 10000000+rand()%1000);
}

TextureViewer::~TextureViewer()
{
  delete ui;
}
