/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

class RDSpinBox64;
class QItemSelection;
class QMenu;
class QPushButton;
class QVBoxLayout;
class RDLabel;
class RDTableView;
class RDSplitter;
class BufferItemModel;
class CollapseGroupBox;
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

struct CBufferData
{
  bool valid = false;
  bool bytesBacked = false;
  bool compileConstants = false;
  ResourceId pipe;
  ResourceId shader;
  rdcstr entryPoint;
};

class BufferViewer : public QFrame, public IBufferViewer, public ICaptureViewer
{
  Q_OBJECT

  Q_PROPERTY(QVariant persistData READ persistData WRITE setPersistData DESIGNABLE false SCRIPTABLE false)

public:
  explicit BufferViewer(ICaptureContext &ctx, bool meshview, QWidget *parent = 0);
  ~BufferViewer();

  static BufferViewer *HasCBufferView(ShaderStage stage, uint32_t slot, uint32_t idx);
  static BufferViewer *GetFirstCBufferView(BufferViewer *exclude);
  bool IsCBufferView() const { return m_CBufferSlot.stage != ShaderStage::Count; }
  void ViewBuffer(uint64_t byteOffset, uint64_t byteSize, ResourceId id, const rdcstr &format = "");
  void ViewCBuffer(const ShaderStage stage, uint32_t slot, uint32_t idx);
  void ViewTexture(ResourceId id, const Subresource &sub, const rdcstr &format = "");

  // IBufferViewer
  QWidget *Widget() override { return this; }
  void ScrollToRow(int32_t row, MeshDataStage stage = MeshDataStage::VSIn) override;
  void ScrollToColumn(int32_t column, MeshDataStage stage = MeshDataStage::VSIn) override;
  void ShowMeshData(MeshDataStage stage) override;
  void SetCurrentInstance(int32_t instance) override;
  void SetCurrentView(int32_t view) override;
  void SetPreviewStage(MeshDataStage stage) override;

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
  void on_showPadding_toggled(bool checked);
  void on_resourceDetails_clicked();
  void on_highlightVerts_toggled(bool checked);
  void on_vtxExploderSlider_valueChanged(int value);
  void on_exploderReset_clicked();
  void on_exploderScale_valueChanged(double value);
  void on_wireframeRender_toggled(bool checked);
  void on_visualisation_currentIndexChanged(int index);
  void on_drawRange_currentIndexChanged(int index);
  void on_controlType_currentIndexChanged(int index);
  void on_camSpeed_valueChanged(double value);
  void on_instance_valueChanged(int value);
  void on_viewIndex_valueChanged(int value);
  void on_rowOffset_valueChanged(int value);
  void on_byteRangeStart_valueChanged(double value);
  void on_byteRangeLength_valueChanged(double value);
  void on_axisMappingCombo_currentIndexChanged(int index);
  void on_axisMappingButton_clicked();
  void on_setFormat_toggled(bool checked);
  void on_resetMeshFilterButton_clicked();

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

  void updateExportActionNames();
  void exportData(const BufferExport &params);
  void debugVertex();
  void fixedVars_contextMenu(const QPoint &pos);

private:
  bool eventFilter(QObject *watched, QEvent *event) override;
  Ui::BufferViewer *ui;
  ICaptureContext &m_Ctx;

  IReplayOutput *m_Output;

  void updateLabelsAndLayout();

  void configureDrawRange();

  void RT_UpdateAndDisplay(IReplayController *r);

  QPushButton *MakePreviousPageButton();
  QPushButton *MakeNextPageButton();

  MeshDisplay m_Config;

  MeshDataStage m_CurStage;

  // cached data from PostVS data
  MeshFormat m_Out1Data, m_Out2Data;

  // the configurations for 3D preview
  MeshFormat m_InPosition, m_InSecondary;
  MeshFormat m_Out1Position, m_Out1Secondary;
  MeshFormat m_Out2Position, m_Out2Secondary;

  QMutex m_BBoxLock;
  QMap<uint32_t, BBoxData> m_BBoxes;

  void populateBBox(PopulateBufferData *data);
  void calcBoundingData(CalcBoundingBoxData &bbox);
  void UI_UpdateBoundingBox(const CalcBoundingBoxData &bbox);
  void UI_UpdateBoundingBoxLabels(int compCount = 0);

  void UI_AddTaskPayloads(RDTreeWidgetItem *root, size_t baseOffset,
                          const rdcarray<ShaderConstant> &consts, BufferData *buffer);
  void UI_AddFixedVariables(RDTreeWidgetItem *root, uint32_t baseOffset,
                            const rdcarray<ShaderConstant> &consts,
                            const rdcarray<ShaderVariable> &vars);
  void UI_RemoveOffsets(RDTreeWidgetItem *root);
  void UI_FixedAddMatrixRows(RDTreeWidgetItem *n, const ShaderConstant &c, const ShaderVariable &v);

  void exportCSV(QTextStream &ts, const QString &prefix, RDTreeWidgetItem *item);

  void FillScrolls(PopulateBufferData *bufdata);

  void GetIndicesForMeshRow(uint32_t row, uint32_t &taskIndex, uint32_t &meshletIndex);

  void UI_ResetArcball();

