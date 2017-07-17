/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Baldur Karlsson
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

#pragma once

#include <QFrame>
#include <QMenu>
#include <QMouseEvent>
#include "Code/CaptureContext.h"

namespace Ui
{
class TextureViewer;
}

class ResourcePreview;
class ThumbnailStrip;
class TextureGoto;
class QFileSystemWatcher;

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
  ShaderStage Stage;
  int index;
  int arrayEl;

  static const Following Default;

  Following();

  Following(FollowType t, ShaderStage s, int i, int a);

  bool operator==(const Following &o);
  bool operator!=(const Following &o);
  static void GetDrawContext(ICaptureContext &ctx, bool &copy, bool &clear, bool &compute);

  int GetHighestMip(ICaptureContext &ctx);
  int GetFirstArraySlice(ICaptureContext &ctx);
  CompType GetTypeHint(ICaptureContext &ctx);

  ResourceId GetResourceId(ICaptureContext &ctx);
  BoundResource GetBoundResource(ICaptureContext &ctx, int arrayIdx);

  static QVector<BoundResource> GetOutputTargets(ICaptureContext &ctx);

  static BoundResource GetDepthTarget(ICaptureContext &ctx);

  QMap<BindpointMap, QVector<BoundResource>> GetReadWriteResources(ICaptureContext &ctx);

  static QMap<BindpointMap, QVector<BoundResource>> GetReadWriteResources(ICaptureContext &ctx,
                                                                          ShaderStage stage);

  QMap<BindpointMap, QVector<BoundResource>> GetReadOnlyResources(ICaptureContext &ctx);

  static QMap<BindpointMap, QVector<BoundResource>> GetReadOnlyResources(ICaptureContext &ctx,
                                                                         ShaderStage stage);

  const ShaderReflection *GetReflection(ICaptureContext &ctx);
  static const ShaderReflection *GetReflection(ICaptureContext &ctx, ShaderStage stage);

  const ShaderBindpointMapping &GetMapping(ICaptureContext &ctx);
  static const ShaderBindpointMapping &GetMapping(ICaptureContext &ctx, ShaderStage stage);
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
    typeHint = CompType::Typeless;
  }

  int displayType;    // RGBA, RGBM, Custom
  QString customShader;
  bool r, g, b, a;
  bool depth, stencil;
  int mip, slice;
  float minrange, maxrange;
  CompType typeHint;
};

class TextureViewer : public QFrame, public ITextureViewer, public ILogViewer
{
private:
  Q_OBJECT

  Q_PROPERTY(QVariant persistData READ persistData WRITE setPersistData DESIGNABLE false SCRIPTABLE false)

public:
  explicit TextureViewer(ICaptureContext &ctx, QWidget *parent = 0);
  ~TextureViewer();

  // ITextureViewer
  QWidget *Widget() override { return this; }
  void ViewTexture(ResourceId ID, bool focus) override;
  void GotoLocation(int x, int y) override;

  // ILogViewerForm
  void OnLogfileLoaded() override;
  void OnLogfileClosed() override;
  void OnSelectedEventChanged(uint32_t eventID) override {}
  void OnEventChanged(uint32_t eventID) override;

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

  void on_locationGoto_clicked();
  void on_viewTexBuffer_clicked();
  void on_texListShow_clicked();
  void on_saveTex_clicked();
  void on_debugPixelContext_clicked();
  void on_pixelHistory_clicked();

  void on_customCreate_clicked();
  void on_customEdit_clicked();
  void on_customDelete_clicked();

  void on_cancelTextureListFilter_clicked();
  void on_textureListFilter_editTextChanged(const QString &text);
  void on_textureListFilter_currentIndexChanged(int index);
  void on_textureList_clicked(const QModelIndex &index);

  // manual slots
  void render_mouseClick(QMouseEvent *e);
  void render_mouseMove(QMouseEvent *e);
  void render_mouseWheel(QWheelEvent *e);
  void render_resize(QResizeEvent *e);
  void render_keyPress(QKeyEvent *e);

