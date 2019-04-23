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

#include "QRDUtils.h"
#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QCollator>
#include <QDesktopServices>
#include <QElapsedTimer>
#include <QFileSystemModel>
#include <QFontDatabase>
#include <QGridLayout>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMetaMethod>
#include <QMouseEvent>
#include <QPainter>
#include <QProcess>
#include <QProgressBar>
#include <QProgressDialog>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QStandardPaths>
#include <QTextBlock>
#include <QTextDocument>
#include <QtMath>
#include "Code/Resources.h"
#include "Widgets/Extended/RDTreeWidget.h"

// normally this is in the renderdoc core library, but it's needed for the 'unknown enum' path,
// so we implement it here using QString. It's inefficient, but this is a very uncommon path -
// either for invalid values or for when a new enum is added and the code isn't updated
template <>
std::string DoStringise(const uint32_t &el)
{
  return QString::number(el).toStdString();
}

// this one we do by hand as it requires formatting
template <>
std::string DoStringise(const ResourceId &el)
{
  uint64_t num;
  memcpy(&num, &el, sizeof(num));
  return lit("ResourceId::%1").arg(num).toStdString();
}

// this is an opaque struct that contains the data to render, hit-test, etc for some text that
// contains links to resources. It will update and cache the names of the resources.
struct RichResourceText
{
  QVector<QVariant> fragments;

  // cached formatted document. We use cacheId to check if it needs to be updated
  QTextDocument doc;
  int cacheId = 0;

  // a plain-text version of the document, suitable for e.g. copy-paste
  QString text;

  // the ideal width for the document
  int idealWidth = 0;

  // cache the context once we've obtained it.
  ICaptureContext *ctxptr = NULL;

  void cacheDocument(const QWidget *widget)
  {
    if(!ctxptr)
      ctxptr = getCaptureContext(widget);

    if(!ctxptr)
      return;

    ICaptureContext &ctx = *(ICaptureContext *)ctxptr;

    int refCache = ctx.ResourceNameCacheID();

    if(cacheId == refCache)
      return;

    cacheId = refCache;

    // use a table to ensure images don't screw up the baseline for text. DON'T JUDGE ME.
    QString html = lit("<table><tr>");

    int i = 0;

    bool highdpi = widget->devicePixelRatioF() > 1.0;

    QVector<int> fragmentIndexFromBlockIndex;

    // there's an empty block at the start.
    fragmentIndexFromBlockIndex.push_back(-1);

    text.clear();

    for(const QVariant &v : fragments)
    {
      if(v.userType() == qMetaTypeId<ResourceId>())
      {
        QString resname = QString(ctx.GetResourceName(v.value<ResourceId>())).toHtmlEscaped();
        html += lit("<td><b>%1</b></td><td><img width=\"16\" src=':/link%3.png'></td>")
                    .arg(resname)
                    .arg(highdpi ? lit("@2x") : QString());
        text += resname;

        // these generate two blocks (one for each cell)
        fragmentIndexFromBlockIndex.push_back(i);
        fragmentIndexFromBlockIndex.push_back(i);
      }
      else
      {
        html += lit("<td>%1</td>").arg(v.toString().toHtmlEscaped());
        text += v.toString();

        // this only generates one block
        fragmentIndexFromBlockIndex.push_back(i);
      }

      i++;
    }

    // there's another empty block at the end
    fragmentIndexFromBlockIndex.push_back(-1);

    html += lit("</tr></table>");

    doc.setDocumentMargin(0);
    doc.setHtml(html);

    if(doc.blockCount() != fragmentIndexFromBlockIndex.count())
    {
      qCritical() << "Block count is not what's expected!" << doc.blockCount()
                  << fragmentIndexFromBlockIndex.count();

      for(i = 0; i < doc.blockCount(); i++)
        doc.findBlockByNumber(i).setUserState(-1);

      return;
    }

    for(i = 0; i < doc.blockCount(); i++)
      doc.findBlockByNumber(i).setUserState(fragmentIndexFromBlockIndex[i]);

    doc.setTextWidth(-1);
    idealWidth = doc.idealWidth();
    doc.setTextWidth(10000);
  }
};

// we use QSharedPointer to refer to the text since the lifetime management of these objects would
// get quite complicated. There's not necessarily an obvious QObject parent to assign to if the text
// is being initialised before being assigned to a widget and we want the most seamless interface we
// can get.
typedef QSharedPointer<RichResourceText> RichResourceTextPtr;

Q_DECLARE_METATYPE(RichResourceTextPtr);

QString ResIdTextToString(RichResourceTextPtr ptr)
{
  return ptr->text;
}

QString ResIdToString(ResourceId ptr)
{
  return ToQStr(ptr);
}

void RegisterMetatypeConversions()
{
  QMetaType::registerConverter<RichResourceTextPtr, QString>(&ResIdTextToString);
  QMetaType::registerConverter<ResourceId, QString>(&ResIdToString);
}

void RichResourceTextInitialise(QVariant &var)
{
  // we only upconvert from strings, any other type with a string representation is not expected to
  // contain ResourceIds. In particular if the variant is already a ResourceId we can return.
  if(GetVariantMetatype(var) != QMetaType::QString)
    return;

  // we trim the string because that will happen naturally when rendering as HTML, and it makes it
  // easier to detect strings where the only contents are ResourceId text.
  QString text = var.toString().trimmed();

  // do a simple string search first before using regular expressions
  if(!text.contains(lit("ResourceId::")))
    return;

  // use regexp to split up into fragments of text and resourceid. The resourceid is then
  // formatted on the fly in RichResourceText::cacheDocument
  static QRegularExpression re(lit("(ResourceId::)([0-9]*)"));

  QRegularExpressionMatch match = re.match(text);

  if(match.hasMatch())
  {
    // if the match is the whole string, this is just a plain ResourceId on its own, so make that
    // the variant without being rich resource text, so we can process it faster.
    if(match.capturedStart(0) == 0 && match.capturedLength(0) == text.length())
    {
      qulonglong idnum = match.captured(2).toULongLong();
      ResourceId id;
      memcpy(&id, &idnum, sizeof(id));

      var = id;
      return;
    }

    RichResourceTextPtr linkedText(new RichResourceText);

    while(match.hasMatch())
    {
      qulonglong idnum = match.captured(2).toULongLong();
      ResourceId id;
      memcpy(&id, &idnum, sizeof(id));

      // push any text that preceeded the ResourceId.
      if(match.capturedStart(1) > 0)
        linkedText->fragments.push_back(text.left(match.capturedStart(1)));

      text.remove(0, match.capturedEnd(2));

      linkedText->fragments.push_back(id);

      match = re.match(text);
    }

    if(!text.isEmpty())
      linkedText->fragments.push_back(text);

    linkedText->doc.setHtml(text);

    var = QVariant::fromValue(linkedText);
  }
}

bool RichResourceTextCheck(const QVariant &var)
{
  return var.userType() == qMetaTypeId<RichResourceTextPtr>() ||
         var.userType() == qMetaTypeId<ResourceId>();
}

// I'm not sure if this should come from the style or not - the QTextDocument handles this in
// the rich text case.
static const int RichResourceTextMargin = 2;

