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

#include "QRDUtils.h"
#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QCloseEvent>
#include <QCollator>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QFileSystemModel>
#include <QFontDatabase>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHeaderView>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
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
#include <QTextBoundaryFinder>
#include <QTextDocument>
#include <QtMath>
#include "Code/Resources.h"
#include "Widgets/Extended/RDListWidget.h"
#include "Widgets/Extended/RDTreeWidget.h"

// normally this is in the renderdoc core library, but it's needed for the 'unknown enum' path,
// so we implement it here using QString. It's inefficient, but this is a very uncommon path -
// either for invalid values or for when a new enum is added and the code isn't updated
template <>
rdcstr DoStringise(const uint32_t &el)
{
  return QString::number(el);
}

// this could be needed for CHECKs in tests
template <>
rdcstr DoStringise(const uint16_t &el)
{
  return QString::number(el);
}

// these ones we do by hand as it requires formatting
template <>
rdcstr DoStringise(const ResourceId &el)
{
  uint64_t num;
  memcpy(&num, &el, sizeof(num));
  return lit("ResourceId::%1").arg(num);
}

QMap<QPair<ResourceId, uint32_t>, uint32_t> PointerTypeRegistry::typeMapping;
rdcarray<ShaderConstantType> PointerTypeRegistry::typeDescriptions;

static const uint32_t TypeIDBit = 0x80000000;

void PointerTypeRegistry::Init()
{
  typeMapping.clear();

  // type ID 0 is reserved as a NULL/empty descriptor
  typeDescriptions.resize(1);
  typeDescriptions[0].name = "<Unknown>";
}

uint32_t PointerTypeRegistry::GetTypeID(ResourceId shader, uint32_t pointerTypeId)
{
  if(pointerTypeId & TypeIDBit)
    shader = ResourceId();
  return typeMapping[qMakePair(shader, pointerTypeId)];
}

uint32_t PointerTypeRegistry::GetTypeID(const ShaderConstantType &structDef)
{
  // see if the type is already registered, return its existing ID
  for(uint32_t i = 1; i < typeDescriptions.size(); i++)
  {
    if(structDef == typeDescriptions[i])
      return TypeIDBit | i;
  }

  uint32_t id = TypeIDBit | (uint32_t)typeDescriptions.size();

  // otherwise register the new type
  typeDescriptions.push_back(structDef);
  typeMapping[qMakePair(ResourceId(), id)] = id;

  return id;
}

const ShaderConstantType &PointerTypeRegistry::GetTypeDescriptor(uint32_t typeId)
{
  return typeDescriptions[typeId & ~TypeIDBit];
}

void PointerTypeRegistry::CacheSubTypes(const ShaderReflection *reflection,
                                        ShaderConstantType &structDef)
{
  if((structDef.pointerTypeID & TypeIDBit) == 0)
    structDef.pointerTypeID =
        PointerTypeRegistry::GetTypeID(reflection->pointerTypes[structDef.pointerTypeID]);

  for(ShaderConstant &member : structDef.members)
    CacheSubTypes(reflection, member.type);
}

void PointerTypeRegistry::CacheShader(const ShaderReflection *reflection)
{
  // nothing to do if there are no pointer types
  if(reflection->pointerTypes.isEmpty())
    return;

  // check if we've already cached this shader (we know there's at least one pointer type)
  if(typeMapping.contains(qMakePair(reflection->resourceId, 0)))
    return;

  for(uint32_t i = 0; i < reflection->pointerTypes.size(); i++)
  {
    ShaderConstantType typeDesc = reflection->pointerTypes[i];

    // first recursively cache all subtypes needed by the root struct types
    CacheSubTypes(reflection, typeDesc);

    // then look up the Type ID for this struct
    typeMapping[qMakePair(reflection->resourceId, i)] = GetTypeID(typeDesc);
  }
}

template <>
rdcstr DoStringise(const PointerVal &el)
{
  if(el.pointerTypeID != ~0U)
  {
    uint32_t ptrTypeId = PointerTypeRegistry::GetTypeID(el.shader, el.pointerTypeID);

    return QFormatStr("GPUAddress::%1::%2").arg(el.pointer).arg(ptrTypeId);
  }
  else
  {
    return QFormatStr("GPUAddress::%1").arg(el.pointer);
  }
}

QString GetTruncatedResourceName(const ICaptureContext &ctx, ResourceId id)
{
  QString name = ctx.GetResourceName(id);
  if(name.length() > 64)
  {
    QTextBoundaryFinder boundaries(QTextBoundaryFinder::Grapheme, name.data(), name.length());
    boundaries.setPosition(64);
    if(!boundaries.isAtBoundary())
      boundaries.toPreviousBoundary();
    int pos = boundaries.position();
    name.resize(pos);
    name += lit("...");
  }

  return name;
}

struct ShaderMessageLink
{
  uint32_t eid;
  uint32_t numMessages;
};

Q_DECLARE_METATYPE(ShaderMessageLink);

