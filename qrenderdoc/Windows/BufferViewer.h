/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2019 Baldur Karlsson
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
#include <QMutex>
#include "Code/Interface/QRDInterface.h"
#include "Code/QRDUtils.h"

namespace Ui
{
class BufferViewer;
}

class QItemSelection;
class QMenu;
class RDTableView;
class BufferItemModel;
class CameraWrapper;
class ArcballWrapper;
class FlycamWrapper;
struct BufferData;
struct PopulateBufferData;
struct CalcBoundingBoxData;

struct BufferExport
{
  enum ExportFormat
  {
    CSV,
    RawBytes,
  };

  ExportFormat format;

  BufferExport(ExportFormat f) : format(f) {}
};

struct BBoxData
{
  struct
  {
    QList<FloatVector> Min;
    QList<FloatVector> Max;
  } bounds[3];
};

class BufferViewer : public QFrame, public IBufferViewer, public ICaptureViewer
{
  Q_OBJECT

  Q_PROPERTY(QVariant persistData READ persistData WRITE setPersistData DESIGNABLE false SCRIPTABLE false)

public:
  explicit BufferViewer(ICaptureContext &ctx, bool meshview, QWidget *parent = 0);
  ~BufferViewer();

  // IBufferViewer
  QWidget *Widget() override { return this; }
  void ScrollToRow(int row, MeshDataStage stage = MeshDataStage::VSIn) override
  {
    ScrollToRow(tableForStage(stage), row);
  }
  void ViewBuffer(uint64_t byteOffset, uint64_t byteSize, ResourceId id,
                  const rdcstr &format = "") override;
  void ViewTexture(uint32_t arrayIdx, uint32_t mip, ResourceId id, const rdcstr &format = "") override;

  // ICaptureViewer
  void OnCaptureLoaded() override;
  void OnCaptureClosed() override;
  void OnSelectedEventChanged(uint32_t eventId) override {}
  void OnEventChanged(uint32_t eventId) override;

  QVariant persistData();
  void setPersistData(const QVariant &persistData);

private slots:
  // automatic slots
  void on_outputTabs_currentChanged(int index);
  void on_resetCamera_clicked();
  void on_autofitCamera_clicked();
  void on_toggleControls_toggled(bool checked);
  void on_syncViews_toggled(bool checked);
  void on_resourceDetails_clicked();
  void on_highlightVerts_toggled(bool checked);
  void on_wireframeRender_toggled(bool checked);
  void on_solidShading_currentIndexChanged(int index);
  void on_drawRange_currentIndexChanged(int index);
  void on_controlType_currentIndexChanged(int index);
  void on_camSpeed_valueChanged(double value);
  void on_instance_valueChanged(int value);
  void on_viewIndex_valueChanged(int value);
  void on_rowOffset_valueChanged(int value);
  void on_byteRangeStart_valueChanged(int value);
  void on_byteRangeLength_valueChanged(int value);

  // manual slots
  void render_mouseMove(QMouseEvent *e);
  void render_clicked(QMouseEvent *e);

  void render_mouseWheel(QWheelEvent *e);
  void render_keyPress(QKeyEvent *e);
  void render_keyRelease(QKeyEvent *e);
  void render_timer();

  void data_selected(const QItemSelection &selected, const QItemSelection &deselected);
  void data_scrolled(int scroll);
  void camGuess_changed(double value);

  void processFormat(const QString &format);

  void exportData(const BufferExport &params);
  void debugVertex();

protected:
  void changeEvent(QEvent *event) override;

private:
  Ui::BufferViewer *ui;
  ICaptureContext &m_Ctx;

  IReplayOutput *m_Output;

  void updateWindowTitle();

  void configureDrawRange();

  void RT_UpdateAndDisplay(IReplayController *r);

  MeshDisplay m_Config;

  MeshDataStage m_CurStage;

  // cached data from PostVS data
  MeshFormat m_PostVS, m_PostGS;

  // the configurations for 3D preview
  MeshFormat m_VSInPosition, m_VSInSecondary;
  MeshFormat m_PostVSPosition, m_PostVSSecondary;
  MeshFormat m_PostGSPosition, m_PostGSSecondary;

  QMutex m_BBoxLock;
  QMap<uint32_t, BBoxData> m_BBoxes;

  void populateBBox(PopulateBufferData *data);
  void calcBoundingData(CalcBoundingBoxData &bbox);
  void UI_UpdateBoundingBox(const CalcBoundingBoxData &bbox);
  void UI_UpdateBoundingBoxLabels(int compCount = 0);

  void UI_ResetArcball();

  // data from raw buffer view
  bool m_IsBuffer = true;
  QString m_Format;
  uint32_t m_TexArrayIdx = 0;
  uint32_t m_TexMip = 0;
  uint64_t m_ByteOffset = 0;
  uint64_t m_ObjectByteSize = UINT64_MAX;
  uint64_t m_ByteSize = UINT64_MAX;
  ResourceId m_BufferID;

  CameraWrapper *m_CurrentCamera = NULL;
  ArcballWrapper *m_Arcball = NULL;
  FlycamWrapper *m_Flycam = NULL;

  bool m_MeshView;

  BufferItemModel *m_ModelVSIn;
  BufferItemModel *m_ModelVSOut;
  BufferItemModel *m_ModelGSOut;

  RDTableView *m_CurView = NULL;
  int m_ContextColumn = -1;

  int m_IdxColWidth;
  int m_DataColWidth;
  int m_DataRowHeight;

  QMenu *m_HeaderMenu = NULL;

  QAction *m_ResetColumnSel = NULL;
  QAction *m_SelectPosColumn = NULL;
  QAction *m_SelectSecondColumn = NULL;
  QAction *m_SelectSecondAlphaColumn = NULL;

  QMenu *m_ExportMenu = NULL;

  QAction *m_ExportCSV = NULL;
  QAction *m_ExportBytes = NULL;
  QAction *m_DebugVert = NULL;

  RDTableView *tableForStage(MeshDataStage stage);
  BufferItemModel *modelForStage(MeshDataStage stage);

  RDTableView *currentTable() { return tableForStage(m_CurStage); }
  BufferItemModel *currentBufferModel() { return modelForStage(m_CurStage); }
  bool isCurrentRasterOut();
  int currentStageIndex();

  void SetupMeshView();
  void SetupRawView();

  void stageRowMenu(MeshDataStage stage, QMenu *menu, const QPoint &pos);
  void meshHeaderMenu(MeshDataStage stage, const QPoint &pos);

  void Reset();

  void updateCheckerboardColours();

  void ClearModels();

  void UI_CalculateMeshFormats();

  void UpdateCurrentMeshConfig();
  void EnableCameraGuessControls();

  void CalcColumnWidth(int maxNumRows = 1);
  void ApplyRowAndColumnDims(int numColumns, RDTableView *view);

  void SyncViews(RDTableView *primary, bool selection, bool scroll);
  void UpdateHighlightVerts();
  void ScrollToRow(RDTableView *view, int row);
};