void RichResourceTextPaint(const QWidget *owner, QPainter *painter, QRect rect, QFont font,
                           QPalette palette, bool mouseOver, QPoint mousePos, const QVariant &var)
{
  // special case handling for ResourceId
  if(var.userType() == qMetaTypeId<ResourceId>())
  {
    QFont origfont = painter->font();
    QFont f = origfont;
    f.setBold(true);
    painter->setFont(f);

    static const int margin = RichResourceTextMargin;

    rect.adjust(margin, 0, -margin * 2, 0);

    QString name;

    ICaptureContext *ctxptr = getCaptureContext(owner);

    ResourceId id = var.value<ResourceId>();

    if(ctxptr)
      name = ctxptr->GetResourceName(id);
    else
      name = ToQStr(id);

    painter->drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, name);

    QRect textRect =
        painter->fontMetrics().boundingRect(rect, Qt::AlignLeft | Qt::AlignVCenter, name);

    const QPixmap &px = Pixmaps::link(owner->devicePixelRatio());

    rect.setTop(textRect.top());
    rect.setWidth(textRect.width() + margin + px.width());
    rect.setHeight(qMax(textRect.height(), px.height()));

    QPoint pos;
    pos.setX(rect.right() - px.width() + 1);
    pos.setY(rect.center().y() - px.height() / 2);

    painter->drawPixmap(pos, px, px.rect());

    if(mouseOver && rect.contains(mousePos) && id != ResourceId())
    {
      int underline_y = textRect.bottom() + margin;

      painter->setPen(QPen(palette.brush(QPalette::WindowText), 1.0));
      painter->drawLine(QPoint(rect.left(), underline_y), QPoint(rect.right(), underline_y));
    }

    painter->setFont(origfont);

    return;
  }

  RichResourceTextPtr linkedText = var.value<RichResourceTextPtr>();

  linkedText->cacheDocument(owner);

  painter->translate(rect.left(), rect.top());

  if(font != linkedText->doc.defaultFont())
    linkedText->doc.setDefaultFont(font);

  // vertical align to the centre, if there's spare room.
  int diff = rect.height() - linkedText->doc.size().height();

  if(diff > 0)
    painter->translate(0, diff / 2);

  linkedText->doc.drawContents(painter, QRectF(0, 0, rect.width(), rect.height()));

  if(mouseOver)
  {
    painter->setPen(QPen(palette.brush(QPalette::WindowText), 1.0));

    QAbstractTextDocumentLayout *layout = linkedText->doc.documentLayout();

    QPoint p = mousePos - rect.topLeft();
    if(diff > 0)
      p -= QPoint(0, diff / 2);

    int pos = layout->hitTest(p, Qt::FuzzyHit);

    if(pos >= 0)
    {
      QTextBlock block = linkedText->doc.findBlock(pos);

      int frag = block.userState();
      if(frag >= 0)
      {
        QVariant v = linkedText->fragments[frag];
        if(v.userType() == qMetaTypeId<ResourceId>() && v.value<ResourceId>() != ResourceId())
        {
          layout->blockBoundingRect(block);
          QRectF blockrect = layout->blockBoundingRect(block);

          if(block.previous().userState() == frag)
          {
            blockrect = blockrect.united(layout->blockBoundingRect(block.previous()));
          }

          if(block.next().userState() == frag)
          {
            blockrect = blockrect.united(layout->blockBoundingRect(block.next()));
          }

          blockrect.translate(0.0, -2.0);

          blockrect.setRight(qMin(blockrect.right(), (qreal)rect.width()));

          painter->drawLine(blockrect.bottomLeft(), blockrect.bottomRight());
        }
      }
    }
  }
}

int RichResourceTextWidthHint(const QWidget *owner, const QFont &font, const QVariant &var)
{
  // special case handling for ResourceId
  if(var.userType() == qMetaTypeId<ResourceId>())
  {
    QFont f = font;
    f.setBold(true);

    static const int margin = RichResourceTextMargin;

    QFontMetrics metrics(f);

    QString name;

    ICaptureContext *ctxptr = getCaptureContext(owner);

    if(ctxptr)
      name = ctxptr->GetResourceName(var.value<ResourceId>());
    else
      name = ToQStr(var.value<ResourceId>());

    const QPixmap &px = Pixmaps::link(owner->devicePixelRatio());

    int ret = margin + metrics.boundingRect(name).width() + margin + px.width() + margin;
    return ret;
  }

  RichResourceTextPtr linkedText = var.value<RichResourceTextPtr>();

  linkedText->cacheDocument(owner);

  return linkedText->idealWidth;
}

bool RichResourceTextMouseEvent(const QWidget *owner, const QVariant &var, QRect rect,
                                const QFont &font, QMouseEvent *event)
{
  // only process clicks or moves
  if(event->type() != QEvent::MouseButtonRelease && event->type() != QEvent::MouseMove)
    return false;

  // special case handling for ResourceId
  if(var.userType() == qMetaTypeId<ResourceId>())
  {
    ResourceId id = var.value<ResourceId>();

    // empty resource ids are not clickable or hover-highlighted.
    if(id == ResourceId())
      return false;

    QFont f = font;
    f.setBold(true);

    static const int margin = RichResourceTextMargin;

    rect.adjust(margin, 0, -margin * 2, 0);

    QString name;

    ICaptureContext *ctxptr = getCaptureContext(owner);

    if(ctxptr)
      name = ctxptr->GetResourceName(id);
    else
      name = ToQStr(id);

    QRect textRect = QFontMetrics(f).boundingRect(rect, Qt::AlignLeft | Qt::AlignVCenter, name);

    const QPixmap &px = Pixmaps::link(owner->devicePixelRatio());

    rect.setTop(textRect.top());
    rect.setWidth(textRect.width() + margin + px.width());
    rect.setHeight(qMax(textRect.height(), px.height()));

    if(rect.contains(event->pos()) && id != ResourceId())
    {
      if(event->type() == QEvent::MouseButtonRelease && ctxptr)
      {
        ICaptureContext &ctx = *(ICaptureContext *)ctxptr;

        if(!ctx.HasResourceInspector())
          ctx.ShowResourceInspector();

        ctx.GetResourceInspector()->Inspect(id);

        ctx.RaiseDockWindow(ctx.GetResourceInspector()->Widget());
      }

      return true;
    }

    return false;
  }

  RichResourceTextPtr linkedText = var.value<RichResourceTextPtr>();

  linkedText->cacheDocument(owner);

  QAbstractTextDocumentLayout *layout = linkedText->doc.documentLayout();

  // vertical align to the centre, if there's spare room.
  int diff = rect.height() - linkedText->doc.size().height();

  QPoint p = event->pos() - rect.topLeft();
  if(diff > 0)
    p -= QPoint(0, diff / 2);

  int pos = layout->hitTest(p, Qt::FuzzyHit);

  if(pos >= 0)
  {
    QTextBlock block = linkedText->doc.findBlock(pos);

    int frag = block.userState();
    if(frag >= 0)
    {
      QVariant v = linkedText->fragments[frag];
      if(v.userType() == qMetaTypeId<ResourceId>())
      {
        // empty resource ids are not clickable or hover-highlighted.
        ResourceId res = v.value<ResourceId>();
        if(res == ResourceId())
          return false;

        if(event->type() == QEvent::MouseButtonRelease && linkedText->ctxptr)
        {
          ICaptureContext &ctx = *(ICaptureContext *)linkedText->ctxptr;

          if(!ctx.HasResourceInspector())
            ctx.ShowResourceInspector();

          ctx.GetResourceInspector()->Inspect(res);

          ctx.RaiseDockWindow(ctx.GetResourceInspector()->Widget());
        }

        return true;
      }
    }
  }

  return false;
}

RichTextViewDelegate::RichTextViewDelegate(QAbstractItemView *parent)
    : m_widget(parent), ForwardingDelegate(parent)
{
}

RichTextViewDelegate::~RichTextViewDelegate()
{
}

void RichTextViewDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                 const QModelIndex &index) const
{
  if(index.isValid())
  {
    QVariant v = index.data();

    if(RichResourceTextCheck(v))
    {
      // draw the item without text, so we get the proper background/selection/etc.
      // we'd like to be able to use the parent delegate's paint here, but either it calls to
      // QStyledItemDelegate which will re-fetch the text (bleh), or it calls to the manual
      // delegate which could do anything. So for this case we just use the style and skip the
      // delegate and hope it works out.
      QStyleOptionViewItem opt = option;
      QStyledItemDelegate::initStyleOption(&opt, index);
      opt.text.clear();
      m_widget->style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter, m_widget);

      painter->save();

      QRect rect = option.rect;
      if(!opt.icon.isNull())
      {
        QIcon::Mode mode;
        if((opt.state & QStyle::State_Enabled) == 0)
          mode = QIcon::Disabled;
        else if(opt.state & QStyle::State_Selected)
          mode = QIcon::Selected;
        else
          mode = QIcon::Normal;
        QIcon::State state = opt.state & QStyle::State_Open ? QIcon::On : QIcon::Off;
        rect.setX(rect.x() + opt.icon.actualSize(opt.decorationSize, mode, state).width() + 4);
      }

      RichResourceTextPaint(m_widget, painter, rect, opt.font, option.palette,
                            option.state & QStyle::State_MouseOver,
                            m_widget->viewport()->mapFromGlobal(QCursor::pos()), v);

      painter->restore();
      return;
    }
  }

  return ForwardingDelegate::paint(painter, option, index);
}