static QRegularExpression htmlRE(lit("<[^>]*>"));

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
  int textCacheId = 0;

  // the ideal width for the document
  int idealWidth = 0;
  int numLines = 1;

  bool forcehtml = false;

  // cache the context once we've obtained it.
  const ICaptureContext *ctxptr = NULL;

  void cacheText(const QWidget *widget)
  {
    if(!ctxptr)
      ctxptr = getCaptureContext(widget);

    if(!ctxptr)
      return;

    cacheText(*ctxptr);
  }

  void cacheText(const ICaptureContext &ctx)
  {
    if(!ctxptr)
      ctxptr = &ctx;

    int refCache = ctx.ResourceNameCacheID();

    if(textCacheId == refCache)
      return;

    textCacheId = refCache;

    text.clear();

    for(const QVariant &v : fragments)
    {
      if(v.userType() == qMetaTypeId<ResourceId>())
      {
        QString resname = GetTruncatedResourceName(ctx, v.value<ResourceId>());
        text += resname;
      }
      else if(v.type() == QVariant::UInt)
      {
        text += lit("EID @%1").arg(v.toUInt());
      }
      else if(v.userType() == qMetaTypeId<ShaderMessageLink>())
      {
        ShaderMessageLink link = v.value<ShaderMessageLink>();

        text +=
            QApplication::translate("qrenderdoc", "%n msg(s)", "Shader messages", link.numMessages);
      }
      else
      {
        if(forcehtml)
        {
          // strip html tags and convert any html entities
          text += QUrl::fromPercentEncoding(v.toString().replace(htmlRE, QString()).toLatin1());
        }
        else
        {
          text += v.toString();
        }
      }
    }
  }

  void cacheDocument(const QWidget *widget)
  {
    if(!ctxptr)
      ctxptr = getCaptureContext(widget);

    if(!ctxptr)
      return;

    ICaptureContext &ctx = *(ICaptureContext *)ctxptr;

    int refCache = ctx.ResourceNameCacheID();

    if(cacheId == refCache && textCacheId == refCache)
      return;

    cacheId = refCache;
    textCacheId = refCache;

    // use a table to ensure images don't screw up the baseline for text. DON'T JUDGE ME.
    QString html = lit("<table><tr>");

    int i = 0;

    bool highdpi = widget && widget->devicePixelRatioF() > 1.0;

    QVector<int> fragmentIndexFromBlockIndex;

    // there's an empty block at the start.
    fragmentIndexFromBlockIndex.push_back(-1);

    text.clear();

    numLines = 1;

    for(const QVariant &v : fragments)
    {
      if(v.userType() == qMetaTypeId<ResourceId>())
      {
        QString resname = GetTruncatedResourceName(ctx, v.value<ResourceId>()).toHtmlEscaped();
        html += lit("<td valign=\"middle\" style=\"line-height: 14px\"><b>%1</b></td>"
                    "<td valign=\"middle\" style=\"line-height: 14px\">"
                    "<img width=\"16\" src=':/link%3.png'></td>")
                    .arg(resname)
                    .arg(highdpi ? lit("@2x") : QString());
        text += resname;

        // these generate two blocks (one for each cell)
        fragmentIndexFromBlockIndex.push_back(i);
        fragmentIndexFromBlockIndex.push_back(i);
      }
      else if(v.userType() == qMetaTypeId<ShaderMessageLink>())
      {
        ShaderMessageLink link = v.value<ShaderMessageLink>();

        QString msgstr =
            QApplication::translate("qrenderdoc", "%n msg(s)", "Shader messages", link.numMessages);

        html += lit("<td valign=\"middle\" style=\"line-height: 14px\">"
                    "<img width=\"16\" src=':/text_add%3.png'></td>"
                    "<td valign=\"middle\" style=\"line-height: 14px\">%1</td>")
                    .arg(msgstr)
                    .arg(highdpi ? lit("@2x") : QString());
        text += msgstr;

        fragmentIndexFromBlockIndex.push_back(i);
        fragmentIndexFromBlockIndex.push_back(i);
      }
      else if(v.type() == QVariant::UInt)
      {
        html += lit("<td valign=\"middle\" style=\"line-height: 14px\">"
                    "<font color='#0000FF'><u>EID @%1</u></font></td>")
                    .arg(v.toUInt());
        text += lit("EID @%1").arg(v.toUInt());

        fragmentIndexFromBlockIndex.push_back(i);
      }
      else
      {
        QString htmlfrag = v.toString();

        if(!forcehtml)
        {
          htmlfrag = htmlfrag.toHtmlEscaped();
          htmlfrag.replace(lit(" "), lit("&nbsp;"));
        }

        int newlines = htmlfrag.count(QLatin1Char('\n'));
        htmlfrag.replace(
            lit("\n"),
            lit("</td></tr></table><table><tr><td valign=\"middle\" style=\"line-height: 14px\">"));

        if(forcehtml)
        {
          text += QUrl::fromPercentEncoding(v.toString().replace(htmlRE, QString()).toLatin1());
        }
        else
        {
          text += v.toString();
        }

        html += lit("<td valign=\"middle\" style=\"line-height: 14px\">%1</td>").arg(htmlfrag);

        numLines += newlines;

        // this generates one block at least
        fragmentIndexFromBlockIndex.push_back(i);
        for(int l = 0; l < newlines; l++)
        {
          fragmentIndexFromBlockIndex.push_back(i);
          fragmentIndexFromBlockIndex.push_back(i);
        }
      }

      i++;
    }

    // there's another empty block at the end
    fragmentIndexFromBlockIndex.push_back(-1);

    html += lit("</tr></table>");

    doc.setDocumentMargin(0);
    doc.setHtml(html);
    if(widget)
      doc.setDefaultFont(widget->font());

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

void GPUAddress::cacheAddress(const QWidget *widget)
{
  if(!ctxptr)
    ctxptr = getCaptureContext(widget);

  // bail out if we don't have a context
  if(!ctxptr)
    return;

  cacheAddress(*ctxptr);
}

void GPUAddress::cacheAddress(const ICaptureContext &ctx)
{
  if(!ctxptr)
    ctxptr = &ctx;

  // bail if we're already cached
  if(base != ResourceId())
    return;

  // find the first matching buffer
  for(const BufferDescription &b : ctxptr->GetBuffers())
  {
    if(b.gpuAddress && b.gpuAddress <= val.pointer && b.gpuAddress + b.length > val.pointer)
    {
      base = b.resourceId;
      offset = val.pointer - b.gpuAddress;
      return;
    }
  }
}

// for the same reason as above we use a shared pointer for GPU addresses too. This ensures the
// cached data doesn't keep getting re-cached in copies.
typedef QSharedPointer<GPUAddress> GPUAddressPtr;

Q_DECLARE_METATYPE(GPUAddressPtr);

ICaptureContext *getCaptureContext(const QWidget *widget)
{
  void *ctxptr = NULL;

  while(widget && !ctxptr)
  {
    ctxptr = widget->property("ICaptureContext").value<void *>();
    widget = widget->parentWidget();
  }

  return (ICaptureContext *)ctxptr;
}

QString ResIdTextToString(RichResourceTextPtr ptr)
{
  ptr->cacheText(NULL);
  return ptr->text;
}

QString ResIdToString(ResourceId ptr)
{
  return ToQStr(ptr);
}

QString GPUAddressToString(GPUAddressPtr addr)
{
  if(addr->base != ResourceId())
    return QFormatStr("%1+%2").arg(ToQStr(addr->base)).arg(addr->offset);
  else
    return QFormatStr("0x%1").arg(addr->val.pointer, 0, 16);
}

QString EnumInterpValueToString(EnumInterpValue val)
{
  return val.str;
}

void RegisterMetatypeConversions()
{
  QMetaType::registerConverter<RichResourceTextPtr, QString>(&ResIdTextToString);
  QMetaType::registerConverter<ResourceId, QString>(&ResIdToString);
  QMetaType::registerConverter<GPUAddressPtr, QString>(&GPUAddressToString);
  QMetaType::registerConverter<EnumInterpValue, QString>(&EnumInterpValueToString);
}

bool HandleURLFragment(RichResourceTextPtr linkedText, QString text, bool parseURLs)
{
  bool hasURL = false;
  if(parseURLs)
  {
    static QRegularExpression urlRE(lit(
        R"(https?://([a-zA-Z0-9]+\.)?[-a-zA-Z0-9@:%._+~#=]{2,256}.([a-z]{2,8})+/[-a-zA-Z0-9@:%_+.~#?&/=]*)"));

    QRegularExpressionMatch urlMatch = urlRE.match(text);

    while(urlMatch.hasMatch())
    {
      QUrl url = urlMatch.captured(0);

      int end = urlMatch.capturedEnd();

      // push any text that preceeded the url.
      if(urlMatch.capturedStart(0) > 0)
        linkedText->fragments.push_back(text.left(urlMatch.capturedStart(0)));

      text.remove(0, end);

      linkedText->fragments.push_back(url);

      urlMatch = urlRE.match(text);

      hasURL = true;
    }
  }

  if(!text.isEmpty())
  {
    linkedText->fragments.push_back(text);
  }
  return hasURL;
}

void RichResourceTextInitialise(QVariant &var, ICaptureContext *ctx, bool parseURLs)
{
  // we only upconvert from strings, any other type with a string representation is not expected to
  // contain ResourceIds. In particular if the variant is already a ResourceId we can return.
  if(GetVariantMetatype(var) != QMetaType::QString)
    return;

  // we trim the string because that will happen naturally when rendering as HTML, and it makes it
  // easier to detect strings where the only contents are ResourceId text.
  QString text = var.toString().trimmed();

  // do a simple string search first before using regular expressions
  if(!text.contains(lit("ResourceId::")) && !text.contains(lit("GPUAddress::")) &&
     !text.contains(lit("__rd_msgs::")) && !text.contains(QLatin1Char('@')) && !parseURLs &&
     !(text.startsWith(lit("<rdhtml>")) && text.endsWith(lit("</rdhtml>"))))
    return;

  // two forms: GPUAddress::012345        - typeless
  //            GPUAddress::012345::991   - using type 991 from PointerTypeRegistry
  static QRegularExpression addrRE(lit("GPUAddress::([0-9]*)(::([0-9]*))?"));

  QRegularExpressionMatch match = addrRE.match(text);

  if(match.hasMatch())
  {
    // don't support mixed text & addresses. Only do the replacement if we matched the whole string
    if(match.capturedStart(0) == 0 && match.capturedLength(0) == text.length())
    {
      GPUAddressPtr addr(new GPUAddress);
      addr->val.pointer = match.captured(1).toULongLong();

      // we deliberately set this to ResourceId() to indicate that we're using an ID from the
      // registry, not a shader-relative index
      addr->val.shader = ResourceId();
      addr->val.pointerTypeID = match.captured(3).toULong();

      var = QVariant::fromValue(addr);
      return;
    }

    return;
  }

  const bool forcehtml = text.startsWith(lit("<rdhtml>")) && text.endsWith(lit("</rdhtml>"));
  if(forcehtml)
  {
    text = text.mid(8, text.length() - 17);
  }

  // use regexp to split up into fragments of text and resourceid. The resourceid is then
  // formatted on the fly in RichResourceText::cacheDocument
  static QRegularExpression resRE(
      lit("(ResourceId::)([0-9]+)|(@)([0-9]+)|(__rd_msgs::)([0-9]+):([0-9]+)"));

  match = resRE.match(text);

  RichResourceTextPtr linkedText(new RichResourceText);

  linkedText->ctxptr = ctx;
  linkedText->forcehtml = forcehtml;

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

    while(match.hasMatch())
    {
      ResourceId id;
      uint32_t eid = 0;
      if(match.captured(1) == lit("ResourceId::"))
      {
        qulonglong idnum = match.captured(2).toULongLong();
        memcpy(&id, &idnum, sizeof(id));

        // push any text that preceeded the ResourceId.
        if(match.capturedStart(1) > 0)
          HandleURLFragment(linkedText, text.left(match.capturedStart(1)), parseURLs);

        text.remove(0, match.capturedEnd(2));

        linkedText->fragments.push_back(id);
      }
      else if(match.captured(5) == lit("__rd_msgs::"))
      {
        ShaderMessageLink link;
        link.eid = match.captured(6).toUInt();
        link.numMessages = match.captured(7).toUInt();

        // push any text that preceeded the msgs link.
        if(match.capturedStart(5) > 0)
          HandleURLFragment(linkedText, text.left(match.capturedStart(5)), parseURLs);

        text.remove(0, match.capturedEnd(7));

        linkedText->fragments.push_back(QVariant::fromValue(link));
      }
      else
      {
        eid = match.captured(4).toUInt();

        int end = match.capturedEnd(4);

        // skip @..x since e.g. @2x appears in high-DPI icons and @0x08732 can appear in shader name
        if(end < text.length() && text[end] == QLatin1Char('x'))
        {
          match = resRE.match(text, end);
          continue;
        }

        int start = match.capturedStart(3);

        // skip matches with an identifier character before the @, like foo@4
        if(start > 0 && (text[start - 1].isLetterOrNumber() || text[start - 1] == QLatin1Char('_')))
        {
          match = resRE.match(text, end);
          continue;
        }

        // push any text that preceeded the EID.
        if(match.capturedStart(3) > 0)
          HandleURLFragment(linkedText, text.left(match.capturedStart(3)), parseURLs);

        text.remove(0, end);

        linkedText->fragments.push_back(eid);
      }

      match = resRE.match(text);
    }

    if(!text.isEmpty())
    {
      // if we didn't get any fragments that means we only encountered false positive matches e.g.
      // @2x. Return the normal text as non-richresourcetext unless we're forcing it
      if(linkedText->fragments.empty() && !forcehtml)
        return;

      HandleURLFragment(linkedText, text, parseURLs);
    }

    var = QVariant::fromValue(linkedText);
  }
  else if(forcehtml)
  {
    linkedText->fragments.push_back(text);

    var = QVariant::fromValue(linkedText);
    return;
  }
  else
  {
    // if our text doesn't match any of the previous pattern, we still want to linkify URLs
    if(HandleURLFragment(linkedText, text, parseURLs))
    {
      var = QVariant::fromValue(linkedText);
    }
  }
}

bool RichResourceTextCheck(const QVariant &var)
{
  return var.userType() == qMetaTypeId<RichResourceTextPtr>() ||
         var.userType() == qMetaTypeId<GPUAddressPtr>() ||
         var.userType() == qMetaTypeId<ResourceId>() || var.userType() == qMetaTypeId<QUrl>();
}

// I'm not sure if this should come from the style or not - the QTextDocument handles this in
// the rich text case.
static const int RichResourceTextMargin = 2;

