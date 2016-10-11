#ifndef RANGEHISTOGRAM_H
#define RANGEHISTOGRAM_H

#include <QWidget>

class RangeHistogram : public QWidget
{
  Q_OBJECT

public:
  explicit RangeHistogram(QWidget *parent = 0);
  ~RangeHistogram();

  void setHistogramRange(float min, float max);
  void setRange(float min, float max);

  void setHistogramData(const QVector<uint32_t> &histogram);

  float blackPoint() { return m_BlackPoint; }
  void setBlackPoint(float val);

  float whitePoint() { return m_WhitePoint; }
  void setWhitePoint(float val);

  float rangeMin() { return m_RangeMin; }
  float rangeMax() { return m_RangeMax; }
signals:
  void rangeUpdated();

protected:
  void mousePressEvent(QMouseEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;
  void mouseMoveEvent(QMouseEvent *e) override;
  void enterEvent(QEvent *e) override;
  void leaveEvent(QEvent *e) override;
  void paintEvent(QPaintEvent *e) override;

private:
  bool ValidRange() const;
  float delta(float val) const;

  static const int m_Margin = 4;
  static const int m_Border = 1;
  static const int m_MarkerSize = 6;

  int totalSpace() const { return m_Margin + m_Border; }
  int regionWidth() const { return rect().width() - totalSpace() * 2; }
  float blackDelta();
  void setBlackDelta(float value);
  float whiteDelta();
  void setWhiteDelta(float value);

  QPoint m_MousePrev = QPoint(-1, -1);

  enum class DraggingMode
  {
    None,
    White,
    Black,
  } m_DragMode;

  QVector<uint32_t> m_HistogramData;
  float m_HistogramMin = 0.0f;
  float m_HistogramMax = 1.0f;

  float m_RangeMin = 0.0f;
  float m_RangeMax = 1.0f;

  float m_BlackPoint = 0.0f;
  float m_WhitePoint = 1.0f;

  float m_MinRangeSize = 1.0e-6f;
};

#endif    // RANGEHISTOGRAM_H
