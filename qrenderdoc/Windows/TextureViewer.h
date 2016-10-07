/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Baldur Karlsson
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

#ifndef TEXTUREVIEWER_H
#define TEXTUREVIEWER_H

#include <QFrame>
#include <QMouseEvent>
#include "Code/CaptureContext.h"

namespace Ui
{
class TextureViewer;
}

class ResourcePreview;
class ThumbnailStrip;

enum struct FollowType
{
  OutputColour,
  OutputDepth,
  ReadWrite,
  ReadOnly
};

struct Following
{
  FollowType Type;
  ShaderStageType Stage;
  int index;
  int arrayEl;

  static const Following Default;

  Following();

  Following(FollowType t, ShaderStageType s, int i, int a);

  bool operator==(const Following &o);
  bool operator!=(const Following &o);
  static void GetDrawContext(CaptureContext *ctx, bool &copy, bool &compute);

  int GetHighestMip(CaptureContext *ctx);
  int GetFirstArraySlice(CaptureContext *ctx);
  FormatComponentType GetTypeHint(CaptureContext *ctx);

  ResourceId GetResourceId(CaptureContext *ctx);
  BoundResource GetBoundResource(CaptureContext *ctx, int arrayIdx);

  static QVector<BoundResource> GetOutputTargets(CaptureContext *ctx);

  static BoundResource GetDepthTarget(CaptureContext *ctx);

  QMap<BindpointMap, QVector<BoundResource>> GetReadWriteResources(CaptureContext *ctx);

  static QMap<BindpointMap, QVector<BoundResource>> GetReadWriteResources(CaptureContext *ctx,
                                                                          ShaderStageType stage);

  QMap<BindpointMap, QVector<BoundResource>> GetReadOnlyResources(CaptureContext *ctx);

  static QMap<BindpointMap, QVector<BoundResource>> GetReadOnlyResources(CaptureContext *ctx,
                                                                         ShaderStageType stage);

  ShaderReflection *GetReflection(CaptureContext *ctx);
  static ShaderReflection *GetReflection(CaptureContext *ctx, ShaderStageType stage);

  ShaderBindpointMapping GetMapping(CaptureContext *ctx);
  static ShaderBindpointMapping GetMapping(CaptureContext *ctx, ShaderStageType stage);
};

struct TexSettings
{
  TexSettings()
  {
    r = g = b = true;
    a = false;
    mip = 0;
    slice = 0;
    minrange = 0.0f;
    maxrange = 1.0f;
    typeHint = eCompType_None;
  }

  int displayType;    // RGBA, RGBM, Custom
  QString customShader;
  bool r, g, b, a;
  bool depth, stencil;
  int mip, slice;
  float minrange, maxrange;
  FormatComponentType typeHint;
};

class TextureViewer : public QFrame, public ILogViewerForm
{
private:
  Q_OBJECT

  Q_PROPERTY(QVariant persistData READ persistData WRITE setPersistData DESIGNABLE false SCRIPTABLE false)

public:
  explicit TextureViewer(CaptureContext *ctx, QWidget *parent = 0);
  ~TextureViewer();

  void OnLogfileLoaded();
  void OnLogfileClosed();
  void OnEventSelected(uint32_t eventID);

  QVariant persistData();
  void setPersistData(const QVariant &persistData);

private slots:
  // automatic slots
  void on_renderHScroll_valueChanged(int position);
  void on_renderVScroll_valueChanged(int position);

  void on_fitToWindow_toggled(bool checked);
  void on_zoomExactSize_clicked();
  void on_zoomOption_currentIndexChanged(int index);

  void on_mipLevel_currentIndexChanged(int index);
  void on_sliceFace_currentIndexChanged(int index);

  void on_overlay_currentIndexChanged(int index);

  void on_zoomRange_clicked();
  void on_autoFit_clicked();
  void on_reset01_clicked();
  void on_visualiseRange_clicked();
  void on_backcolorPick_clicked();
  void on_checkerBack_clicked();

  // manual slots
  void render_mouseClick(QMouseEvent *e);
  void render_mouseMove(QMouseEvent *e);
  void render_mouseWheel(QWheelEvent *e);
  void render_resize(QResizeEvent *e);
  void render_keyPress(QKeyEvent *e);

  void thumb_clicked(QMouseEvent *);

  void zoomOption_returnPressed();

  void channelsWidget_toggled(bool checked) { UI_UpdateChannels(); }
  void channelsWidget_selected(int index) { UI_UpdateChannels(); }
private:
  void RT_FetchCurrentPixel(uint32_t x, uint32_t y, PixelValue &pickValue, PixelValue &realValue);
  void RT_PickPixelsAndUpdate(IReplayRenderer *);
  void RT_PickHoverAndUpdate(IReplayRenderer *);
  void RT_UpdateAndDisplay(IReplayRenderer *);

  void UI_RecreatePanels();

  void UI_UpdateStatusText();
  void UI_UpdateTextureDetails();
  void UI_OnTextureSelectionChanged(bool newdraw);

  void UI_UpdateChannels();

  ResourcePreview *UI_CreateThumbnail(ThumbnailStrip *strip);
  void UI_CreateThumbnails();
  void InitResourcePreview(ResourcePreview *prev, ResourceId id, FormatComponentType typeHint,
                           bool force, Following &follow, const QString &bindName,
                           const QString &slotName);

  void InitStageResourcePreviews(ShaderStageType stage,
                                 const rdctype::array<ShaderResource> &resourceDetails,
                                 const rdctype::array<BindpointMap> &mapping,
                                 QMap<BindpointMap, QVector<BoundResource>> &ResList,
                                 ThumbnailStrip *prevs, int &prevIndex, bool copy, bool rw);

  bool currentTextureIsLocked() { return false; }
  void setFitToWindow(bool checked);

  void setCurrentZoomValue(float zoom);
  float getCurrentZoomValue();

  bool ScrollUpdateScrollbars = true;

  float CurMaxScrollX();
  float CurMaxScrollY();

  float GetFitScale();

  QPoint getScrollPosition();
  void setScrollPosition(const QPoint &pos);

  void UI_UpdateFittedScale();
  void UI_SetScale(float s);
  void UI_SetScale(float s, int x, int y);
  void UI_CalcScrollbars();

  QPoint m_DragStartScroll;
  QPoint m_DragStartPos;

  QPoint m_CurHoverPixel;
  QPoint m_PickedPoint;

  QSizeF m_PrevSize;

  PixelValue m_CurRealValue;
  PixelValue m_CurPixelValue;
  PixelValue m_CurHoverValue;

  QColor darkBack;
  QColor lightBack;

  int m_HighWaterStatusLength = 0;
  int m_PrevFirstArraySlice = -1;
  int m_PrevHighestMip = -1;

  Ui::TextureViewer *ui;
  CaptureContext *m_Ctx = NULL;
  IReplayOutput *m_Output = NULL;

  Following m_Following = Following::Default;
  QMap<ResourceId, TexSettings> m_TextureSettings;

  TextureDisplay m_TexDisplay;
};

#endif    // TEXTUREVIEWER_H