void RichResourceTextPaint(const QWidget *owner, QPainter *painter, QRect rect, QFont font,
                           QPalette palette, QStyle::State state, QPoint mousePos,
                           const QVariant &var)
{
  QBrush foreBrush = palette.brush(state & QStyle::State_Selected ? QPalette::HighlightedText
                                                                  : QPalette::WindowText);

  painter->save();

  // special case handling for ResourceId/GPUAddress on its own
  if(var.userType() == qMetaTypeId<ResourceId>() || var.userType() == qMetaTypeId<GPUAddressPtr>())
  {
    QFont f = painter->font();
    f.setBold(true);
    painter->setFont(f);

    static const int margin = RichResourceTextMargin;

    rect.adjust(margin, 0, -margin * 2, 0);

    QString name;

    bool valid = false;

    if(var.userType() == qMetaTypeId<ResourceId>())
    {
      ICaptureContext *ctxptr = getCaptureContext(owner);

      ResourceId id = var.value<ResourceId>();

      valid = (id != ResourceId());

      if(ctxptr)
        name = GetTruncatedResourceName(*ctxptr, id);
      else
        name = ToQStr(id);
    }
    else
    {
      GPUAddressPtr ptr = var.value<GPUAddressPtr>();

      ptr->cacheAddress(owner);

      valid = (ptr->val.pointer != 0);

      if(valid)
      {
        if(ptr->base != ResourceId())
        {
          name =
              QFormatStr("%1+%2").arg(GetTruncatedResourceName(*ptr->ctxptr, ptr->base)).arg(ptr->offset);
        }
        else
        {
          name = QFormatStr("Unknown 0x%1").arg(ptr->val.pointer, 16, 16, QLatin1Char('0'));
          valid = false;
        }
      }
      else
      {
        name = lit("NULL");
      }
    }

    painter->setPen(foreBrush.color());
    painter->drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, name);

    QRect textRect =
        painter->fontMetrics().boundingRect(rect, Qt::AlignLeft | Qt::AlignVCenter, name);

    const QPixmap &px = Pixmaps::link(owner->devicePixelRatio());

    painter->setClipRect(rect);

    textRect.setLeft(rect.left());
    textRect.setWidth(textRect.width() + margin + px.width());
    textRect.setHeight(qMax(textRect.height(), px.height()));

    QPoint pos;
    pos.setX(textRect.right() - px.width() + 1);
    pos.setY(textRect.center().y() - px.height() / 2);

    painter->drawPixmap(pos, px, px.rect());

    if((state & QStyle::State_MouseOver) && textRect.contains(mousePos) && valid)
    {
      int underline_y = textRect.bottom() - margin;

      painter->setPen(QPen(foreBrush, 1.0));
      painter->drawLine(QPoint(textRect.left(), underline_y), QPoint(textRect.right(), underline_y));
    }

    painter->restore();

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
    painter->translate(1, diff / 2);
  else
    painter->translate(1, 0);

  QAbstractTextDocumentLayout::PaintContext docCtx;
  docCtx.palette = palette;

  docCtx.clip = QRectF(0, 0, rect.width() - 1, rect.height());

  if(state & QStyle::State_Selected)
  {
    QAbstractTextDocumentLayout::Selection sel;
    sel.format.setForeground(foreBrush.color());
    sel.cursor = QTextCursor(&linkedText->doc);
    sel.cursor.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
    sel.cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    docCtx.selections.push_back(sel);
  }
  else
  {
    docCtx.palette.setColor(QPalette::Text, foreBrush.color());
  }

  painter->setClipRect(docCtx.clip);

  linkedText->doc.documentLayout()->draw(painter, docCtx);

  if(state & QStyle::State_MouseOver)
  {
    painter->setPen(QPen(foreBrush, 1.0));

    QAbstractTextDocumentLayout *layout = linkedText->doc.documentLayout();

    QPoint p = mousePos - rect.topLeft();
    if(diff > 0)
      p -= QPoint(1, diff / 2);
    else
      p -= QPoint(1, 0);

    int pos = layout->hitTest(p, Qt::FuzzyHit);

    if(pos >= 0)
    {
      QTextBlock block = linkedText->doc.findBlock(pos);

      int frag = block.userState();
      if(frag >= 0)
      {
        QVariant v = linkedText->fragments[frag];
        if((v.userType() == qMetaTypeId<ResourceId>() && v.value<ResourceId>() != ResourceId()) ||
           v.userType() == qMetaTypeId<ShaderMessageLink>() || v.userType() == qMetaTypeId<QUrl>())
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

          if(v.userType() != qMetaTypeId<QUrl>())
          {
            blockrect.translate(0.0, -2.0);
          }

          blockrect.setRight(qMin(blockrect.right(), (qreal)rect.width()));

          painter->drawLine(blockrect.bottomLeft(), blockrect.bottomRight());
        }
      }
    }
  }

  painter->restore();
}

int RichResourceTextWidthHint(const QWidget *owner, const QFont &font, const QVariant &var)
{
  // special case handling for ResourceId/GPUAddress on its own
  if(var.userType() == qMetaTypeId<ResourceId>() || var.userType() == qMetaTypeId<GPUAddressPtr>())
  {
    QFont f = font;
    f.setBold(true);

    static const int margin = RichResourceTextMargin;

    QFontMetrics metrics(f);

    QString name;

    if(var.userType() == qMetaTypeId<ResourceId>())
    {
      ICaptureContext *ctxptr = getCaptureContext(owner);

      ResourceId id = var.value<ResourceId>();

      if(ctxptr)
        name = GetTruncatedResourceName(*ctxptr, id);
      else
        name = ToQStr(id);
    }
    else
    {
      GPUAddressPtr ptr = var.value<GPUAddressPtr>();

      ptr->cacheAddress(owner);

      if(ptr->val.pointer != 0)
        name =
            QFormatStr("%1+%2").arg(GetTruncatedResourceName(*ptr->ctxptr, ptr->base)).arg(ptr->offset);
      else
        name = lit("NULL");
    }

    const QPixmap &px = Pixmaps::link(owner->devicePixelRatio());

    int ret = margin + metrics.boundingRect(name).width() + margin + px.width() + margin;
    return ret;
  }

  RichResourceTextPtr linkedText = var.value<RichResourceTextPtr>();

  linkedText->cacheDocument(owner);

  return linkedText->idealWidth;
}

int RichResourceTextHeightHint(const QWidget *owner, const QFont &font, const QVariant &var)
{
  QFontMetrics metrics(font);

  if(var.userType() == qMetaTypeId<RichResourceTextPtr>())
  {
    RichResourceTextPtr linkedText = var.value<RichResourceTextPtr>();

    static const int margin = RichResourceTextMargin;

    linkedText->cacheDocument(owner);

    return linkedText->numLines * (metrics.lineSpacing() + margin * 2);
  }

  return metrics.height();
}

bool RichResourceTextMouseEvent(const QWidget *owner, const QVariant &var, QRect rect,
                                const QFont &font, QMouseEvent *event)
{
  // only process clicks or moves
  if(event->type() != QEvent::MouseButtonRelease && event->type() != QEvent::MouseMove)
    return false;

  // only process left button clicks
  if(event->type() == QEvent::MouseButtonRelease && event->button() != Qt::LeftButton)
    return false;

  // special case handling for ResourceId/GPUAddress on its own
  if(var.userType() == qMetaTypeId<ResourceId>() || var.userType() == qMetaTypeId<GPUAddressPtr>())
  {
    ResourceId id;
    GPUAddressPtr ptr;
    const ICaptureContext *ctxptr = NULL;

    if(var.userType() == qMetaTypeId<ResourceId>())
    {
      id = var.value<ResourceId>();

      // empty resource ids are not clickable or hover-highlighted.
      if(id == ResourceId())
        return false;
    }

    if(var.userType() == qMetaTypeId<GPUAddressPtr>())
    {
      ptr = var.value<GPUAddressPtr>();

      ptr->cacheAddress(owner);

      // NULL or unknown addresses also are not clickable
      if(ptr->val.pointer == 0 || ptr->base == ResourceId())
        return false;
    }

    QFont f = font;
    f.setBold(true);

    static const int margin = RichResourceTextMargin;

    rect.adjust(margin, 0, -margin * 2, 0);

    QString name;

    if(var.userType() == qMetaTypeId<ResourceId>())
    {
      ctxptr = getCaptureContext(owner);

      if(ctxptr)
        name = GetTruncatedResourceName(*ctxptr, id);
      else
        name = ToQStr(id);
    }
    else
    {
      ctxptr = ptr->ctxptr;

      name =
          QFormatStr("%1+%2").arg(GetTruncatedResourceName(*ptr->ctxptr, ptr->base)).arg(ptr->offset);
    }

    QRect textRect = QFontMetrics(f).boundingRect(rect, Qt::AlignLeft | Qt::AlignVCenter, name);

    const QPixmap &px = Pixmaps::link(owner->devicePixelRatio());

    rect.setTop(textRect.top());
    rect.setWidth(textRect.width() + margin + px.width());
    rect.setHeight(qMax(textRect.height(), px.height()));

    if(rect.contains(event->pos()))
    {
      if(var.userType() == qMetaTypeId<ResourceId>())
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
      else if(var.userType() == qMetaTypeId<GPUAddressPtr>())
      {
        if(event->type() == QEvent::MouseButtonRelease && ctxptr)
        {
          ICaptureContext &ctx = *(ICaptureContext *)ctxptr;

          const ShaderConstantType &ptrType = PointerTypeRegistry::GetTypeDescriptor(ptr->val);

          QString formatter;

          if(!ptrType.members.isEmpty())
            formatter = BufferFormatter::DeclareStruct(
                BufferFormatter::EstimatePackingRules(ResourceId(), ptrType.members), ResourceId(),
                ptrType.name, ptrType.members, ptrType.arrayByteStride);

          IBufferViewer *view = ctx.ViewBuffer(ptr->offset, ~0ULL, ptr->base, formatter);

          ctx.AddDockWindow(view->Widget(), DockReference::MainToolArea, NULL);
        }

        return true;
      }
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
    p -= QPoint(1, diff / 2);
  else
    p -= QPoint(1, 0);

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
      else if(v.userType() == qMetaTypeId<ShaderMessageLink>())
      {
        ShaderMessageLink link = v.value<ShaderMessageLink>();

        if(event->type() == QEvent::MouseButtonRelease && linkedText->ctxptr)
        {
          ICaptureContext &ctx = *(ICaptureContext *)linkedText->ctxptr;

          ctx.SetEventID({}, link.eid, link.eid, false);

          IShaderMessageViewer *shad = ctx.ViewShaderMessages(ShaderStageMask::All);

          ctx.AddDockWindow(shad->Widget(), DockReference::MainToolArea, NULL);
        }

        return true;
      }
      else if(v.type() == QVariant::UInt)
      {
        uint32_t eid = v.value<uint32_t>();

        if(event->type() == QEvent::MouseButtonRelease && linkedText->ctxptr)
        {
          ICaptureContext &ctx = *(ICaptureContext *)linkedText->ctxptr;

          ctx.SetEventID({}, eid, eid, false);
        }

        return true;
      }
      else if(v.type() == QVariant::Url)
      {
        QUrl url = v.value<QUrl>();

        if(event->type() == QEvent::MouseButtonRelease)
        {
          QDesktopServices::openUrl(url);
        }

        return true;
      }
    }
  }

  return false;
}