QSize RichTextViewDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
  if(index.isValid())
  {
    QVariant v = index.data();

    if(RichResourceTextCheck(v))
      return QSize(RichResourceTextWidthHint(m_widget, option.font, v), option.fontMetrics.height());
  }

  return ForwardingDelegate::sizeHint(option, index);
}

bool RichTextViewDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
                                       const QStyleOptionViewItem &option, const QModelIndex &index)
{
  if(event->type() == QEvent::MouseButtonRelease && index.isValid())
  {
    QVariant v = index.data();

    if(RichResourceTextCheck(v))
    {
      QRect rect = option.rect;

      QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();

      if(!icon.isNull())
      {
        rect.setX(rect.x() +
                  icon.actualSize(option.decorationSize, QIcon::Normal, QIcon::On).width() + 4);
      }

      // ignore the return value, we always consume clicks on this cell
      RichResourceTextMouseEvent(m_widget, v, rect, option.font, (QMouseEvent *)event);
      return true;
    }
  }

  return ForwardingDelegate::editorEvent(event, model, option, index);
}

bool RichTextViewDelegate::linkHover(QMouseEvent *e, const QFont &font, const QModelIndex &index)
{
  if(index.isValid())
  {
    QVariant v = index.data();

    if(RichResourceTextCheck(v))
    {
      QRect rect = m_widget->visualRect(index);

      QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();

      if(!icon.isNull())
      {
        rect.setX(
            rect.x() +
            icon.actualSize(QSize(rect.height(), rect.height()), QIcon::Normal, QIcon::On).width() +
            4);
      }

      return RichResourceTextMouseEvent(m_widget, v, rect, font, e);
    }
  }

  return false;
}

#include "renderdoc_tostr.inl"

QString ToQStr(const ResourceUsage usage, const GraphicsAPI apitype)
{
  if(IsD3D(apitype))
  {
    switch(usage)
    {
      case ResourceUsage::VertexBuffer: return lit("Vertex Buffer");
      case ResourceUsage::IndexBuffer: return lit("Index Buffer");

      case ResourceUsage::VS_Constants: return lit("VS - Constant Buffer");
      case ResourceUsage::GS_Constants: return lit("GS - Constant Buffer");
      case ResourceUsage::HS_Constants: return lit("HS - Constant Buffer");
      case ResourceUsage::DS_Constants: return lit("DS - Constant Buffer");
      case ResourceUsage::CS_Constants: return lit("CS - Constant Buffer");
      case ResourceUsage::PS_Constants: return lit("PS - Constant Buffer");
      case ResourceUsage::All_Constants: return lit("All - Constant Buffer");

      case ResourceUsage::StreamOut: return lit("Stream Out");

      case ResourceUsage::VS_Resource: return lit("VS - Resource");
      case ResourceUsage::GS_Resource: return lit("GS - Resource");
      case ResourceUsage::HS_Resource: return lit("HS - Resource");
      case ResourceUsage::DS_Resource: return lit("DS - Resource");
      case ResourceUsage::CS_Resource: return lit("CS - Resource");
      case ResourceUsage::PS_Resource: return lit("PS - Resource");
      case ResourceUsage::All_Resource: return lit("All - Resource");

      case ResourceUsage::VS_RWResource: return lit("VS - UAV");
      case ResourceUsage::HS_RWResource: return lit("HS - UAV");
      case ResourceUsage::DS_RWResource: return lit("DS - UAV");
      case ResourceUsage::GS_RWResource: return lit("GS - UAV");
      case ResourceUsage::PS_RWResource: return lit("PS - UAV");
      case ResourceUsage::CS_RWResource: return lit("CS - UAV");
      case ResourceUsage::All_RWResource: return lit("All - UAV");

      case ResourceUsage::InputTarget: return lit("Color Input");
      case ResourceUsage::ColorTarget: return lit("Rendertarget");
      case ResourceUsage::DepthStencilTarget: return lit("Depthstencil");

      case ResourceUsage::Indirect: return lit("Indirect argument");

      case ResourceUsage::Clear: return lit("Clear");

      case ResourceUsage::GenMips: return lit("Generate Mips");
      case ResourceUsage::Resolve: return lit("Resolve");
      case ResourceUsage::ResolveSrc: return lit("Resolve - Source");
      case ResourceUsage::ResolveDst: return lit("Resolve - Dest");
      case ResourceUsage::Copy: return lit("Copy");
      case ResourceUsage::CopySrc: return lit("Copy - Source");
      case ResourceUsage::CopyDst: return lit("Copy - Dest");

      case ResourceUsage::Barrier: return lit("Barrier");
      default: break;
    }
  }
  else if(apitype == GraphicsAPI::OpenGL || apitype == GraphicsAPI::Vulkan)
  {
    const bool vk = (apitype == GraphicsAPI::Vulkan);

    switch(usage)
    {
      case ResourceUsage::VertexBuffer: return lit("Vertex Buffer");
      case ResourceUsage::IndexBuffer: return lit("Index Buffer");

      case ResourceUsage::VS_Constants: return lit("VS - Uniform Buffer");
      case ResourceUsage::GS_Constants: return lit("GS - Uniform Buffer");
      case ResourceUsage::HS_Constants: return lit("HS - Uniform Buffer");
      case ResourceUsage::DS_Constants: return lit("DS - Uniform Buffer");
      case ResourceUsage::CS_Constants: return lit("CS - Uniform Buffer");
      case ResourceUsage::PS_Constants: return lit("PS - Uniform Buffer");
      case ResourceUsage::All_Constants: return lit("All - Uniform Buffer");

      case ResourceUsage::StreamOut: return lit("Transform Feedback");

      case ResourceUsage::VS_Resource: return lit("VS - Texture");
      case ResourceUsage::GS_Resource: return lit("GS - Texture");
      case ResourceUsage::HS_Resource: return lit("HS - Texture");
      case ResourceUsage::DS_Resource: return lit("DS - Texture");
      case ResourceUsage::CS_Resource: return lit("CS - Texture");
      case ResourceUsage::PS_Resource: return lit("PS - Texture");
      case ResourceUsage::All_Resource: return lit("All - Texture");

      case ResourceUsage::VS_RWResource: return lit("VS - Image/SSBO");
      case ResourceUsage::HS_RWResource: return lit("HS - Image/SSBO");
      case ResourceUsage::DS_RWResource: return lit("DS - Image/SSBO");
      case ResourceUsage::GS_RWResource: return lit("GS - Image/SSBO");
      case ResourceUsage::PS_RWResource: return lit("PS - Image/SSBO");
      case ResourceUsage::CS_RWResource: return lit("CS - Image/SSBO");
      case ResourceUsage::All_RWResource: return lit("All - Image/SSBO");

      case ResourceUsage::InputTarget: return lit("FBO Input");
      case ResourceUsage::ColorTarget: return lit("FBO Color");
      case ResourceUsage::DepthStencilTarget: return lit("FBO Depthstencil");

      case ResourceUsage::Indirect: return lit("Indirect argument");

      case ResourceUsage::Clear: return lit("Clear");

      case ResourceUsage::GenMips: return lit("Generate Mips");
      case ResourceUsage::Resolve: return vk ? lit("Resolve") : lit("Framebuffer blit");
      case ResourceUsage::ResolveSrc:
        return vk ? lit("Resolve - Source") : lit("Framebuffer blit - Source");
      case ResourceUsage::ResolveDst:
        return vk ? lit("Resolve - Dest") : lit("Framebuffer blit - Dest");
      case ResourceUsage::Copy: return lit("Copy");
      case ResourceUsage::CopySrc: return lit("Copy - Source");
      case ResourceUsage::CopyDst: return lit("Copy - Dest");

      case ResourceUsage::Barrier: return lit("Barrier");
      default: break;
    }
  }

  return lit("Unknown");
}

