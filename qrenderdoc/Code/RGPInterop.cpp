/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2019 Baldur Karlsson
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

#include "RGPInterop.h"
#include <QApplication>
#include <QTcpServer>
#include <QTcpSocket>

bool RGPInterop::RGPSupportsInterop(const QString &RGPPath)
{
  uint32_t majorVersion = 0;
  uint32_t minorVersion = 0;

  const char *searchString = "RGPVersion=";
  const int searchStringLength = (int)strlen(searchString);

  // look for an embedded string in the exe
  QFile f(RGPPath);
  if(f.open(QIODevice::ReadOnly))
  {
    QByteArray contents = f.readAll();

    int search = 0;
    do
    {
      int needle = contents.indexOf("RGPVersion=", search);

      if(needle == -1)
        break;

      search = needle + searchStringLength;

      // bail if there isn't enough room for the string plus X.Y
      if(contents.size() - needle < searchStringLength + 3)
        break;

      // get the major version number
      const char *major = contents.data() + search;

      const char *sep = major;

      // find the separator
      while(*sep >= '0' && *sep <= '9')
        sep++;

      // get the minor version number
      const char *minor = sep + 1;

      const char *end = minor;

      // find the end
      while(*end >= '0' && *end <= '9')
        end++;

      // convert the strings to integers
      QByteArray majorStr(major, sep - major);
      QByteArray minorStr(minor, end - minor);

      bool ok = false;
      majorVersion = majorStr.toUInt(&ok);

      if(!ok)
      {
        majorVersion = 0;
        continue;
      }

      minorVersion = minorStr.toUInt(&ok);

      if(!ok)
      {
        majorVersion = minorVersion = 0;
        continue;
      }

      // found the version
      break;

    } while(search >= 0);
  }

  // interop supported in RGP V1.2 and higher
  if(majorVersion > 1 || (majorVersion == 1 && minorVersion > 1))
  {
    return true;
  }

  return false;
}

template <>
rdcstr DoStringise(const RGPCommand &el)
{
  BEGIN_ENUM_STRINGISE(RGPCommand);
  {
    STRINGISE_ENUM_CLASS_NAMED(Initialize, "initialize");
    STRINGISE_ENUM_CLASS_NAMED(SetEvent, "set_event");
    STRINGISE_ENUM_CLASS_NAMED(Terminate, "terminate");
  }
  END_ENUM_STRINGISE();
}

RGPInterop::RGPInterop(ICaptureContext &ctx) : m_Ctx(ctx)
{
  m_Server = new QTcpServer(NULL);
  m_Server->listen(QHostAddress::Any, Port);

  QObject::connect(m_Server, &QTcpServer::newConnection, [this]() {
    if(m_Socket == NULL)
    {
      m_Socket = m_Server->nextPendingConnection();
      ConnectionEstablished();
    }
    else
    {
      // close any other connections while we already have one
      delete m_Server->nextPendingConnection();
    }
  });
}

RGPInterop::~RGPInterop()
{
  RGPInteropTerminate terminate;
  QString encoded = EncodeCommand(RGPCommand::Terminate, terminate.toParams(m_Version));

  if(m_Socket)
  {
    m_Socket->write(encoded.trimmed().toUtf8().data());
    m_Socket->waitForBytesWritten();
  }

  m_Server->close();
  delete m_Server;
}

void RGPInterop::InitializeRGP()
{
  RGPInteropInit init;

  init.interop_version = 1;
  init.interop_name = lit("RenderDoc");

  QString encoded = EncodeCommand(RGPCommand::Initialize, init.toParams(m_Version));

  if(m_Socket)
  {
    m_Socket->write(encoded.trimmed().toUtf8().data());
  }
}

bool RGPInterop::HasRGPEvent(uint32_t eventId)
{
  if(m_Version == 0)
    return false;

  if(m_Socket == NULL)
    return false;

  return m_Event2RGP[eventId].interoplinearid != 0;
}

bool RGPInterop::SelectRGPEvent(uint32_t eventId)
{
  if(m_Version == 0)
    return false;

  RGPInteropEvent ev = m_Event2RGP[eventId];

  if(ev.interoplinearid == 0)
    return false;

  QString encoded = EncodeCommand(RGPCommand::SetEvent, ev.toParams(m_Version));

  if(m_Socket)
  {
    m_Socket->write(encoded.trimmed().toUtf8().data());
    return true;
  }

  return false;
}

void RGPInterop::EventSelected(RGPInteropEvent event)
{
  uint32_t eventId = m_RGP2Event[event.interoplinearid];

  if(eventId == 0)
  {
    qWarning() << "RGP Event " << event.interoplinearid << event.cmdbufid << event.eventname
               << " did not correspond to a known eventId";
    return;
  }

  const DrawcallDescription *draw = m_Ctx.GetDrawcall(eventId);

  if(draw && QString(draw->name) != event.eventname)
    qWarning() << "Drawcall name mismatch. Expected " << event.eventname << " but got "
               << QString(draw->name);

  m_Ctx.SetEventID({}, eventId, eventId);

  BringToForeground(m_Ctx.GetMainWindow()->Widget());
}

void RGPInterop::ConnectionEstablished()
{
  QObject::connect(m_Socket, &QAbstractSocket::disconnected, [this]() {
    m_Socket->deleteLater();
    m_Socket = NULL;
  });

  // initial handshake and protocol version
  InitializeRGP();

  // TODO: negotiate mapping version
  uint32_t version = 1;
  CreateMapping(version);

  // add a handler that appends all data to the read buffer and processes each time more comes in.
  QObject::connect(m_Socket, &QIODevice::readyRead, [this]() {
    // append all available data
    m_ReadBuffer += m_Socket->readAll();

    // process the read buffer
    ProcessReadBuffer();
  });
}