QString RichResourceTextFormat(ICaptureContext &ctx, QVariant var)
{
  RichResourceTextInitialise(var, &ctx);
  if(var.userType() == qMetaTypeId<ResourceId>())
    return GetTruncatedResourceName(ctx, var.value<ResourceId>());

  if(var.userType() == qMetaTypeId<GPUAddressPtr>())
  {
    var.value<GPUAddressPtr>()->cacheAddress(ctx);

    // a bit hacky, but recurse to pick up the 'name' of the pointer base buffer
    return RichResourceTextFormat(ctx, var.toString());
  }

  if(var.userType() == qMetaTypeId<RichResourceTextPtr>())
    var.value<RichResourceTextPtr>()->cacheText(ctx);

  // either it's something else and wasn't rich resource, in which case just return the string
  // representation, or it's a fully formatted rich resource document, where the cached text will do
  // the trick with ResIdTextToString.
  return var.toString();
}

FullEditorDelegate::FullEditorDelegate(QWidget *parent) : QStyledItemDelegate(parent)
{
}

QWidget *FullEditorDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                                          const QModelIndex &index) const
{
  return new QLineEdit(parent);
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

      QRect rect = opt.rect;
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

      RichResourceTextPaint(m_widget, painter, rect, opt.font, opt.palette, opt.state,
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
      return QSize(
          RichResourceTextWidthHint(m_widget, option.font, v),
          qMax(RichResourceTextHeightHint(m_widget, option.font, v), option.fontMetrics.height()));
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

ButtonDelegate::ButtonDelegate(const QIcon &icon, QString text, QWidget *parent)
    : m_Icon(icon), m_Text(text), QStyledItemDelegate(parent)
{
}

void ButtonDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                           const QModelIndex &index) const
{
  if(m_VisibleRole != -1 && index.data(m_VisibleRole) != m_VisibleValue)
    return QStyledItemDelegate::paint(painter, option, index);

  // draw the background to get selection etc
  QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &option, painter);

  QStyleOptionButton button;

  button.text = m_Text;
  button.icon = m_Icon;

  if(m_EnableRole == -1 || index.data(m_EnableRole) == m_EnableValue)
    button.state = QStyle::State_Enabled;

  if(m_ClickedIndex == index)
    button.state |= QStyle::State_Sunken;

  QSize sz =
      QApplication::style()->sizeFromContents(QStyle::CT_PushButton, &button, option.decorationSize);

  button.rect = getButtonRect(option.rect, sz);
  button.iconSize = option.decorationSize;

  QApplication::style()->drawControl(QStyle::CE_PushButton, &button, painter);
}

QSize ButtonDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
  if(m_VisibleRole != -1 && index.data(m_VisibleRole) != m_VisibleValue)
    return QStyledItemDelegate::sizeHint(option, index);

  QStyleOptionButton button;
  button.text = m_Text;
  button.icon = m_Icon;
  button.state = QStyle::State_Enabled;

  return QApplication::style()->sizeFromContents(QStyle::CT_PushButton, &button,
                                                 option.decorationSize);
}

bool ButtonDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
                                 const QStyleOptionViewItem &option, const QModelIndex &index)
{
  if(m_VisibleRole != -1 && index.data(m_VisibleRole) != m_VisibleValue)
    return QStyledItemDelegate::editorEvent(event, model, option, index);

  if(event->type() == QEvent::MouseButtonPress)
  {
    QMouseEvent *e = (QMouseEvent *)event;

    QPoint p = e->pos();

    QSize sz = sizeHint(option, index);
    QRect rect = getButtonRect(option.rect, sz);

    if(rect.contains(p) && (m_EnableRole == -1 || index.data(m_EnableRole) == m_EnableValue))
    {
      m_ClickedIndex = index;
      return true;
    }
  }
  else if(event->type() == QEvent::MouseMove)
  {
    QMouseEvent *e = (QMouseEvent *)event;

    if(m_ClickedIndex != index || (e->buttons() & Qt::LeftButton) == 0)
    {
      m_ClickedIndex = QModelIndex();
    }
    else
    {
      QPoint p = e->pos();

      QSize sz = sizeHint(option, index);
      QRect rect = getButtonRect(option.rect, sz);

      if(!rect.contains(p))
      {
        m_ClickedIndex = QModelIndex();
      }
    }
  }
  else if(event->type() == QEvent::MouseButtonRelease)
  {
    if(m_ClickedIndex == index && index != QModelIndex())
    {
      m_ClickedIndex = QModelIndex();

      QMouseEvent *e = (QMouseEvent *)event;

      QPoint p = e->pos();

      QSize sz = sizeHint(option, index);
      QRect rect = getButtonRect(option.rect, sz);

      if(rect.contains(p))
      {
        emit messageClicked(index);
        return true;
      }
    }
  }

  return QStyledItemDelegate::editorEvent(event, model, option, index);
}

QRect ButtonDelegate::getButtonRect(const QRect boundsRect, const QSize sz) const
{
  QRect rect = boundsRect;
  rect.setWidth(qMin(rect.width(), sz.width()));
  rect.setHeight(qMin(rect.height(), sz.height()));
  if(m_Centered)
    rect.moveLeft(rect.center().x() - rect.width() / 2);
  rect.moveTop(rect.center().y() - rect.height() / 2);
  // clip if the rounding from centering caused us to go out of bounds
  rect.setTop(qMax(rect.top(), boundsRect.top()));
  rect.setLeft(qMax(rect.left(), boundsRect.left()));
  return rect;
}

#include "renderdoc_tostr.inl"