QString ToQStr(const ShaderStage stage, const GraphicsAPI apitype)
{
  if(IsD3D(apitype))
  {
    switch(stage)
    {
      case ShaderStage::Vertex: return lit("Vertex");
      case ShaderStage::Hull: return lit("Hull");
      case ShaderStage::Domain: return lit("Domain");
      case ShaderStage::Geometry: return lit("Geometry");
      case ShaderStage::Pixel: return lit("Pixel");
      case ShaderStage::Compute: return lit("Compute");
      default: break;
    }
  }
  else if(apitype == GraphicsAPI::OpenGL || apitype == GraphicsAPI::Vulkan)
  {
    switch(stage)
    {
      case ShaderStage::Vertex: return lit("Vertex");
      case ShaderStage::Tess_Control: return lit("Tess. Control");
      case ShaderStage::Tess_Eval: return lit("Tess. Eval");
      case ShaderStage::Geometry: return lit("Geometry");
      case ShaderStage::Fragment: return lit("Fragment");
      case ShaderStage::Compute: return lit("Compute");
      default: break;
    }
  }

  return lit("Unknown");
}

QString TypeString(const SigParameter &sig)
{
  QString ret = lit("");

  if(sig.compType == CompType::Float)
    ret += lit("float");
  else if(sig.compType == CompType::UInt || sig.compType == CompType::UScaled)
    ret += lit("uint");
  else if(sig.compType == CompType::SInt || sig.compType == CompType::SScaled)
    ret += lit("int");
  else if(sig.compType == CompType::UNorm || sig.compType == CompType::UNormSRGB)
    ret += lit("unorm float");
  else if(sig.compType == CompType::SNorm)
    ret += lit("snorm float");
  else if(sig.compType == CompType::Depth)
    ret += lit("float");

  if(sig.compCount > 1)
    ret += QString::number(sig.compCount);

  return ret;
}

QString D3DSemanticString(const SigParameter &sig)
{
  if(sig.systemValue == ShaderBuiltin::Undefined)
    return sig.semanticIdxName;

  QString sysValues[ENUM_ARRAY_SIZE(ShaderBuiltin)] = {
      lit("SV_Undefined"),
      lit("SV_Position"),
      lit("Unsupported (PointSize)"),
      lit("SV_ClipDistance"),
      lit("SV_CullDistance"),
      lit("SV_RenderTargetIndex"),
      lit("SV_ViewportIndex"),
      lit("SV_VertexID"),
      lit("SV_PrimitiveID"),
      lit("SV_InstanceID"),
      lit("Unsupported (DispatchSize)"),
      lit("SV_DispatchThreadID"),
      lit("SV_GroupID"),
      lit("SV_GroupIndex"),
      lit("SV_GroupThreadID"),
      lit("SV_GSInstanceID"),
      lit("SV_OutputControlPointID"),
      lit("SV_DomainLocation"),
      lit("SV_IsFrontFace"),
      lit("SV_Coverage"),
      lit("Unsupported (SamplePosition)"),
      lit("SV_SampleIndex"),
      lit("Unsupported (PatchNumVertices)"),
      lit("SV_TessFactor"),
      lit("SV_InsideTessFactor"),
      lit("SV_Target"),
      lit("SV_Depth"),
      lit("SV_DepthGreaterEqual"),
      lit("SV_DepthLessEqual"),
  };

  QString ret = sysValues[size_t(sig.systemValue)];

  // need to include the index if it's a system value semantic that's numbered
  if(sig.systemValue == ShaderBuiltin::ColorOutput ||
     sig.systemValue == ShaderBuiltin::CullDistance || sig.systemValue == ShaderBuiltin::ClipDistance)
    ret += QString::number(sig.semanticIndex);

  return ret;
}

QString GetComponentString(byte mask)
{
  QString ret;

  if((mask & 0x1) > 0)
    ret += lit("R");
  if((mask & 0x2) > 0)
    ret += lit("G");
  if((mask & 0x4) > 0)
    ret += lit("B");
  if((mask & 0x8) > 0)
    ret += lit("A");

  return ret;
}

void CombineUsageEvents(ICaptureContext &ctx, const rdcarray<EventUsage> &usage,
                        std::function<void(uint32_t startEID, uint32_t endEID, ResourceUsage use)> callback)
{
  uint32_t start = 0;
  uint32_t end = 0;
  ResourceUsage us = ResourceUsage::IndexBuffer;

  for(const EventUsage u : usage)
  {
    if(start == 0)
    {
      start = end = u.eventId;
      us = u.usage;
      continue;
    }

    const DrawcallDescription *draw = ctx.GetDrawcall(u.eventId);

    bool distinct = false;

    // if the usage is different from the last, add a new entry,
    // or if the previous draw link is broken.
    if(u.usage != us || draw == NULL || draw->previous == 0)
    {
      distinct = true;
    }
    else
    {
      // otherwise search back through real draws, to see if the
      // last event was where we were - otherwise it's a new
      // distinct set of drawcalls and should have a separate
      // entry in the context menu
      const DrawcallDescription *prev = draw->previous;

      while(prev != NULL && prev->eventId > end)
      {
        if(!(prev->flags & (DrawFlags::Dispatch | DrawFlags::Drawcall | DrawFlags::CmdList)))
        {
          prev = prev->previous;
        }
        else
        {
          distinct = true;
          break;
        }

        if(prev == NULL)
          distinct = true;
      }
    }

    if(distinct)
    {
      callback(start, end, us);
      start = end = u.eventId;
      us = u.usage;
    }

    end = u.eventId;
  }

  if(start != 0)
    callback(start, end, us);
}

void addStructuredObjects(RDTreeWidgetItem *parent, const StructuredObjectList &objs,
                          bool parentIsArray)
{
  for(const SDObject *obj : objs)
  {
    if(obj->type.flags & SDTypeFlags::Hidden)
      continue;

    QVariant param;

    if(parentIsArray)
      param = QFormatStr("[%1]").arg(parent->childCount());
    else
      param = obj->name;

    RDTreeWidgetItem *item = new RDTreeWidgetItem({param, QString()});

    // we don't identify via the type name as many types could be serialised as a ResourceId -
    // e.g. ID3D11Resource* or ID3D11Buffer* which would be the actual typename. We want to preserve
    // that for the best raw structured data representation instead of flattening those out to just
    // "ResourceId", and we also don't want to store two types ('fake' and 'real'), so instead we
    // check the custom string.
    if(obj->type.basetype == SDBasic::Resource)
    {
      ResourceId id;
      static_assert(sizeof(id) == sizeof(obj->data.basic.u), "ResourceId is no longer uint64_t!");
      memcpy(&id, &obj->data.basic.u, sizeof(id));

      param = id;
    }
    else if(obj->type.flags & SDTypeFlags::NullString)
    {
      param = lit("NULL");
    }
    else if(obj->type.flags & SDTypeFlags::HasCustomString)
    {
      param = obj->data.str;
    }
    else
    {
      switch(obj->type.basetype)
      {
        case SDBasic::Chunk:
        case SDBasic::Struct:
          param = QFormatStr("%1()").arg(obj->type.name);
          addStructuredObjects(item, obj->data.children, false);
          break;
        case SDBasic::Array:
          param = QFormatStr("%1[]").arg(obj->type.name);
          addStructuredObjects(item, obj->data.children, true);
          break;
        case SDBasic::Null: param = lit("NULL"); break;
        case SDBasic::Buffer: param = lit("(%1 bytes)").arg(obj->type.byteSize); break;
        case SDBasic::String:
        {
          QStringList lines = QString(obj->data.str).split(QLatin1Char('\n'));
          QString trimmedStr;
          for(int i = 0; i < 3 && i < lines.count(); i++)
            trimmedStr += lines[i] + QLatin1Char('\n');
          if(lines.count() > 3)
            trimmedStr += lit("...");
          param = trimmedStr.trimmed();
          break;
        }
        case SDBasic::Resource:
        case SDBasic::Enum:
        case SDBasic::UnsignedInteger: param = Formatter::HumanFormat(obj->data.basic.u); break;
        case SDBasic::SignedInteger: param = Formatter::Format(obj->data.basic.i); break;
        case SDBasic::Float: param = Formatter::Format(obj->data.basic.d); break;
        case SDBasic::Boolean: param = (obj->data.basic.b ? lit("True") : lit("False")); break;
        case SDBasic::Character: param = QString(QLatin1Char(obj->data.basic.c)); break;
      }
    }

    item->setText(1, param);

    parent->addChild(item);
  }
}