void RGPInterop::CreateMapping(const rdcarray<DrawcallDescription> &drawcalls)
{
  const SDFile &file = m_Ctx.GetStructuredFile();

  for(const DrawcallDescription &draw : drawcalls)
  {
    for(const APIEvent &ev : draw.events)
    {
      if(ev.chunkIndex == 0 || ev.chunkIndex >= file.chunks.size())
        continue;

      const SDChunk *chunk = file.chunks[ev.chunkIndex];

      if(m_EventNames.contains(chunk->name, Qt::CaseSensitive))
      {
        m_Event2RGP[ev.eventId].interoplinearid = (uint32_t)m_RGP2Event.size();
        if(ev.eventId == draw.eventId)
          m_Event2RGP[ev.eventId].eventname = draw.name;
        else
          m_Event2RGP[ev.eventId].eventname = chunk->name;

        m_RGP2Event.push_back(ev.eventId);
      }
    }

    // if we have children, step into them first before going to our next sibling
    if(!draw.children.empty())
      CreateMapping(draw.children);
  }
}

void RGPInterop::CreateMapping(uint32_t version)
{
  m_Version = version;

  if(m_Ctx.APIProps().pipelineType == GraphicsAPI::Vulkan)
  {
    if(version == 1)
    {
      m_EventNames << lit("vkCmdDispatch") << lit("vkCmdDraw") << lit("vkCmdDrawIndexed");
    }
  }
  else if(m_Ctx.APIProps().pipelineType == GraphicsAPI::D3D12)
  {
    // these names must match those in DoStringise(const D3D12Chunk &el) for the chunks

    if(version == 1)
    {
      m_EventNames << lit("ID3D12GraphicsCommandList::Dispatch")
                   << lit("ID3D12GraphicsCommandList::DrawInstanced")
                   << lit("ID3D12GraphicsCommandList::DrawIndexedInstanced");
    }
  }

  // if we don't have any event names, this API doesn't have a mapping or this was an unrecognised
  // version.
  if(m_EventNames.isEmpty())
    return;

  m_Event2RGP.resize(m_Ctx.GetLastDrawcall()->eventId + 1);

  // linearId 0 is invalid, so map to eventId 0.
  // the first real event will be linearId 1
  m_RGP2Event.push_back(0);

  CreateMapping(m_Ctx.CurDrawcalls());
}

QString RGPInterop::EncodeCommand(RGPCommand command, QVariantList params)
{
  QString ret;

  QString cmd = ToQStr(command);

  ret += lit("command=%1\n").arg(cmd);

  // iterate params in pair, name and value
  for(int i = 0; i + 1 < params.count(); i += 2)
    ret += QFormatStr("%1.%2=%3\n").arg(cmd).arg(params[i].toString()).arg(params[i + 1].toString());

  ret += lit("endcommand=%1\n").arg(cmd);

  return ret;
}

bool RGPInterop::DecodeCommand(QString command)
{
  QStringList lines = command.trimmed().split(QLatin1Char('\n'));

  if(lines[0].indexOf(lit("command=")) != 0 || lines.last().indexOf(lit("endcommand=")) != 0)
  {
    qWarning() << "Malformed RGP command:\n" << command;
    return false;
  }

  QString commandName = lines[0].split(QLatin1Char('='))[1];

  if(lines.last().split(QLatin1Char('='))[1] != commandName)
  {
    qWarning() << "Mismatch between command and endcommand:\n" << command;
    return false;
  }

  lines.pop_front();
  lines.pop_back();

  QVariantList params;

  QString prefix = commandName + lit(".");

  for(QString &param : lines)
  {
    int eq = param.indexOf(QLatin1Char('='));

    if(eq < 0)
    {
      qWarning() << "Malformed param: " << param;
      continue;
    }

    QString key = param.left(eq);
    QString value = param.mid(eq + 1);

    if(!key.startsWith(prefix))
    {
      qWarning() << "Malformed param key for" << commandName << ": " << key;
      continue;
    }

    key = key.mid(prefix.count());

    params << key << value;
  }

  if(commandName == ToQStr(RGPCommand::SetEvent))
  {
    RGPInteropEvent ev;
    ev.fromParams(m_Version, params);

    EventSelected(ev);

    return true;
  }
  else if(commandName == ToQStr(RGPCommand::Initialize))
  {
    RGPInteropInit init;
    init.fromParams(m_Version, params);

    // TODO: decode the params here. This will contain the interop
    // version and the name of the tool connected to RenderDoc

    return true;
  }
  else if(commandName == ToQStr(RGPCommand::Terminate))
  {
    // RGP has shut down so disconnect the socket etc
    emit m_Socket->disconnected();
    return true;
  }
  else
  {
    qWarning() << "Unrecognised command: " << commandName;
  }

  return false;
}

void RGPInterop::ProcessReadBuffer()
{
  // we might have partial data, so wait until we have a full command
  do
  {
    int idx = m_ReadBuffer.indexOf("endcommand=");

    // if we don't have endcommand= yet, we don't have a full command
    if(idx < 0)
      return;

    idx = m_ReadBuffer.indexOf('\n', idx);

    // also break if we don't have the full line yet including newline.
    if(idx < 0)
      return;

    // extract the command and decode as UTF-8
    QString command = QString::fromUtf8(m_ReadBuffer.data(), idx + 1);

    // remove the command from our buffer, to retain any partial subsequent command we might have
    m_ReadBuffer.remove(0, idx + 1);

    // process this command
    DecodeCommand(command);

    // loop again - we might have read multiple commands
  } while(true);
}