QString ToQStr(const ResourceUsage usage, const GraphicsAPI apitype)
{
  if(IsD3D(apitype))
  {
    switch(usage)
    {
      case ResourceUsage::Unused: return lit("Unused");

      case ResourceUsage::VertexBuffer: return lit("Vertex Buffer");
      case ResourceUsage::IndexBuffer: return lit("Index Buffer");

      case ResourceUsage::VS_Constants: return lit("VS - Constant Buffer");
      case ResourceUsage::GS_Constants: return lit("GS - Constant Buffer");
      case ResourceUsage::HS_Constants: return lit("HS - Constant Buffer");
      case ResourceUsage::DS_Constants: return lit("DS - Constant Buffer");
      case ResourceUsage::PS_Constants: return lit("PS - Constant Buffer");
      case ResourceUsage::CS_Constants: return lit("CS - Constant Buffer");
      case ResourceUsage::TS_Constants: return lit("AS - Constant Buffer");
      case ResourceUsage::MS_Constants: return lit("MS - Constant Buffer");
      case ResourceUsage::All_Constants: return lit("All - Constant Buffer");

      case ResourceUsage::StreamOut: return lit("Stream Out");

      case ResourceUsage::VS_Resource: return lit("VS - Resource");
      case ResourceUsage::GS_Resource: return lit("GS - Resource");
      case ResourceUsage::HS_Resource: return lit("HS - Resource");
      case ResourceUsage::DS_Resource: return lit("DS - Resource");
      case ResourceUsage::PS_Resource: return lit("PS - Resource");
      case ResourceUsage::CS_Resource: return lit("CS - Resource");
      case ResourceUsage::TS_Resource: return lit("AS - Resource");
      case ResourceUsage::MS_Resource: return lit("MS - Resource");
      case ResourceUsage::All_Resource: return lit("All - Resource");

      case ResourceUsage::VS_RWResource: return lit("VS - UAV");
      case ResourceUsage::HS_RWResource: return lit("HS - UAV");
      case ResourceUsage::DS_RWResource: return lit("DS - UAV");
      case ResourceUsage::GS_RWResource: return lit("GS - UAV");
      case ResourceUsage::PS_RWResource: return lit("PS - UAV");
      case ResourceUsage::CS_RWResource: return lit("CS - UAV");
      case ResourceUsage::TS_RWResource: return lit("AS - UAV");
      case ResourceUsage::MS_RWResource: return lit("MS - UAV");
      case ResourceUsage::All_RWResource: return lit("All - UAV");

      case ResourceUsage::InputTarget: return lit("Color Input");
      case ResourceUsage::ColorTarget: return lit("Rendertarget");
      case ResourceUsage::DepthStencilTarget: return lit("Depthstencil");

      case ResourceUsage::Indirect: return lit("Indirect argument");

      case ResourceUsage::Clear: return lit("Clear");
      case ResourceUsage::Discard: return lit("Discard");

      case ResourceUsage::GenMips: return lit("Generate Mips");
      case ResourceUsage::Resolve: return lit("Resolve");
      case ResourceUsage::ResolveSrc: return lit("Resolve - Source");
      case ResourceUsage::ResolveDst: return lit("Resolve - Dest");
      case ResourceUsage::Copy: return lit("Copy");
      case ResourceUsage::CopySrc: return lit("Copy - Source");
      case ResourceUsage::CopyDst: return lit("Copy - Dest");

      case ResourceUsage::Barrier: return lit("Barrier");

      case ResourceUsage::CPUWrite: return lit("CPU Write");
    }
  }
  else if(apitype == GraphicsAPI::OpenGL || apitype == GraphicsAPI::Vulkan)
  {
    const bool vk = (apitype == GraphicsAPI::Vulkan);

    switch(usage)
    {
      case ResourceUsage::Unused: return lit("Unused");

      case ResourceUsage::VertexBuffer: return lit("Vertex Buffer");
      case ResourceUsage::IndexBuffer: return lit("Index Buffer");

      case ResourceUsage::VS_Constants: return lit("VS - Uniform Buffer");
      case ResourceUsage::GS_Constants: return lit("GS - Uniform Buffer");
      case ResourceUsage::HS_Constants: return lit("TCS - Uniform Buffer");
      case ResourceUsage::DS_Constants: return lit("TES - Uniform Buffer");
      case ResourceUsage::PS_Constants: return lit("FS - Uniform Buffer");
      case ResourceUsage::CS_Constants: return lit("CS - Uniform Buffer");
      case ResourceUsage::TS_Constants: return lit("TS - Uniform Buffer");
      case ResourceUsage::MS_Constants: return lit("MS - Uniform Buffer");
      case ResourceUsage::All_Constants: return lit("All - Uniform Buffer");

      case ResourceUsage::StreamOut: return lit("Transform Feedback");

      case ResourceUsage::VS_Resource: return lit("VS - Texture");
      case ResourceUsage::GS_Resource: return lit("GS - Texture");
      case ResourceUsage::HS_Resource: return lit("TCS - Texture");
      case ResourceUsage::DS_Resource: return lit("TES - Texture");
      case ResourceUsage::PS_Resource: return lit("FS - Texture");
      case ResourceUsage::CS_Resource: return lit("CS - Texture");
      case ResourceUsage::TS_Resource: return lit("TS - Texture");
      case ResourceUsage::MS_Resource: return lit("MS - Texture");
      case ResourceUsage::All_Resource: return lit("All - Texture");

      case ResourceUsage::VS_RWResource: return lit("VS - Image/SSBO");
      case ResourceUsage::HS_RWResource: return lit("HS - Image/SSBO");
      case ResourceUsage::DS_RWResource: return lit("TCS - Image/SSBO");
      case ResourceUsage::GS_RWResource: return lit("TES - Image/SSBO");
      case ResourceUsage::PS_RWResource: return lit("FS - Image/SSBO");
      case ResourceUsage::CS_RWResource: return lit("CS - Image/SSBO");
      case ResourceUsage::TS_RWResource: return lit("TS - Image/SSBO");
      case ResourceUsage::MS_RWResource: return lit("MS - Image/SSBO");
      case ResourceUsage::All_RWResource: return lit("All - Image/SSBO");

      case ResourceUsage::InputTarget: return lit("FB Input");
      case ResourceUsage::ColorTarget: return lit("FB Color");
      case ResourceUsage::DepthStencilTarget: return lit("FB Depthstencil");

      case ResourceUsage::Indirect: return lit("Indirect argument");

      case ResourceUsage::Clear: return lit("Clear");
      case ResourceUsage::Discard: return lit("Discard");

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

      case ResourceUsage::CPUWrite: return lit("CPU Write");
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
      case ShaderStage::Amplification: return lit("Amplif.");
      case ShaderStage::Mesh: return lit("Mesh");
      case ShaderStage::RayGen: return lit("RayGen");
      case ShaderStage::Intersection: return lit("Intersection");
      case ShaderStage::AnyHit: return lit("AnyHit");
      case ShaderStage::ClosestHit: return lit("ClosestHit");
      case ShaderStage::Miss: return lit("Miss");
      case ShaderStage::Callable: return lit("Callable");
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
      case ShaderStage::Task: return lit("Task");
      case ShaderStage::Mesh: return lit("Mesh");
      case ShaderStage::RayGen: return lit("RayGen");
      case ShaderStage::Intersection: return lit("Intersection");
      case ShaderStage::AnyHit: return lit("AnyHit");
      case ShaderStage::ClosestHit: return lit("ClosestHit");
      case ShaderStage::Miss: return lit("Miss");
      case ShaderStage::Callable: return lit("Callable");
      default: break;
    }
  }

  return lit("Unknown");
}

QString ToQStr(const AddressMode addr, const GraphicsAPI apitype)
{
  if(IsD3D(apitype))
  {
    switch(addr)
    {
      case AddressMode::Wrap: return lit("Wrap");
      case AddressMode::Mirror: return lit("Mirror");
      case AddressMode::MirrorOnce: return lit("MirrorOnce");
      case AddressMode::ClampEdge: return lit("ClampEdge");
      case AddressMode::ClampBorder: return lit("ClampBorder");
      default: break;
    }
  }
  else
  {
    switch(addr)
    {
      case AddressMode::Repeat: return lit("Repeat");
      case AddressMode::MirrorRepeat: return lit("MirrorRepeat");
      case AddressMode::MirrorClamp: return lit("MirrorClamp");
      case AddressMode::ClampEdge: return lit("ClampEdge");
      case AddressMode::ClampBorder: return lit("ClampBorder");
      default: break;
    }
  }

  return lit("Unknown");
}

QString ToQStr(const ShadingRateCombiner addr, const GraphicsAPI apitype)
{
  if(IsD3D(apitype))
  {
    switch(addr)
    {
      case ShadingRateCombiner::Keep: return lit("Passthrough");
      case ShadingRateCombiner::Replace: return lit("Override");
      default: break;
    }
  }

  return ToQStr(addr);
}

QString TypeString(const SigParameter &sig)
{
  QString ret = ToQStr(sig.varType);

  if(sig.compCount > 1)
    ret += QString::number(sig.compCount);

  return ret;
}