bool SaveToJSON(QVariantMap &data, QIODevice &f, const char *magicIdentifier, uint32_t magicVersion)
{
  // marker that this data is valid
  if(magicIdentifier)
    data[QString::fromLatin1(magicIdentifier)] = magicVersion;

  QJsonDocument doc = QJsonDocument::fromVariant(data);

  if(doc.isEmpty() || doc.isNull())
  {
    qCritical() << "Failed to convert data to JSON document";
    return false;
  }

  QByteArray jsontext = doc.toJson(QJsonDocument::Indented);

  qint64 ret = f.write(jsontext);

  if(ret != jsontext.size())
  {
    qCritical() << "Failed to write JSON data: " << ret << " " << f.errorString();
    return false;
  }

  return true;
}

bool LoadFromJSON(QVariantMap &data, QIODevice &f, const char *magicIdentifier, uint32_t magicVersion)
{
  QByteArray json = f.readAll();

  if(json.isEmpty())
  {
    qCritical() << "Read invalid empty JSON data from file " << f.errorString();
    return false;
  }

  QJsonDocument doc = QJsonDocument::fromJson(json);

  if(doc.isEmpty() || doc.isNull())
  {
    qCritical() << "Failed to convert file to JSON document";
    return false;
  }

  data = doc.toVariant().toMap();

  QString ident = QString::fromLatin1(magicIdentifier);

  if(data.isEmpty() || !data.contains(ident))
  {
    qCritical() << "Converted config data is invalid or unrecognised";
    return false;
  }

  if(data[ident].toUInt() != magicVersion)
  {
    qCritical() << "Converted config data is not the right version";
    return false;
  }

  return true;
}

QString VariantToJSON(const QVariantMap &data)
{
  return QString::fromUtf8(QJsonDocument::fromVariant(data).toJson(QJsonDocument::Indented));
}

QVariantMap JSONToVariant(const QString &json)
{
  return QJsonDocument::fromJson(json.toUtf8()).toVariant().toMap();
}

int GUIInvoke::methodIndex = -1;

void GUIInvoke::init()
{
  GUIInvoke *invoke = new GUIInvoke(NULL, {});
  methodIndex = invoke->metaObject()->indexOfMethod(QMetaObject::normalizedSignature("doInvoke()"));
  invoke->deleteLater();
}

void GUIInvoke::call(QObject *obj, const std::function<void()> &f)
{
  if(!obj)
    qCritical() << "GUIInvoke::call called with NULL object";

  if(onUIThread())
  {
    if(obj)
      f();
    return;
  }

  defer(obj, f);
}

void GUIInvoke::defer(QObject *obj, const std::function<void()> &f)
{
  if(!obj)
    qCritical() << "GUIInvoke::defer called with NULL object";

  GUIInvoke *invoke = new GUIInvoke(obj, f);
  invoke->moveToThread(qApp->thread());
  invoke->metaObject()->method(methodIndex).invoke(invoke, Qt::QueuedConnection);
}

void GUIInvoke::blockcall(QObject *obj, const std::function<void()> &f)
{
  if(!obj)
    qCritical() << "GUIInvoke::blockcall called with NULL object";

  if(onUIThread())
  {
    if(obj)
      f();
    return;
  }

  GUIInvoke *invoke = new GUIInvoke(obj, f);
  invoke->moveToThread(qApp->thread());
  invoke->metaObject()->method(methodIndex).invoke(invoke, Qt::BlockingQueuedConnection);
}

bool GUIInvoke::onUIThread()
{
  return qApp->thread() == QThread::currentThread();
}

QString RDDialog::DefaultBrowsePath;

const QMessageBox::StandardButtons RDDialog::YesNoCancel =
    QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

void RDDialog::show(QMenu *menu, QPoint pos)
{
  // menus aren't always visible immediately, so we need to listen for aboutToHide to exit the event
  // loop. As a safety precaution because I don't trust the damn signals, if we loop for over a
  // second then we'll quit as soon as the menu is not visible
  volatile bool menuHiding = false;
  auto connection =
      QObject::connect(menu, &QMenu::aboutToHide, [&menuHiding]() { menuHiding = true; });

  menu->setWindowModality(Qt::ApplicationModal);
  menu->popup(pos);

  QElapsedTimer elapsed;
  elapsed.start();

  QEventLoop loop;
  for(;;)
  {
    // stop processing once aboutToHide has been signalled
    if(menuHiding)
      break;

    // stop processing if 1s has passed and the menu isn't visible anymore.
    if(elapsed.hasExpired(1000) && !menu->isVisible())
      break;

    loop.processEvents(QEventLoop::WaitForMoreEvents);
    QCoreApplication::sendPostedEvents();
  }

  QObject::disconnect(connection);
}

int RDDialog::show(QDialog *dialog)
{
// workaround for QTBUG-56382 needed on windows only - it can break on other platforms
#if defined(Q_OS_WIN32)
  dialog->setWindowModality(Qt::ApplicationModal);
  dialog->show();
  QEventLoop loop;
  while(dialog->isVisible())
  {
    loop.processEvents(QEventLoop::WaitForMoreEvents);
    QCoreApplication::sendPostedEvents();
  }
#else
  dialog->exec();
#endif

  return dialog->result();
}

QMessageBox::StandardButton RDDialog::messageBox(QMessageBox::Icon icon, QWidget *parent,
                                                 const QString &title, const QString &text,
                                                 QMessageBox::StandardButtons buttons,
                                                 QMessageBox::StandardButton defaultButton)
{
  QMessageBox::StandardButton ret = defaultButton;

  QObject *parentObj = parent;

  if(parentObj == NULL)
  {
    // for 'global' message boxes with no parents, just use the app as the parent pointer
    parentObj = qApp;
  }

  // if we're already on the right thread, this boils down to a function call
  GUIInvoke::blockcall(parentObj, [&]() {
    QMessageBox mb(icon, title, text, buttons, parent);
    mb.setDefaultButton(defaultButton);
    show(&mb);
    ret = mb.standardButton(mb.clickedButton());
  });
  return ret;
}

QMessageBox::StandardButton RDDialog::messageBoxChecked(QMessageBox::Icon icon, QWidget *parent,
                                                        const QString &title, const QString &text,
                                                        QCheckBox *checkBox, bool &checked,
                                                        QMessageBox::StandardButtons buttons,
                                                        QMessageBox::StandardButton defaultButton)
{
  QMessageBox::StandardButton ret = defaultButton;

  // if we're already on the right thread, this boils down to a function call
  GUIInvoke::blockcall(parent, [&]() {
    QMessageBox mb(icon, title, text, buttons, parent);
    mb.setDefaultButton(defaultButton);
    mb.setCheckBox(checkBox);
    show(&mb);
    checked = mb.checkBox()->isChecked();
    ret = mb.standardButton(mb.clickedButton());
  });

  return ret;
}

QString RDDialog::getExistingDirectory(QWidget *parent, const QString &caption, const QString &dir,
                                       QFileDialog::Options options)
{
  QFileDialog fd(parent, caption, dir, QString());
  fd.setAcceptMode(QFileDialog::AcceptOpen);
  fd.setFileMode(QFileDialog::DirectoryOnly);
  fd.setOptions(options);
  show(&fd);

  if(fd.result() == QFileDialog::Accepted)
  {
    QStringList files = fd.selectedFiles();
    if(!files.isEmpty())
      return files[0];
  }

  return QString();
}