  // data from raw buffer view
  bool m_IsBuffer = true;
  QString m_Format;
  Subresource m_TexSub = {0, 0, 0};
  uint64_t m_ByteOffset = 0;
  uint64_t m_PagingByteOffset = 0;
  uint64_t m_ObjectByteSize = UINT64_MAX;
  uint64_t m_ByteSize = UINT64_MAX;
  ResourceId m_BufferID;

  struct CBufferSlot
  {
    ShaderStage stage;
    uint32_t slot;
    uint32_t arrayIdx;

    bool operator==(const CBufferSlot &c) const
    {
      return stage == c.stage && slot == c.slot && arrayIdx == c.arrayIdx;
    }
  } m_CBufferSlot = {ShaderStage::Count, 0, 0};

  CBufferData m_CurCBuffer;

  static QList<BufferViewer *> m_CBufferViews;

  CameraWrapper *m_CurrentCamera = NULL;
  ArcballWrapper *m_Arcball = NULL;
  FlycamWrapper *m_Flycam = NULL;

  bool m_MeshView;

  // for ease of reading, these stages are named as in, out1, and out2. Note however that this does
  // NOT correspond to which table widgets these fill out. Since it is most common to look at VS In
  // and VS Out for classic vertex pipeline draws, and Task Out and Mesh Out for mesh shader draws
  // (and because mesh shader input visualisation is not natively available) we pair up VS In and
  // Task Out on the same control, VS Out and Mesh Out on the same control, and GS/Tess Out and Mesh
  // In on the same control.

  // the input stage
  BufferItemModel *m_ModelIn;
  // the first output stage (vertex, or task)
  BufferItemModel *m_ModelOut1;
  // the second output stage (geometry, or mesh)
  BufferItemModel *m_ModelOut2;

  // the container widgets for each stage which are remapped depending on the draw type to one of
  // the above. This may mean 1:1 for traditional draws or [0] being out1 (task out), [1] being out2
  // (mesh out) and [2] being in
  QWidget *m_Containers[3] = {};

  enum class MeshFilter
  {
    None,
    TaskGroup,
    Mesh,
  };

  MeshFilter m_CurMeshFilter = MeshFilter::None;
  uint32_t m_FilteredTaskGroup = ~0U, m_FilteredMeshGroup = ~0U;
  uint32_t m_TaskFilterRowOffset = 0, m_MeshFilterRowOffset = 0;

  PopulateBufferData *m_Scrolls = NULL;

  QPoint m_Scroll[4];

  int m_Sequence = 0;

  RDTableView *m_CurView = NULL;
  bool m_CurFixed = false;
  int m_ContextColumn = -1;

  int m_ColumnWidthRowCount = -1;
  int m_IdxColWidth;
  int m_DataColWidth;
  int m_DataRowHeight;
  const int m_ErrorColWidth = 500;

  int previousAxisMappingIndex = 0;

  RichTextViewDelegate *m_delegate = NULL;

  QVBoxLayout *m_VLayout = NULL;
  RDSplitter *m_OuterSplitter = NULL;
  RDSplitter *m_InnerSplitter = NULL;

  CollapseGroupBox *m_FixedGroup = NULL;
  CollapseGroupBox *m_RepeatedGroup = NULL;

  QFrame *m_RepeatedControlBar = NULL;

  RDLabel *m_RepeatedOffset = NULL;

  QMenu *m_HeaderMenu = NULL;

  QAction *m_ResetColumnSel = NULL;
  QAction *m_SelectPosColumn = NULL;
  QAction *m_SelectSecondColumn = NULL;
  QAction *m_SelectSecondAlphaColumn = NULL;

  QMenu *m_ExportMenu = NULL;

  QAction *m_ExportCSV = NULL;
  QAction *m_ExportBytes = NULL;
  QAction *m_DebugVert = NULL;
  QAction *m_FilterMesh = NULL;
  QAction *m_RemoveFilter = NULL;
  QAction *m_GotoTask = NULL;

  RDSpinBox64 *byteRangeStart = NULL;
  RDSpinBox64 *byteRangeLength = NULL;

  RDTableView *tableForStage(MeshDataStage stage);
  BufferItemModel *modelForStage(MeshDataStage stage);

  RDTableView *currentTable() { return tableForStage(m_CurStage); }
  BufferItemModel *currentBufferModel() { return modelForStage(m_CurStage); }
  bool isCurrentRasterOut();
  int currentStageIndex();
  bool isMeshDraw();

  void SetupMeshView();
  void SetupRawView();

  void SetMeshFilter(MeshFilter filter, uint32_t taskGroup = ~0U, uint32_t meshGroup = ~0U);

  void stageRowMenu(MeshDataStage stage, QMenu *menu, const QPoint &pos);
  void meshHeaderMenu(MeshDataStage stage, const QPoint &pos);

  void Reset();

  void ClearModels();

  void UI_ConfigureFormats();
  void UI_ConfigureVertexPipeFormats();
  void UI_ConfigureMeshPipeFormats();

  void UpdateCurrentMeshConfig();
  void EnableCameraGuessControls();

  void CalcColumnWidth(int maxNumRows = 1);
  void ApplyRowAndColumnDims(int numColumns, RDTableView *view, int dataColWidth);

  void SyncViews(RDTableView *primary, bool selection, bool scroll);
  void UpdateHighlightVerts();
  void ScrollToRow(RDTableView *view, int row);
  void ScrollToColumn(RDTableView *view, int column);

  bool showAxisMappingDialog();
};