QString D3DSemanticString(const SigParameter &sig)
{
  if(sig.systemValue == ShaderBuiltin::Undefined)
    return sig.semanticIdxName;

  QString sysValues[] = {
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
      lit("Unsupported (GroupSize)"),
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
      lit("Unsupported (BaseVertex)"),
      lit("Unsupported (BaseInstance)"),
      lit("Unsupported (DrawIndex)"),
      lit("Unsupported (StencilReference)"),
      lit("Unsupported (PointCoord)"),
      lit("Unsupported (IsHelper)"),
      lit("Unsupported (SubgroupSize)"),
      lit("Unsupported (NumSubgroups)"),
      lit("Unsupported (SubgroupIndexInWorkgroup)"),
      lit("Unsupported (IndexInSubgroup)"),
      lit("Unsupported (SubgroupEqualMask)"),
      lit("Unsupported (SubgroupGreaterEqualMask)"),
      lit("Unsupported (SubgroupGreaterMask)"),
      lit("Unsupported (SubgroupLessEqualMask)"),
      lit("Unsupported (SubgroupLessMask)"),
      lit("Unsupported (DeviceIndex)"),
      lit("SV_InnerCoverage"),
      lit("Unsupported (FragAreaSize)"),
      lit("Unsupported (FragInvocationCount)"),
      lit("SV_ShadingRate"),
      lit("SV_Barycentrics"),
      lit("SV_CullPrimitive"),
      lit("out indices"),
      lit("SV_ViewID"),
  };

  static_assert(arraydim<ShaderBuiltin>() == ARRAY_COUNT(sysValues),
                "System values have changed - update HLSL stub generation");

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

  for(const EventUsage &u : usage)
  {
    if(start == 0)
    {
      start = end = u.eventId;
      us = u.usage;
    }

    if(u.usage == us && u.eventId == end)
      continue;

    const ActionDescription *action = ctx.GetAction(u.eventId);

    bool distinct = false;

    // if the usage is different from the last, add a new entry,
    // or if the previous action link is broken.
    if(u.usage != us || action == NULL || action->previous == 0)
    {
      distinct = true;
    }
    else
    {
      // otherwise search back through real actions, to see if the
      // last event was where we were - otherwise it's a new
      // distinct set of actions and should have a separate
      // entry in the context menu
      const ActionDescription *prev = action->previous;

      while(prev != NULL && prev->eventId > end)
      {
        if(!(prev->flags & (ActionFlags::Dispatch | ActionFlags::MeshDispatch |
                            ActionFlags::Drawcall | ActionFlags::CmdList)))
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
      if(end == u.eventId && us == u.usage)
      {
        start = 0;
      }
      else
      {
        start = end = u.eventId;
        us = u.usage;
      }
    }

    end = u.eventId;
  }

  if(start != 0)
    callback(start, end, us);
}

QVariant SDObject2Variant(const SDObject *obj, bool inlineImportant)
{
  QVariant param;

  // we don't identify via the type name as many types could be serialised as a ResourceId -
  // e.g. ID3D11Resource* or ID3D11Buffer* which would be the actual typename. We want to preserve
  // that for the best raw structured data representation instead of flattening those out to just
  // "ResourceId", and we also don't want to store two types ('fake' and 'real'), so instead we
  // check the custom string.
  if(obj->type.basetype == SDBasic::Resource)
  {
    param = QVariant::fromValue(obj->data.basic.id);
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
    Formatter::FormatterFlags flags = Formatter::NoFlags;
    if((obj->type.flags & SDTypeFlags::OffsetOrSize) ||
       ((obj->GetParent() && obj->GetParent()->type.flags & SDTypeFlags::OffsetOrSize)))
      flags = Formatter::OffsetSize;

    switch(obj->type.basetype)
    {
      case SDBasic::Chunk:
      {
        if(inlineImportant)
        {
          QString name = obj->name;

          // don't display any "ClassName::" prefix by default here
          int nsSep = name.indexOf(lit("::"));
          if(nsSep > 0)
            name.remove(0, nsSep + 2);

          name += lit("(");

          bool onlyImportant(obj->type.flags & SDTypeFlags::ImportantChildren);

          bool first = true;
          for(const SDObject *child : *obj)
          {
            // never display hidden members
            if(child->type.flags & SDTypeFlags::Hidden)
              continue;

            if(!onlyImportant || (child->type.flags & SDTypeFlags::Important))
            {
              if(!first)
                name += lit(", ");
              name += SDObject2Variant(child, true).toString();
              first = false;
            }
          }

          name += lit(")");
          param = name;
        }
        else
        {
          param = QVariant();
        }
        break;
      }
      case SDBasic::Struct:
      case SDBasic::Array:
      {
        // only inline important arrays with up to 4 elements
        if(inlineImportant && (obj->type.basetype == SDBasic::Struct || obj->NumChildren() <= 4))
        {
          int numImportantChildren = 0;

          if(obj->NumChildren() == 0)
          {
            param = lit("{}");
          }
          else
          {
            bool importantChildren(obj->type.flags & SDTypeFlags::ImportantChildren);

            bool first = true;
            QString s;
            for(size_t i = 0; i < obj->NumChildren(); i++)
            {
              const SDObject *child = obj->GetChild(i);

              if(!importantChildren || (obj->GetChild(i)->type.flags & SDTypeFlags::Important))
              {
                if(!first)
                  s += lit(", ");
                first = false;
                s += SDObject2Variant(child, true).toString();
                numImportantChildren++;
              }
            }

            // when a struct only has one important member, just display that as-if it were the
            // struct. We rely on the underlying important flagging to not make things too
            // confusing.
            // This addresses case where there's a struct hierarchy but we only care about one
            // member a level or two down - we don't end up with { { { { Resource } } } }
            // it also helps with structs that we want to display as just a single thing - like a
            // struct that references a resource with adjacent properties, which we want to elide to
            // just the resource itself.
            if(importantChildren && numImportantChildren == 1 && obj->type.basetype == SDBasic::Struct)
              param = s;
            else
              param = QFormatStr("{ %1 }").arg(s);
          }
        }
        else
        {
          param = obj->type.basetype == SDBasic::Array
                      ? QFormatStr("%1[%2]").arg(obj->type.name).arg(obj->NumChildren())
                      : QFormatStr("%1()").arg(obj->type.name);
        }
        break;
      }
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
      case SDBasic::UnsignedInteger:
        param = Formatter::HumanFormat(obj->data.basic.u, flags);
        break;
      case SDBasic::SignedInteger: param = Formatter::Format(obj->data.basic.i); break;
      case SDBasic::Float: param = Formatter::Format(obj->data.basic.d); break;
      case SDBasic::Boolean: param = (obj->data.basic.b ? lit("True") : lit("False")); break;
      case SDBasic::Character: param = QString(QLatin1Char(obj->data.basic.c)); break;
    }
  }

  return param;
}

void addStructuredChildren(RDTreeWidgetItem *parent, const SDObject &parentObj)
{
  for(const SDObject *obj : parentObj)
  {
    if(obj->type.flags & SDTypeFlags::Hidden)
      continue;

    QVariant name;

    if(parentObj.type.basetype == SDBasic::Array)
      name = QFormatStr("[%1]").arg(parent->childCount());
    else
      name = obj->name;

    RDTreeWidgetItem *item = new RDTreeWidgetItem({name, QString()});

    item->setText(1, SDObject2Variant(obj, false));

    if(obj->type.basetype == SDBasic::Chunk || obj->type.basetype == SDBasic::Struct ||
       obj->type.basetype == SDBasic::Array)
      addStructuredChildren(item, *obj);

    parent->addChild(item);
  }
}

static void validateForJSON(const QVariant &data, QString path = QString())
{
  switch((QMetaType::Type)data.type())
  {
    case QMetaType::QVariantList:
    {
      QVariantList list = data.toList();
      int i = 0;
      for(QVariant &v : list)
        validateForJSON(v, path + QFormatStr("[%1]").arg(i++));
      break;
    }
    case QMetaType::QVariantMap:
    {
      QVariantMap map = data.toMap();
      for(const QString &str : map.keys())
        validateForJSON(map[str], path + lit(".") + str);
      break;
    }
    case QMetaType::QByteArray:
    {
      qCritical() << "Qt can't reliably serialise QByteArray to JSON.\n"
                  << "Older versions write it as a byte string, new versions base64 encode it.\n"
                  << "Manually encode if needed and add value as string." << path;
    }
    default:
      // all other types we assume are fine
      break;
  }
}

static QJsonDocument validateAndMakeJSON(const QVariantMap &data)
{
  validateForJSON(data);

  return QJsonDocument::fromVariant(data);
}

bool SaveToJSON(QVariantMap &data, QIODevice &f, const char *magicIdentifier, uint32_t magicVersion)
{
  // marker that this data is valid
  if(magicIdentifier)
    data[QString::fromLatin1(magicIdentifier)] = magicVersion;

  QJsonDocument doc = validateAndMakeJSON(data);

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
  return QString::fromUtf8(validateAndMakeJSON(data).toJson(QJsonDocument::Indented));
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
    // we expect that this should always be non-negative because only the filters we know about
    // should get selected
    if(i >= 0)
    {
      fd.setDefaultSuffix(defaultSuffixes.value(i));
    }
    else
    {
      // GNOME has a bug that passes an empty string to filterSelected, so we ignore it with a
      // warning.
      if(filter == QString())
      {
        qWarning() << "Empty filter string passed to QFileDialog::filterSelected. "
                   << "Ignoring this as a likely GNOME bug, default suffix is still: "
                   << fd.defaultSuffix();
      }
      else
      {
        // some filter that we don't recognise was selected! Try to figure out the suffix on the
        // fly
        QStringList suffixes = getDefaultSuffixesFromFilter(filter);

        if(suffixes.empty())
        {
          qWarning() << "Unknown filter " << filter << " selected. "
                     << "Couldn't determine filename suffix, default suffix is still: "
                     << fd.defaultSuffix();
        }
        else
        {
          fd.setDefaultSuffix(suffixes[0]);

          qWarning() << "Unknown filter " << filter << " selected. "
                     << "Using default suffix: " << fd.defaultSuffix();
        }
      }
    }
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
      QFormatStr(
          "QLabel { border: solid #%1%2%3; border-bottom-width: 1px; border-right-width: 1px;")
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

      cellStyle += lit(" };");

      w->setStyleSheet(cellStyle);
    }
  }
}

int Formatter::m_minFigures = 2, Formatter::m_maxFigures = 5, Formatter::m_expNegCutoff = 5,
    Formatter::m_expPosCutoff = 7;
double Formatter::m_expNegValue = 0.00001;       // 10^(-5)
double Formatter::m_expPosValue = 10000000.0;    // 10^7
QFont *Formatter::m_Font = NULL;
QFont *Formatter::m_FixedFont = NULL;
float Formatter::m_FontBaseSize = 10.0f;    // this should always be overridden below, but just in
                                            // case let's pick a sensible value
QString Formatter::m_DefaultFontFamily;
QString Formatter::m_DefaultMonoFontFamily;
float Formatter::m_FixedFontBaseSize = 10.0f;
QColor Formatter::m_DarkChecker, Formatter::m_LightChecker;
OffsetSizeDisplayMode Formatter::m_OffsetSizeDisplayMode = OffsetSizeDisplayMode::Auto;