QString RDDialog::getOpenFileName(QWidget *parent, const QString &caption, const QString &dir,
                                  const QString &filter, QString *selectedFilter,
                                  QFileDialog::Options options)
{
  QString d = dir;
  if(d.isEmpty())
    d = DefaultBrowsePath;

  QFileDialog fd(parent, caption, d, filter);
  fd.setFileMode(QFileDialog::ExistingFile);
  fd.setAcceptMode(QFileDialog::AcceptOpen);
  fd.setOptions(options);
  show(&fd);

  if(fd.result() == QFileDialog::Accepted)
  {
    if(selectedFilter)
      *selectedFilter = fd.selectedNameFilter();

    QStringList files = fd.selectedFiles();
    if(!files.isEmpty())
    {
      DefaultBrowsePath = QFileInfo(files[0]).dir().absolutePath();
      return files[0];
    }
  }

  return QString();
}

QString RDDialog::getExecutableFileName(QWidget *parent, const QString &caption, const QString &dir,
                                        const QString &defaultExe, QFileDialog::Options options)
{
  QString d = dir;
  if(d.isEmpty())
    d = DefaultBrowsePath;

  QString filter;

#if defined(Q_OS_WIN32)
  // can't filter by executable bit on windows, but we have extensions
  filter = QApplication::translate("RDDialog", "Executables (*.exe);;All Files (*)");
#endif

  QFileDialog fd(parent, caption, d, filter);
  fd.setOptions(options);
  fd.setAcceptMode(QFileDialog::AcceptOpen);
  fd.setFileMode(QFileDialog::ExistingFile);
  {
    QFileFilterModel *fileProxy = new QFileFilterModel(parent);
    fileProxy->setRequirePermissions(QDir::Executable);
    fd.setProxyModel(fileProxy);
  }
  if(!defaultExe.isEmpty())
    fd.selectFile(defaultExe);
  show(&fd);

  if(fd.result() == QFileDialog::Accepted)
  {
    QStringList files = fd.selectedFiles();
    if(!files.isEmpty())
    {
      DefaultBrowsePath = QFileInfo(files[0]).dir().absolutePath();
      return files[0];
    }
  }

  return QString();
}

static QStringList getDefaultSuffixesFromFilter(const QString &filter)
{
  // capture the first suffix found and discard the rest
  static const QRegularExpression regex(lit("\\*\\.([\\w.]+).*"));

  QStringList suffixes;
  for(const QString &s : filter.split(lit(";;")))
  {
    suffixes << regex.match(s).captured(1);
  }
  return suffixes;
}

QString RDDialog::getSaveFileName(QWidget *parent, const QString &caption, const QString &dir,
                                  const QString &filter, QString *selectedFilter,
                                  QFileDialog::Options options)
{
  QString d = dir;
  if(d.isEmpty())
    d = DefaultBrowsePath;

  QFileDialog fd(parent, caption, d, filter);
  fd.setAcceptMode(QFileDialog::AcceptSave);
  fd.setOptions(options);
  const QStringList &defaultSuffixes = getDefaultSuffixesFromFilter(filter);
  if(!defaultSuffixes.isEmpty())
    fd.setDefaultSuffix(defaultSuffixes.first());
  QObject::connect(&fd, &QFileDialog::filterSelected, [&](const QString &filter) {
    int i = fd.nameFilters().indexOf(filter);
    fd.setDefaultSuffix(defaultSuffixes.value(i));
  });
  show(&fd);

  if(fd.result() == QFileDialog::Accepted)
  {
    if(selectedFilter)
      *selectedFilter = fd.selectedNameFilter();

    QStringList files = fd.selectedFiles();
    if(!files.isEmpty())
    {
      DefaultBrowsePath = QFileInfo(files[0]).dir().absolutePath();
      return files[0];
    }
  }

  return QString();
}

bool QFileFilterModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
  QModelIndex idx = sourceModel()->index(source_row, 0, source_parent);

  QFileSystemModel *fs = qobject_cast<QFileSystemModel *>(sourceModel());

  if(!fs)
  {
    qCritical() << "Expected a QFileSystemModel as the source model!";
    return true;
  }

  if(fs->isDir(idx))
    return true;

  QFile::Permissions permissions =
      (QFile::Permissions)sourceModel()->data(idx, QFileSystemModel::FilePermissions).toInt();

  if((m_requireMask & QDir::Readable) && !(permissions & QFile::ReadUser))
    return false;
  if((m_requireMask & QDir::Writable) && !(permissions & QFile::WriteUser))
    return false;
  if((m_requireMask & QDir::Executable) && !(permissions & QFile::ExeUser))
    return false;

  if((m_excludeMask & QDir::Readable) && (permissions & QFile::ReadUser))
    return false;
  if((m_excludeMask & QDir::Writable) && (permissions & QFile::WriteUser))
    return false;
  if((m_excludeMask & QDir::Executable) && (permissions & QFile::ExeUser))
    return false;

  return true;
}

QCollatorSortFilterProxyModel::QCollatorSortFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
  m_collator = new QCollator();
}

QCollatorSortFilterProxyModel::~QCollatorSortFilterProxyModel()
{
  delete m_collator;
}

bool QCollatorSortFilterProxyModel::lessThan(const QModelIndex &source_left,
                                             const QModelIndex &source_right) const
{
  return m_collator->compare(sourceModel()->data(source_left, sortRole()).toString(),
                             sourceModel()->data(source_right, sortRole()).toString()) < 0;
}

void addGridLines(QGridLayout *grid, QColor gridColor)
{
  QString style =
      QFormatStr("border: solid #%1%2%3; border-bottom-width: 1px; border-right-width: 1px;")
          .arg(gridColor.red(), 2, 16, QLatin1Char('0'))
          .arg(gridColor.green(), 2, 16, QLatin1Char('0'))
          .arg(gridColor.blue(), 2, 16, QLatin1Char('0'));

  for(int y = 0; y < grid->rowCount(); y++)
  {
    for(int x = 0; x < grid->columnCount(); x++)
    {
      QLayoutItem *item = grid->itemAtPosition(y, x);

      if(item == NULL)
        continue;

      QWidget *w = item->widget();

      if(w == NULL)
        continue;

      QString cellStyle = style;

      if(x == 0)
        cellStyle += lit("border-left-width: 1px;");
      if(y == 0)
        cellStyle += lit("border-top-width: 1px;");

      w->setStyleSheet(cellStyle);
    }
  }
}

int Formatter::m_minFigures = 2, Formatter::m_maxFigures = 5, Formatter::m_expNegCutoff = 5,
    Formatter::m_expPosCutoff = 7;
double Formatter::m_expNegValue = 0.00001;       // 10^(-5)
double Formatter::m_expPosValue = 10000000.0;    // 10^7
QFont *Formatter::m_Font = NULL;
QColor Formatter::m_DarkChecker, Formatter::m_LightChecker;

void Formatter::setParams(const PersistantConfig &config)
{
  m_minFigures = qMax(0, config.Formatter_MinFigures);
  m_maxFigures = qMax(2, config.Formatter_MaxFigures);
  m_expNegCutoff = qMax(0, config.Formatter_NegExp);
  m_expPosCutoff = qMax(0, config.Formatter_PosExp);

  m_expNegValue = qPow(10.0, -config.Formatter_NegExp);
  m_expPosValue = qPow(10.0, config.Formatter_PosExp);

  if(!m_Font)
    m_Font = new QFont();
  *m_Font =
      config.Font_PreferMonospaced ? QFontDatabase::systemFont(QFontDatabase::FixedFont) : QFont();

  Formatter::setPalette(QApplication::palette());
}

void Formatter::setPalette(QPalette palette)
{
  m_DarkChecker = palette.color(QPalette::Mid);
  m_LightChecker = m_DarkChecker.lighter(150);

  RENDERDOC_SetColors(m_DarkChecker, m_LightChecker, IsDarkTheme());
}

void Formatter::shutdown()
{
  delete m_Font;
}

