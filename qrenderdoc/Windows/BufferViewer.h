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
#include "Code/CaptureContext.h"

namespace Ui
{
class BufferViewer;
}

class RDTableView;
class BufferItemModel;
class CameraWrapper;
class ArcballWrapper;
class FlycamWrapper;
struct BufferData;

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

class BufferViewer : public QFrame, public ILogViewerForm
{
  Q_OBJECT

public:
  explicit BufferViewer(CaptureContext &ctx, bool meshview, QWidget *parent = 0);
  ~BufferViewer();

  void ViewBuffer(uint64_t byteOffset, uint64_t byteSize, ResourceId id, const QString &format = "");
  void ViewTexture(uint32_t arrayIdx, uint32_t mip, ResourceId id, const QString &format = "");

  void OnLogfileLoaded();
  void OnLogfileClosed();
  void OnSelectedEventChanged(uint32_t eventID) {}
  void OnEventChanged(uint32_t eventID);

  void ScrollToRow(int row, MeshDataStage stage = eMeshDataStage_VSIn)
  {
    if(stage == eMeshDataStage_VSOut)
      ScrollToRow(m_ModelVSOut, row);
    else if(stage == eMeshDataStage_GSOut)
      ScrollToRow(m_ModelGSOut, row);
    else
      ScrollToRow(m_ModelVSIn, row);
  }

private slots:
  // automatic slots
  void on_outputTabs_currentChanged(int index);
  void on_resetCamera_clicked();
  void on_autofitCamera_clicked();
  void on_toggleControls_toggled(bool checked);
  void on_syncViews_toggled(bool checked);
  void on_highlightVerts_toggled(bool checked);
  void on_wireframeRender_toggled(bool checked);
  void on_solidShading_currentIndexChanged(int index);
  void on_drawRange_currentIndexChanged(int index);
  void on_controlType_currentIndexChanged(int index);
  void on_camSpeed_valueChanged(double value);
  void on_instance_valueChanged(int arg1);
  void on_rowOffset_valueChanged(int arg1);

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

private:
  Ui::BufferViewer *ui;
  CaptureContext &m_Ctx;

  IReplayOutput *m_Output;

  void RT_UpdateAndDisplay(IReplayRenderer *);
  void RT_FetchMeshData(IReplayRenderer *r);

  MeshDisplay m_Config;

  MeshDataStage m_CurStage;

  // data from mesh output
  MeshFormat m_PostVS;
  MeshFormat m_PostGS;

  // the configurations for 3D preview
  MeshFormat m_VSInPosition, m_VSInSecondary;
  MeshFormat m_PostVSPosition, m_PostVSSecondary;
  MeshFormat m_PostGSPosition, m_PostGSSecondary;

  struct BBoxData
  {
    struct
    {
      QList<FloatVector> Min;
      QList<FloatVector> Max;
    } bounds[3];
  };

  struct CalcBoundingBoxData
  {
    uint32_t eventID;
    uint32_t inst;
    int32_t baseVertex;

    struct StageData
    {
      QList<FormatElement> elements;
      uint32_t count;
      BufferData *indices = NULL;
      QList<BufferData *> buffers;
    } input[3];

    BBoxData output;
  };

  QMutex m_BBoxLock;
  QMap<uint32_t, BBoxData> m_BBoxes;

  void calcBoundingData(CalcBoundingBoxData &bbox);
  void updateBoundingBox(const CalcBoundingBoxData &bbox);

  void resetArcball();

  // data from raw buffer view
  bool m_IsBuffer = true;
  uint32_t m_TexArrayIdx = 0;
  uint32_t m_TexMip = 0;
  uint64_t m_ByteOffset = 0;
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

  RDTableView *currentTable();
  BufferItemModel *currentBufferModel();
  bool isCurrentRasterOut();

  void SetupMeshView();
  void SetupRawView();

  void Reset();
  void ClearModels();

  void guessPositionColumn(BufferItemModel *model);
  void guessSecondaryColumn(BufferItemModel *model);
  void updatePreviewColumns();
  void configureMeshColumns();

  void UpdateMeshConfig();
  void EnableCameraGuessControls();

  void CalcColumnWidth();
  void ApplyColumnWidths(int numColumns, RDTableView *view);

  void SyncViews(RDTableView *primary, bool selection, bool scroll);
  void UpdateHighlightVerts();
  void ScrollToRow(BufferItemModel *model, int row);
};