void Formatter::setParams(const PersistantConfig &config)
{
  m_minFigures = qMax(0, config.Formatter_MinFigures);
  m_maxFigures = qMax(2, config.Formatter_MaxFigures);
  m_expNegCutoff = qMax(0, config.Formatter_NegExp);
  m_expPosCutoff = qMax(0, config.Formatter_PosExp);

  m_expNegValue = qPow(10.0, -config.Formatter_NegExp);
  m_expPosValue = qPow(10.0, config.Formatter_PosExp);

  m_OffsetSizeDisplayMode = config.Formatter_OffsetSizeDisplayMode;

  if(!m_Font)
  {
    m_Font = new QFont();
    m_FontBaseSize = QApplication::font().pointSizeF();
    m_FixedFont = new QFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_FixedFontBaseSize = m_FixedFont->pointSizeF();
    m_DefaultFontFamily = QApplication::font().family();
    m_DefaultMonoFontFamily = QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
  }

  // this is only used for display to the user
  if(m_DefaultFontFamily.isEmpty())
    m_DefaultFontFamily = lit("System font");
  if(m_DefaultMonoFontFamily.isEmpty())
    m_DefaultMonoFontFamily = lit("System font");

  if(config.Font_PreferMonospaced)
  {
    *m_Font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    if(!config.Font_MonoFamily.isEmpty())
      m_Font->setFamily(config.Font_MonoFamily);
  }
  else
  {
    *m_Font = QFont();
    if(!config.Font_Family.isEmpty())
      m_Font->setFamily(config.Font_Family);
  }

  m_Font->setPointSizeF(m_FontBaseSize * config.Font_GlobalScale);
  QFont f = QApplication::font();
  f.setPointSizeF(m_FontBaseSize * config.Font_GlobalScale);
  if(!config.Font_Family.isEmpty())
    f.setFamily(config.Font_Family);
  QApplication::setFont(f);

  if(!config.Font_MonoFamily.isEmpty())
    m_FixedFont->setFamily(config.Font_MonoFamily);

  m_FixedFont->setPointSizeF(m_FixedFontBaseSize * config.Font_GlobalScale);

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

QString Formatter::HumanFormat(uint64_t u, FormatterFlags flags)
{
  if(u == UINT16_MAX)
    return lit("UINT16_MAX");
  if(u == UINT32_MAX)
    return lit("UINT32_MAX");
  if(u == UINT64_MAX)
    return lit("UINT64_MAX");

  // format as hex when over a certain threshold
  bool displayHex = (u > 0xffffff);

  if(flags & OffsetSize)
  {
    switch(m_OffsetSizeDisplayMode)
    {
      case OffsetSizeDisplayMode::Hexadecimal: displayHex = true; break;
      case OffsetSizeDisplayMode::Decimal: displayHex = false; break;
      default: break;
    }
  }
  if(displayHex)
  {
    if(u < UINT32_MAX)
    {
      uint32_t u32 = u;
      return lit("0x") + Format(u32, true);
    }
    return lit("0x") + Format(u, true);
  }

  return Format(u);
}

class RDProgressDialog : public QProgressDialog
{
public:
  RDProgressDialog(const QString &labelText, QWidget *parent)
      // we add 1 so that the progress value never hits maximum until we are actually finished
      : QProgressDialog(labelText, QString(), 0, maxProgress + 1, parent), m_Label(this)
  {
    setWindowTitle(tr("Please Wait"));
    setWindowFlags(Qt::CustomizeWindowHint | Qt::Dialog | Qt::WindowTitleHint);
    setWindowIcon(QIcon());
    setMinimumSize(QSize(250, 0));
    setMaximumSize(QSize(500, 200));
    setCancelButton(NULL);
    setMinimumDuration(0);
    setWindowModality(Qt::ApplicationModal);
    setValue(0);

    m_Label.setText(labelText);
    m_Label.setAlignment(Qt::AlignCenter);
    m_Label.setWordWrap(true);

    setLabel(&m_Label);
  }

  void enableCancel() { setCancelButtonText(tr("Cancel")); }
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
  void closeEvent(QCloseEvent *event) override { event->ignore(); }
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
      lit("kdesudo"),
      lit("gksudo"),
      lit("beesu"),
  };

  // if none of the graphical options, then look for sudo and either
  const QString termEmulator[] = {
      lit("x-terminal-emulator"),
      lit("gnome-terminal"),
      lit("konsole"),
      lit("xterm"),
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
    QObject::connect(process, OverloadedSlot<int, QProcess::ExitStatus>::of(&QProcess::finished),
                     [parent, process, finishedCallback](int exitCode) {
                       process->deleteLater();
                       GUIInvoke::call(parent, finishedCallback);
                     });

    return true;
  }

  QString sudo = QStandardPaths::findExecutable(lit("sudo"));

  if(sudo.isEmpty())
  {
    RDDialog::critical(parent, lit("Error running program as root"),
                       lit("Couldn't find graphical or terminal sudo program!\n"
                           "Please run '%1' with args '%2' manually.")
                           .arg(fullExecutablePath)
                           .arg(params.join(QLatin1Char(' '))));
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
    termParams << lit("-e")
               << lit("bash -c 'echo Running \"%1 %2\" as root.;echo;sudo %1 %2'")
                      .arg(fullExecutablePath)
                      .arg(params.join(QLatin1Char(' ')));

    process->start(term, termParams);

    // when the process exits, call the callback and delete
    QObject::connect(process, OverloadedSlot<int, QProcess::ExitStatus>::of(&QProcess::finished),
                     [parent, process, finishedCallback](int exitCode) {
                       process->deleteLater();
                       GUIInvoke::call(parent, finishedCallback);
                     });

    return true;
  }

  RDDialog::critical(parent, lit("Error running program as root"),
                     lit("Couldn't find graphical or terminal emulator to launch sudo!\n"
                         "Please manually run: sudo \"%1\" %2")
                         .arg(fullExecutablePath)
                         .arg(params.join(QLatin1Char(' '))));

  return false;
#endif
}

void RevealFilenameInExternalFileBrowser(const QString &filePath)
{
#if defined(Q_OS_WIN32)
  // on windows we can ask explorer to highlight the exact file.
  QProcess::startDetached(lit("explorer.exe"),
                          QStringList() << lit("/select,") << QDir::toNativeSeparators(filePath));
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
  rdcstr argString = args;

  // perform some kind of sane parsing
  bool dquot = false, squot = false;    // are we inside ''s or ""s

  // current character
  char *c = &argString[0];

  // current argument we're building
  rdcstr a;

  while(*c)
  {
    if(!dquot && !squot && (*c == ' ' || *c == '\t'))
    {
      if(!a.empty())
        ret << QString(a);

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
    ret << QString(a);
#endif

  return ret;
}

void ShowProgressDialog(QWidget *window, const QString &labelText, ProgressFinishedMethod finished,
                        ProgressUpdateMethod update, ProgressCancelMethod cancel)
{
  if(finished())
    return;

  RDProgressDialog dialog(labelText, window);

  if(cancel)
    dialog.enableCancel();

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
  progressTickerThread.setName(lit("Progress Dialog"));
  progressTickerThread.start();

  // show the dialog
  RDDialog::show(&dialog);

  // signal the thread to exit if somehow we got here without it finishing, then wait for the thread
  // to clean itself up
  tickerSemaphore.tryAcquire();
  progressTickerThread.wait();

  if(cancel && dialog.wasCanceled())
    cancel();
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

// we declare this partial class to get the accessors. THIS IS DANGEROUS as the ABI is unstable and
// this is a private class. The first few functions have been stable for a while so we hope that it
// will remain so. If a stable interface is added in future like QX11Info we should definitely use
// it instead.
//
// Unfortunately we need this for Wayland, so we only ever use it when we are absolutely forced to
// because we're running under the Wayland Qt platform.
class QOpenGLContext;

class Q_GUI_EXPORT QPlatformNativeInterface : public QObject
{
  Q_OBJECT
public:
  virtual void *nativeResourceForIntegration(const QByteArray &resource);
  virtual void *nativeResourceForContext(const QByteArray &resource, QOpenGLContext *context);
  virtual void *nativeResourceForScreen(const QByteArray &resource, QScreen *screen);
  virtual void *nativeResourceForWindow(const QByteArray &resource, QWindow *window);
};

void *AccessWaylandPlatformInterface(const QByteArray &resource, QWindow *window)
{
  QPlatformNativeInterface *native = QGuiApplication::platformNativeInterface();
  return native->nativeResourceForWindow(resource, window);
}

// Default Qt doesn't do this in release Qt builds, which is all we use
#if defined(Q_OS_WIN32)

#include <windows.h>

typedef HRESULT(WINAPI *PFN_SetThreadDescription)(HANDLE hThread, PCWSTR lpThreadDescription);

const DWORD MS_VC_EXCEPTION = 0x406D1388;
#pragma pack(push, 8)
typedef struct tagTHREADNAME_INFO
{
  DWORD dwType;        // Must be 0x1000.
  LPCSTR szName;       // Pointer to name (in user addr space).
  DWORD dwThreadID;    // Thread ID (-1=caller thread).
  DWORD dwFlags;       // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)

static void SetThreadNameWithException(const char *name)
{
  THREADNAME_INFO info;
  info.dwType = 0x1000;
  info.szName = name;
  info.dwThreadID = GetCurrentThreadId();
  info.dwFlags = 0;
  __try
  {
    RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR *)(&info));
  }
  __except(EXCEPTION_CONTINUE_EXECUTION)
  {
  }
}

void LambdaThread::windowsSetName()
{
  // try to use the fancy modern API
  static PFN_SetThreadDescription setThreadDesc = (PFN_SetThreadDescription)GetProcAddress(
      GetModuleHandleA("kernel32.dll"), "SetThreadDescription");

  if(setThreadDesc)
  {
    setThreadDesc(GetCurrentThread(), m_Name.toStdWString().c_str());
  }
  else
  {
    // don't throw the exception if there's no debugger present
    if(!IsDebuggerPresent())
      return;

    SetThreadNameWithException(m_Name.toStdString().c_str());
  }
}

#else

void LambdaThread::windowsSetName()
{
}

#endif

void UpdateVisibleColumns(rdcstr windowTitle, int columnCount, QHeaderView *header,
                          const QStringList &headers)
{
  QDialog dialog;
  RDListWidget list;
  QDialogButtonBox buttons;

  dialog.setWindowTitle(windowTitle);
  dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);

  for(int visIdx = 0; visIdx < columnCount; visIdx++)
  {
    int logIdx = header->logicalIndex(visIdx);

    QListWidgetItem *item = new QListWidgetItem(headers[logIdx], &list);

    item->setData(Qt::UserRole, logIdx);

    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);

    // The first column must stay enabled
    if(logIdx == 0)
      item->setFlags(item->flags() & ~Qt::ItemIsEnabled);

    item->setCheckState(header->isSectionHidden(logIdx) ? Qt::Unchecked : Qt::Checked);
  }

  list.setSelectionMode(QAbstractItemView::SingleSelection);
  list.setDragDropMode(QAbstractItemView::DragDrop);
  list.setDefaultDropAction(Qt::MoveAction);

  buttons.setOrientation(Qt::Horizontal);
  buttons.setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  buttons.setCenterButtons(true);

  QObject::connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  QObject::connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  QVBoxLayout *layout = new QVBoxLayout(&dialog);
  layout->addWidget(new QLabel(QString::fromUtf8("Select the columns to enable."), &dialog));
  layout->addWidget(&list);
  layout->addWidget(&buttons);

  if(!RDDialog::show(&dialog))
    return;

  for(int i = 0; i < columnCount; i++)
  {
    int logicalIdx = list.item(i)->data(Qt::UserRole).toInt();

    if(list.item(i)->checkState() == Qt::Unchecked)
      header->hideSection(logicalIdx);
    else
      header->showSection(logicalIdx);

    header->moveSection(header->visualIndex(logicalIdx), i);
  }
}

const rdcarray<SDObject *> &StructuredDataItemModel::objects() const
{
  return m_Objects;
}

void StructuredDataItemModel::setObjects(const rdcarray<SDObject *> &objs)
{
  emit beginResetModel();
  m_Objects = objs;
  emit endResetModel();
}