QString Formatter::Format(double f, bool)
{
  if(f != 0.0 && (qAbs(f) < m_expNegValue || qAbs(f) > m_expPosValue))
    return QFormatStr("%1").arg(f, -m_minFigures, 'E', m_maxFigures);

  QString ret = QFormatStr("%1").arg(f, 0, 'f', m_maxFigures);

  // trim excess trailing 0s
  int decimal = ret.lastIndexOf(QLatin1Char('.'));
  if(decimal > 0)
  {
    decimal += m_minFigures;

    const int len = ret.count();

    int remove = 0;
    while(len - remove - 1 > decimal && ret.at(len - remove - 1) == QLatin1Char('0'))
      remove++;

    if(remove > 0)
      ret.chop(remove);
  }

  return ret;
}

QString Formatter::HumanFormat(uint64_t u)
{
  if(u == UINT16_MAX)
    return lit("UINT16_MAX");
  if(u == UINT32_MAX)
    return lit("UINT32_MAX");
  if(u == UINT64_MAX)
    return lit("UINT64_MAX");

  // format as hex when over a certain threshold
  if(u > 0xffffff)
    return lit("0x") + Format(u, true);

  return Format(u);
}

class RDProgressDialog : public QProgressDialog
{
public:
  RDProgressDialog(const QString &labelText, QWidget *parent)
      // we add 1 so that the progress value never hits maximum until we are actually finished
      : QProgressDialog(labelText, QString(), 0, maxProgress + 1, parent),
        m_Label(this)
  {
    setWindowTitle(tr("Please Wait"));
    setWindowFlags(Qt::CustomizeWindowHint | Qt::Dialog | Qt::WindowTitleHint);
    setWindowIcon(QIcon());
    setMinimumSize(QSize(250, 0));
    setMaximumSize(QSize(250, 10000));
    setCancelButton(NULL);
    setMinimumDuration(0);
    setWindowModality(Qt::ApplicationModal);
    setValue(0);

    m_Label.setText(labelText);
    m_Label.setAlignment(Qt::AlignCenter);
    m_Label.setWordWrap(true);

    setLabel(&m_Label);
  }

  void setPercentage(float percent) { setValue(int(maxProgress * percent)); }
  void setInfinite(bool infinite)
  {
    if(infinite)
    {
      setMinimum(0);
      setMaximum(0);
      setValue(0);
    }
    else
    {
      setMinimum(0);
      setMaximum(maxProgress + 1);
      setValue(0);
    }
  }

  void closeAndReset()
  {
    setValue(maxProgress);
    hide();
    reset();
  }

protected:
  void keyPressEvent(QKeyEvent *e) override
  {
    if(e->key() == Qt::Key_Escape)
      return;

    QProgressDialog::keyPressEvent(e);
  }

  QLabel m_Label;

  static const int maxProgress = 1000;
};

#if defined(Q_OS_WIN32)

#include <windows.h>

#include <shellapi.h>

typedef LSTATUS(APIENTRY *PFN_RegCreateKeyExA)(HKEY hKey, LPCSTR lpSubKey, DWORD Reserved,
                                               LPSTR lpClass, DWORD dwOptions, REGSAM samDesired,
                                               CONST LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                               PHKEY phkResult, LPDWORD lpdwDisposition);

typedef LSTATUS(APIENTRY *PFN_RegCloseKey)(HKEY hKey);

#else

#include <unistd.h>

#endif

bool IsRunningAsAdmin()
{
#if defined(Q_OS_WIN32)
  // try to open HKLM\Software for write.
  HKEY key = NULL;

  // access dynamically to get around the pain of trying to link to extra window libs in qt
  HMODULE mod = LoadLibraryA("advapi32.dll");

  if(mod == NULL)
    return false;

  PFN_RegCreateKeyExA create = (PFN_RegCreateKeyExA)GetProcAddress(mod, "RegCreateKeyExA");
  PFN_RegCloseKey close = (PFN_RegCloseKey)GetProcAddress(mod, "RegCloseKey");

  LSTATUS ret = ERROR_PROC_NOT_FOUND;

  if(create && close)
  {
    ret = create(HKEY_LOCAL_MACHINE, "SOFTWARE", 0, NULL, 0, KEY_READ | KEY_WRITE, NULL, &key, NULL);

    if(key)
      close(key);
  }

  FreeLibrary(mod);

  return (ret == ERROR_SUCCESS);

#else

  // this isn't ideal, we should check something else since a user may have permissions to do what
  // we want to do
  return geteuid() == 0;

#endif
}

bool RunProcessAsAdmin(const QString &fullExecutablePath, const QStringList &params,
                       QWidget *parent, bool hidden, std::function<void()> finishedCallback)
{
#if defined(Q_OS_WIN32)

  std::wstring wideExe = QDir::toNativeSeparators(fullExecutablePath).toStdWString();
  std::wstring wideParams;

  for(QString p : params)
  {
    wideParams += L"\"";
    wideParams += p.toStdWString();
    wideParams += L"\" ";
  }

  SHELLEXECUTEINFOW info = {};
  info.cbSize = sizeof(info);
  info.fMask = SEE_MASK_NOCLOSEPROCESS;
  info.lpVerb = L"runas";
  info.lpFile = wideExe.c_str();
  info.lpParameters = wideParams.c_str();
  info.nShow = hidden ? SW_HIDE : SW_SHOWNORMAL;

  ShellExecuteExW(&info);

  if((uintptr_t)info.hInstApp > 32 && info.hProcess != NULL)
  {
    if(finishedCallback)
    {
      HANDLE h = info.hProcess;

      // do the wait on another thread
      LambdaThread *thread = new LambdaThread([h, parent, finishedCallback]() {
        WaitForSingleObject(h, 30000);
        CloseHandle(h);
        GUIInvoke::call(parent, finishedCallback);
      });
      thread->selfDelete(true);
      thread->start();
    }
    else
    {
      CloseHandle(info.hProcess);
    }

    return true;
  }

  return false;

#else
  // try to find a way to run the application elevated.
  const QString graphicalSudo[] = {
      lit("pkexec"), lit("kdesudo"), lit("gksudo"), lit("beesu"),
  };

  // if none of the graphical options, then look for sudo and either
  const QString termEmulator[] = {
      lit("x-terminal-emulator"), lit("gnome-terminal"), lit("konsole"), lit("xterm"),
  };

  for(const QString &sudo : graphicalSudo)
  {
    QString inPath = QStandardPaths::findExecutable(sudo);

    // can't find in path
    if(inPath.isEmpty())
      continue;

    QProcess *process = new QProcess;

    QStringList sudoParams;
    // these programs need a -- to indicate the end of their options, before the program
    if(sudo == lit("kdesudo") || sudo == lit("gksudo"))
      sudoParams << lit("--");

    sudoParams << fullExecutablePath;
    for(const QString &p : params)
      sudoParams << p;

    qInfo() << "Running" << sudo << "with params" << sudoParams;

    // run with sudo
    process->start(sudo, sudoParams);

    // when the process exits, call the callback and delete
    QObject::connect(process, OverloadedSlot<int>::of(&QProcess::finished),
                     [parent, process, finishedCallback](int exitCode) {
                       process->deleteLater();
                       GUIInvoke::call(parent, finishedCallback);
                     });

    return true;
  }

  QString sudo = QStandardPaths::findExecutable(lit("sudo"));

  if(sudo.isEmpty())
  {
    qCritical() << "Couldn't find graphical or terminal sudo program!\n"
                << "Please run " << fullExecutablePath << "with args" << params << "manually.";
    return false;
  }

  for(const QString &term : termEmulator)
  {
    QString inPath = QStandardPaths::findExecutable(term);

    // can't find in path
    if(inPath.isEmpty())
      continue;

    QProcess *process = new QProcess;

    // run terminal sudo with emulator
    QStringList termParams;
    termParams
        << lit("-e")
        << lit("bash -c 'sudo %1 %2'").arg(fullExecutablePath).arg(params.join(QLatin1Char(' ')));

    process->start(term, termParams);

    // when the process exits, call the callback and delete
    QObject::connect(process, OverloadedSlot<int>::of(&QProcess::finished),
                     [parent, process, finishedCallback](int exitCode) {
                       process->deleteLater();
                       GUIInvoke::call(parent, finishedCallback);
                     });

    return true;
  }

  qCritical() << "Couldn't find graphical or terminal emulator to launch sudo.\n"
              << "Please run " << fullExecutablePath << "with args" << params << "manually.";

  return false;
#endif
}