  void textureTab_Changed(int index);
  void textureTab_Closing(int index);

  void thumb_clicked(QMouseEvent *);
  void thumb_doubleClicked(QMouseEvent *);
  void texContextItem_triggered();
  void showDisabled_triggered();
  void showEmpty_triggered();

  void zoomOption_returnPressed();

  void range_rangeUpdated();
  void rangePoint_textChanged(QString text);
  void rangePoint_leave();
  void rangePoint_keyPress(QKeyEvent *e);

  void customShaderModified(const QString &path);

  void channelsWidget_mouseClicked(QMouseEvent *event);
  void channelsWidget_toggled(bool checked) { UI_UpdateChannels(); }
  void channelsWidget_selected(int index) { UI_UpdateChannels(); }
private:
  void RT_FetchCurrentPixel(uint32_t x, uint32_t y, PixelValue &pickValue, PixelValue &realValue);
  void RT_PickPixelsAndUpdate(IReplayController *);
  void RT_PickHoverAndUpdate(IReplayController *);
  void RT_UpdateAndDisplay(IReplayController *);
  void RT_UpdateVisualRange(IReplayController *);

  void UI_RecreatePanels();

  void UI_UpdateStatusText();
  void UI_UpdateTextureDetails();
  void UI_OnTextureSelectionChanged(bool newdraw);

  void UI_SetHistogramRange(const TextureDescription *tex, CompType typeHint);

  void UI_UpdateChannels();

  void SetupTextureTabs();

  void Reset();

  ResourcePreview *UI_CreateThumbnail(ThumbnailStrip *strip);
  void UI_CreateThumbnails();
  void InitResourcePreview(ResourcePreview *prev, ResourceId id, CompType typeHint, bool force,
                           Following &follow, const QString &bindName, const QString &slotName);

  void InitStageResourcePreviews(ShaderStage stage,
                                 const rdctype::array<ShaderResource> &resourceDetails,
                                 const rdctype::array<BindpointMap> &mapping,
                                 QMap<BindpointMap, QVector<BoundResource>> &ResList,
                                 ThumbnailStrip *prevs, int &prevIndex, bool copy, bool rw);

  void AddResourceUsageEntry(QMenu &menu, uint32_t start, uint32_t end, ResourceUsage usage);
  void OpenResourceContextMenu(ResourceId id, const rdctype::array<EventUsage> &usage);

  void AutoFitRange();
  void rangePoint_Update();

  bool currentTextureIsLocked() { return m_LockedId != ResourceId(); }
  void setFitToWindow(bool checked);

  void setCurrentZoomValue(float zoom);
  float getCurrentZoomValue();

  bool ScrollUpdateScrollbars = true;

  float CurMaxScrollX();
  float CurMaxScrollY();

  float GetFitScale();

  int realRenderWidth() const;
  int realRenderHeight() const;

  QPoint getScrollPosition();
  void setScrollPosition(const QPoint &pos);

  TextureDescription *GetCurrentTexture();
  void UI_UpdateCachedTexture();

  void ShowGotoPopup();

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

  bool m_ShowEmpty = false;
  bool m_ShowDisabled = false;

  bool m_Visualise = false;
  bool m_NoRangePaint = false;
  bool m_RangePoint_Dirty = false;

  ResourceId m_LockedId;
  QMap<ResourceId, QWidget *> m_LockedTabs;

  TextureGoto *m_Goto;

  Ui::TextureViewer *ui;
  ICaptureContext &m_Ctx;
  IReplayOutput *m_Output = NULL;

  TextureDescription *m_CachedTexture;
  Following m_Following = Following::Default;
  QMap<ResourceId, TexSettings> m_TextureSettings;

  QFileSystemWatcher *m_Watcher = NULL;
  QStringList m_CustomShadersBusy;
  QMap<QString, ResourceId> m_CustomShaders;
  QMap<QString, IShaderViewer *> m_CustomShaderEditor;

  void reloadCustomShaders(const QString &filter);

  TextureDisplay m_TexDisplay;
};