// on 64-bit we've got plenty of bits so we can pack the indices into top/bottom 32-bits.
// on 32-bit we assume there won't be any huge arrays or we'll run out of memory elsewhere probably,
// so we pack 9/21 bits allowing for up to 512 arrays of ~2 million entries each.
// bear in mind 2 bits are reserved for the index tag itself
struct IndexMasks
{
  static constexpr quintptr IndexBits() { return sizeof(void *) == 8 ? 32U : 21U; }
  static quintptr GetArrayID(quintptr packed) { return packed >> IndexBits(); }
  static quintptr GetIndexInArray(quintptr packed)
  {
    return packed & ((quintptr(1U) << IndexBits()) - 1);
  }
  static quintptr Pack(quintptr arrayId, quintptr idxInArray)
  {
    return (arrayId << IndexBits()) | idxInArray;
  }
};

StructuredDataItemModel::Index StructuredDataItemModel::decodeIndex(QModelIndex idx) const
{
  // if it's a direct pointer, the low bits will be 0 for alignment.
  Index ret;
  ret.tag = IndexTag(idx.internalId() & 0x3);

  if(ret.tag == Direct)
  {
    ret.obj = (SDObject *)idx.internalPointer();
  }
  else
  {
    quintptr packed = idx.internalId() >> 2;
    ret.obj = (SDObject *)m_Arrays[IndexMasks::GetArrayID(packed)];
    ret.indexInArray = IndexMasks::GetIndexInArray(packed);
  }

  return ret;
}

quintptr StructuredDataItemModel::encodeIndex(Index idx) const
{
  if(idx.tag == Direct)
    return (quintptr)idx.obj;

  int arrayId = m_Arrays.indexOf(idx.obj);
  if(arrayId == -1)
  {
    m_Arrays.push_back(idx.obj);
    arrayId = m_Arrays.count() - 1;
  }

  quintptr packed = IndexMasks::Pack(arrayId, idx.indexInArray);
  return (packed << 2U) | quintptr(idx.tag);
}

// for large arrays (more than this size) paginate it with pages of this size.
// This is primarily beneficial for lazy arrays to avoid needing to lazily evaluate a huge array.
const int ArrayPageSize = 250;

bool StructuredDataItemModel::isLargeArray(SDObject *obj) const
{
  return (int)obj->NumChildren() > ArrayPageSize;
}

QModelIndex StructuredDataItemModel::index(int row, int column, const QModelIndex &parent) const
{
  if(row < 0 || column < 0 || row >= rowCount(parent) || column >= columnCount(parent))
    return QModelIndex();

  SDObject *par = NULL;

  if(parent.isValid())
  {
    Index decodedParent = decodeIndex(parent);

    // the children of page nodes are real nodes. We cache the array member's index here too for
    // parent() lookups
    if(decodedParent.tag == PageNode)
    {
      int idx = decodedParent.indexInArray + row;

      m_ArrayMembers[decodedParent.obj->GetChild(idx)] = idx;

      return createIndex(row, column, encodeIndex({ArrayMember, decodedParent.obj, idx}));
    }
    else if(decodedParent.tag == ArrayMember)
    {
      par = decodedParent.obj->GetChild(decodedParent.indexInArray);
    }
    else
    {
      par = decodedParent.obj;
    }

    // if this parent node is a large array, the children are page nodes, otherwise the child is a
    // direct node
    if(isLargeArray(par))
    {
      return createIndex(row, column, encodeIndex({PageNode, par, row * ArrayPageSize}));
    }
    else
    {
      if(par->type.flags & SDTypeFlags::HiddenChildren)
      {
        // if this node has hidden children, get the index relative to only visible children
        int idx = 0;
        for(size_t i = 0; i < par->NumChildren(); i++)
        {
          SDObject *ch = par->GetChild(i);

          if(ch->type.flags & SDTypeFlags::Hidden)
            continue;

          if(idx == row)
            return createIndex(row, column, encodeIndex({Direct, ch, 0}));

          idx++;
        }
      }

      return createIndex(row, column, encodeIndex({Direct, par->GetChild(row), 0}));
    }
  }
  else
  {
    return createIndex(row, column, encodeIndex({Direct, m_Objects[row], 0}));
  }
}

QModelIndex StructuredDataItemModel::parent(const QModelIndex &index) const
{
  if(index.internalPointer() == NULL)
    return QModelIndex();

  Index decodedIndex = decodeIndex(index);

  SDObject *obj = NULL;

  // array members have parents that are page nodes
  if(decodedIndex.tag == ArrayMember)
  {
    int pageRow = decodedIndex.indexInArray / ArrayPageSize;
    return createIndex(decodedIndex.indexInArray / ArrayPageSize, 0,
                       encodeIndex({PageNode, decodedIndex.obj, pageRow * ArrayPageSize}));
  }
  else if(decodedIndex.tag == PageNode)
  {
    obj = decodedIndex.obj;
  }
  else
  {
    obj = decodedIndex.obj->GetParent();
  }

  // need to figure out the index for obj, it could be an array member itself in theory, or it might
  // be direct, or it could be a root object
  if(obj)
  {
    SDObject *parent = obj->GetParent();

    if(parent == NULL)
    {
      int row = m_Objects.indexOf(obj);
      if(row >= 0)
        return createIndex(row, 0, obj);

      qCritical() << "Encountered object with no parent that is not a root";

      return QModelIndex();
    }

    // if the parent is a large array
    if(isLargeArray(parent))
    {
      // we expect to have set up our member index before this lookup was ever needed
      auto it = m_ArrayMembers.find(obj);
      if(it == m_ArrayMembers.end())
      {
        qCritical() << "Expected member index to be set up, but it is not";

        return QModelIndex();
      }

      // return the index for this item, with the child index we looked up
      return createIndex(it.value(), 0, encodeIndex({ArrayMember, parent, it.value()}));
    }

    // search our parent to find out our child index
    int idx = 0;
    bool largeArray = isLargeArray(parent);
    for(size_t i = 0; i < parent->NumChildren(); i++)
    {
      const SDObject *ch = parent->GetChild(i);
      if(ch == obj)
        return createIndex(idx, 0, obj);

      // for non-large arrays, we account for hidden children. Large arrays should not have any
      // hidden children anyway
      if(!largeArray && (ch->type.flags & SDTypeFlags::Hidden))
        continue;

      idx++;
    }

    return QModelIndex();
  }

  return QModelIndex();
}

int StructuredDataItemModel::rowCount(const QModelIndex &parent) const
{
  if(!parent.isValid())
    return m_Objects.count();

  Index decodedIdx = decodeIndex(parent);

  SDObject *obj = NULL;

  // if this is a page node, it either has PageSize children if it's not the last one, or the
  // remainder
  if(decodedIdx.tag == PageNode)
  {
    size_t pageBase = decodedIdx.indexInArray;

    return qMin(ArrayPageSize, int(decodedIdx.obj->NumChildren() - pageBase));
  }
  else if(decodedIdx.tag == ArrayMember)
  {
    obj = decodedIdx.obj->GetChild(decodedIdx.indexInArray);
  }
  else
  {
    obj = decodedIdx.obj;
  }

  if(obj)
  {
    if(isLargeArray(obj))
    {
      return (int(obj->NumChildren()) + ArrayPageSize - 1) / ArrayPageSize;
    }
    else
    {
      if(obj->type.flags & SDTypeFlags::HiddenChildren)
      {
        // if this node has hidden children, count the *actual* number of children
        int numChildren = 0;
        for(size_t i = 0; i < obj->NumChildren(); i++)
        {
          if(obj->GetChild(i)->type.flags & SDTypeFlags::Hidden)
            continue;

          numChildren++;
        }

        return numChildren;
      }
      else
      {
        return (int)obj->NumChildren();
      }
    }
  }

  return 0;
}

int StructuredDataItemModel::columnCount(const QModelIndex &parent) const
{
  return m_ColumnNames.count();
}

QVariant StructuredDataItemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if(orientation == Qt::Horizontal && role == Qt::DisplayRole)
  {
    if(section < m_ColumnNames.count())
      return m_ColumnNames[section];
  }

  return QVariant();
}

Qt::ItemFlags StructuredDataItemModel::flags(const QModelIndex &index) const
{
  if(!index.isValid())
    return 0;

  return QAbstractItemModel::flags(index);
}

QVariant StructuredDataItemModel::data(const QModelIndex &index, int role) const
{
  if(role != Qt::DisplayRole || index.column() >= columnCount())
    return QVariant();

  Index decodedIdx = decodeIndex(index);
  SDObject *obj = NULL;

  if(decodedIdx.tag == PageNode)
  {
    if(m_ColumnValues[index.column()] == Name)
    {
      size_t pageBase = decodedIdx.indexInArray;
      size_t pageCount = qMin(ArrayPageSize, int(decodedIdx.obj->NumChildren() - pageBase));

      return QFormatStr("[%1..%2]").arg(pageBase).arg(pageBase + pageCount - 1);
    }
    return QVariant();
  }
  else if(decodedIdx.tag == ArrayMember)
  {
    obj = decodedIdx.obj->GetChild(decodedIdx.indexInArray);
  }
  else
  {
    obj = decodedIdx.obj;
  }

  if(obj)
  {
    switch(m_ColumnValues[index.column()])
    {
      case Name:
        if(decodedIdx.tag == ArrayMember)
          return QFormatStr("[%1]").arg(decodedIdx.indexInArray);
        else if(obj->GetParent() && obj->GetParent()->type.basetype == SDBasic::Array)
          return QFormatStr("[%1]").arg(index.row());
        else
          return obj->name;
      case Value:
      {
        QVariant v = SDObject2Variant(obj, obj->GetParent() ? false : true);
        RichResourceTextInitialise(v, NULL);
        return v;
      }
      case Type: return obj->type.name;
    }
  }

  return QVariant();
}

QRClickToolButton::QRClickToolButton(QWidget *parent) : QToolButton(parent)
{
}

void QRClickToolButton::mousePressEvent(QMouseEvent *e)
{
  if(e->button() == Qt::RightButton)
    emit rightClicked();
  else
    QToolButton::mousePressEvent(e);
}