void RevealFilenameInExternalFileBrowser(const QString &filePath)
{
#if defined(Q_OS_WIN32)
  // on windows we can ask explorer to highlight the exact file.
  QProcess::startDetached(lit("explorer.exe"), QStringList() << lit("/select,")
                                                             << QDir::toNativeSeparators(filePath));
#else
  // on all other platforms, we just use QDesktopServices to invoke the external file browser on the
  // directory and hope that's close enough.
  QDesktopServices::openUrl(QFileInfo(filePath).absoluteDir().absolutePath());
#endif
}

QStringList ParseArgsList(const QString &args)
{
  QStringList ret;

  if(args.isEmpty())
    return ret;

// on windows just use the function provided by the system
#if defined(Q_OS_WIN32)
  std::wstring wargs = args.toStdWString();

  int argc = 0;
  wchar_t **argv = CommandLineToArgvW(wargs.c_str(), &argc);

  for(int i = 0; i < argc; i++)
    ret << QString::fromWCharArray(argv[i]);

  LocalFree(argv);
#else
  std::string argString = args.toStdString();

  // perform some kind of sane parsing
  bool dquot = false, squot = false;    // are we inside ''s or ""s

  // current character
  char *c = &argString[0];

  // current argument we're building
  std::string a;

  while(*c)
  {
    if(!dquot && !squot && (*c == ' ' || *c == '\t'))
    {
      if(!a.empty())
        ret << QString::fromStdString(a);

      a = "";
    }
    else if(!dquot && *c == '"')
    {
      dquot = true;
    }
    else if(!squot && *c == '\'')
    {
      squot = true;
    }
    else if(dquot && *c == '"')
    {
      dquot = false;
    }
    else if(squot && *c == '\'')
    {
      squot = false;
    }
    else if(squot)
    {
      // single quotes don't escape, just copy literally until we leave single quote mode
      a.push_back(*c);
    }
    else if(dquot)
    {
      // handle escaping
      if(*c == '\\')
      {
        c++;
        if(*c)
        {
          a.push_back(*c);
        }
        else
        {
          qCritical() << "Malformed args list:" << args;
          return ret;
        }
      }
      else
      {
        a.push_back(*c);
      }
    }
    else
    {
      a.push_back(*c);
    }

    c++;
  }

  // if we were building an argument when we hit the end of the string
  if(!a.empty())
    ret << QString::fromStdString(a);
#endif

  return ret;
}

void ShowProgressDialog(QWidget *window, const QString &labelText, ProgressFinishedMethod finished,
                        ProgressUpdateMethod update)
{
  if(finished())
    return;

  RDProgressDialog dialog(labelText, window);

  // if we don't have an update function, set the progress display to be 'infinite spinner'
  dialog.setInfinite(!update);

  QSemaphore tickerSemaphore(1);

  // start a lambda thread to tick our functions and close the progress dialog when we're done.
  LambdaThread progressTickerThread([finished, update, &dialog, &tickerSemaphore]() {
    while(tickerSemaphore.available())
    {
      QThread::msleep(30);

      if(update)
        GUIInvoke::call(&dialog, [update, &dialog]() { dialog.setPercentage(update()); });

      GUIInvoke::call(&dialog, [finished, &tickerSemaphore]() {
        if(finished())
          tickerSemaphore.tryAcquire();
      });
    }

    GUIInvoke::call(&dialog, [&dialog]() { dialog.closeAndReset(); });
  });
  progressTickerThread.start();

  // show the dialog
  RDDialog::show(&dialog);

  // signal the thread to exit if somehow we got here without it finishing, then wait for it thread
  // to clean itself up
  tickerSemaphore.tryAcquire();
  progressTickerThread.wait();
}

void UpdateTransferProgress(qint64 xfer, qint64 total, QElapsedTimer *timer,
                            QProgressBar *progressBar, QLabel *progressLabel, QString progressText)
{
  if(xfer >= total)
  {
    progressBar->setMaximum(10000);
    progressBar->setValue(10000);
    return;
  }

  if(total <= 0)
  {
    progressBar->setMaximum(10000);
    progressBar->setValue(0);
    return;
  }

  progressBar->setMaximum(10000);
  progressBar->setValue(int(10000.0 * (double(xfer) / double(total))));

  double xferMB = double(xfer) / 1000000.0;
  double totalMB = double(total) / 1000000.0;

  double secondsElapsed = double(timer->nsecsElapsed()) * 1.0e-9;

  double speedMBS = xferMB / secondsElapsed;

  qulonglong secondsRemaining = qulonglong(double(totalMB - xferMB) / speedMBS);

  if(secondsElapsed > 1.0)
  {
    QString remainString;

    qulonglong minutesRemaining = (secondsRemaining / 60) % 60;
    qulonglong hoursRemaining = (secondsRemaining / 3600);
    secondsRemaining %= 60;

    if(hoursRemaining > 0)
      remainString = QFormatStr("%1:%2:%3")
                         .arg(hoursRemaining, 2, 10, QLatin1Char('0'))
                         .arg(minutesRemaining, 2, 10, QLatin1Char('0'))
                         .arg(secondsRemaining, 2, 10, QLatin1Char('0'));
    else if(minutesRemaining > 0)
      remainString = QFormatStr("%1:%2")
                         .arg(minutesRemaining, 2, 10, QLatin1Char('0'))
                         .arg(secondsRemaining, 2, 10, QLatin1Char('0'));
    else
      remainString = QApplication::translate("qrenderdoc", "%1 seconds").arg(secondsRemaining);

    double speed = speedMBS;

    bool MBs = true;
    if(speedMBS < 1)
    {
      MBs = false;
      speed *= 1000;
    }

    progressLabel->setText(
        QApplication::translate("qrenderdoc", "%1\n%2 MB / %3 MB. %4 remaining (%5 %6)")
            .arg(progressText)
            .arg(xferMB, 0, 'f', 2)
            .arg(totalMB, 0, 'f', 2)
            .arg(remainString)
            .arg(speed, 0, 'f', 2)
            .arg(MBs ? lit("MB/s") : lit("KB/s")));
  }
}

void setEnabledMultiple(const QList<QWidget *> &widgets, bool enabled)
{
  for(QWidget *w : widgets)
    w->setEnabled(enabled);
}

QString GetSystemUsername()
{
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

  QString username = env.value(lit("USER"));
  if(username == QString())
    username = env.value(lit("USERNAME"));
  if(username == QString())
    username = lit("Unknown_User");

  return username;
}

void BringToForeground(QWidget *window)
{
#ifdef Q_OS_WIN
  SetWindowPos((HWND)window->winId(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
  window->setWindowState(Qt::WindowActive);
  window->raise();
  window->showNormal();
  window->show();
  SetWindowPos((HWND)window->winId(), HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
#else
  window->activateWindow();
  window->raise();
  window->showNormal();
#endif
}

bool IsDarkTheme()
{
  float baseLum = getLuminance(QApplication::palette().color(QPalette::Base));
  float textLum = getLuminance(QApplication::palette().color(QPalette::Text));

  // if the base is dark than the text, then it's a light-on-dark theme (aka dark theme)
  return (baseLum < textLum);
}

float getLuminance(const QColor &col)
{
  return (float)(0.2126 * qPow(col.redF(), 2.2) + 0.7152 * qPow(col.greenF(), 2.2) +
                 0.0722 * qPow(col.blueF(), 2.2));
}

QColor contrastingColor(const QColor &col, const QColor &defaultCol)
{
  float backLum = getLuminance(col);
  float textLum = getLuminance(defaultCol);

  bool backDark = backLum < 0.2f;
  bool textDark = textLum < 0.2f;

  // if they're contrasting, use the text colour desired
  if(backDark != textDark)
    return defaultCol;

  // otherwise pick a contrasting colour
  if(backDark)
    return QColor(Qt::white);
  else
    return QColor(Qt::black);
}
