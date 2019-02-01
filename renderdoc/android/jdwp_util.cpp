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

#include "jdwp.h"

// The handshake and packet spec is defined in:
// https://docs.oracle.com/javase/1.5.0/docs/guide/jpda/jdwp-spec.html
// This gives the overall structure of each packet, plus the format of the 'basic' types like
// objectID, value, location, etc.

namespace JDWP
{
uint32_t Command::idalloc = 42;

uint32_t Command::Send(StreamWriter &writer)
{
  id = idalloc++;
  length = 11 + (uint32_t)data.size();

  uint32_t tmp = EndianSwap(length);
  writer.Write(tmp);
  tmp = EndianSwap(id);
  writer.Write(tmp);

  // no need to endian swap these
  byte flags = 0;
  writer.Write(flags);
  writer.Write(commandset);
  writer.Write(command);

  // already endian swapped
  writer.Write(data.data(), data.size());

  writer.Flush();

  return id;
}

void Command::Recv(StreamReader &reader)
{
  reader.Read(length);
  length = EndianSwap(length);
  reader.Read(id);
  id = EndianSwap(id);

  byte flags = 0;
  reader.Read(flags);
  if(flags == 0x80)
  {
    commandset = CommandSet::Unknown;
    command = 0;
    uint16_t errInt = 0;
    reader.Read(errInt);
    error = (Error)EndianSwap(errInt);
  }
  else
  {
    reader.Read(commandset);
    reader.Read(command);
    error = Error::None;
  }

  data.clear();
  if(length >= 11)
  {
    data.resize(length - 11);
    reader.Read(&data[0], data.size());
  }
}

CommandData Command::GetData()
{
  return CommandData(data);
}

void CommandData::ReadBytes(void *bytes, size_t length)
{
  // read the data if there's enough, otherwise set to 0s.
  if(offs + length <= data.size())
    memcpy(bytes, data.data() + offs, length);
  else
    memset(bytes, 0, length);

  // always increment the offset so we can see how much we over-read in Done().
  offs += length;
}

void CommandData::WriteBytes(const void *bytes, size_t length)
{
  const byte *start = (const byte *)bytes;
  data.insert(data.end(), start, start + length);
}

template <>
CommandData &CommandData::Read(std::string &str)
{
  uint32_t length = 0;
  Read(length);
  str.resize(length);
  ReadBytes(&str[0], length);
  return *this;
}

template <>
CommandData &CommandData::Write(const std::string &str)
{
  uint32_t length = (uint32_t)str.size();
  Write(length);
  WriteBytes(str.c_str(), str.size());
  return *this;
}

template <>
CommandData &CommandData::Read(taggedObjectID &id)
{
  Read((byte &)id.tag);
  Read(id.id);
  return *this;
}

template <>
CommandData &CommandData::Write(const taggedObjectID &id)
{
  Write((const byte &)id.tag);
  Write(id.id);
  return *this;
}

template <>
CommandData &CommandData::Read(value &val)
{
  Read((byte &)val.tag);
  switch(val.tag)
  {
    case Tag::Unknown: break;
    case Tag::Array: Read(val.Array); break;
    case Tag::Byte: Read(val.Byte); break;
    case Tag::Char: Read(val.Char); break;
    case Tag::Object: Read(val.Object); break;
    case Tag::Float: Read(val.Float); break;
    case Tag::Double: Read(val.Double); break;
    case Tag::Int: Read(val.Int); break;
    case Tag::Long: Read(val.Long); break;
    case Tag::Short: Read(val.Short); break;
    case Tag::Void: break;
    case Tag::Boolean: Read(val.Bool); break;
    case Tag::String: Read(val.String); break;
    case Tag::Thread: Read(val.Thread); break;
    case Tag::ThreadGroup: Read(val.ThreadGroup); break;
    case Tag::ClassLoader: Read(val.ClassLoader); break;
    case Tag::ClassObject: Read(val.ClassObject); break;
  }
  return *this;
}

template <>
CommandData &CommandData::Write(const value &val)
{
  Write((const byte &)val.tag);
  switch(val.tag)
  {
    case Tag::Unknown: break;
    case Tag::Array: Write(val.Array); break;
    case Tag::Byte: Write(val.Byte); break;
    case Tag::Char: Write(val.Char); break;
    case Tag::Object: Write(val.Object); break;
    case Tag::Float: Write(val.Float); break;
    case Tag::Double: Write(val.Double); break;
    case Tag::Int: Write(val.Int); break;
    case Tag::Long: Write(val.Long); break;
    case Tag::Short: Write(val.Short); break;
    case Tag::Void: break;
    case Tag::Boolean: Write(val.Bool); break;
    case Tag::String: Write(val.String); break;
    case Tag::Thread: Write(val.Thread); break;
    case Tag::ThreadGroup: Write(val.ThreadGroup); break;
    case Tag::ClassLoader: Write(val.ClassLoader); break;
    case Tag::ClassObject: Write(val.ClassObject); break;
  }
  return *this;
}

template <>
CommandData &CommandData::Read(Location &loc)
{
  Read((byte &)loc.tag);
  Read(loc.clss);
  Read(loc.meth);
  Read(loc.index);
  return *this;
}

template <>
CommandData &CommandData::Write(const Location &loc)
{
  Write((const byte &)loc.tag);
  Write(loc.clss);
  Write(loc.meth);
  Write(loc.index);
  return *this;
}

// objectID variants
template <>
CommandData &CommandData::Read(objectID &id)
{
  ReadBytes(&id, objectID::getSize());
  id.EndianSwap();
  return *this;
}

template <>
CommandData &CommandData::Write(const objectID &id)
{
  objectID tmp = id;
  tmp.EndianSwap();
  WriteBytes(&tmp, objectID::getSize());
  return *this;
}

template <>
CommandData &CommandData::Read(referenceTypeID &id)
{
  ReadBytes(&id, referenceTypeID::getSize());
  id.EndianSwap();
  return *this;
}

template <>
CommandData &CommandData::Write(const referenceTypeID &id)
{
  referenceTypeID tmp = id;
  tmp.EndianSwap();
  WriteBytes(&tmp, referenceTypeID::getSize());
  return *this;
}

template <>
CommandData &CommandData::Read(methodID &id)
{
  ReadBytes(&id, methodID::getSize());
  id.EndianSwap();
  return *this;
}

template <>
CommandData &CommandData::Write(const methodID &id)
{
  methodID tmp = id;
  tmp.EndianSwap();
  WriteBytes(&tmp, methodID::getSize());
  return *this;
}

template <>
CommandData &CommandData::Read(fieldID &id)
{
  ReadBytes(&id, fieldID::getSize());
  id.EndianSwap();
  return *this;
}

template <>
CommandData &CommandData::Write(const fieldID &id)
{
  fieldID tmp = id;
  tmp.EndianSwap();
  WriteBytes(&tmp, fieldID::getSize());
  return *this;
}

template <>
CommandData &CommandData::Read(frameID &id)
{
  ReadBytes(&id, frameID::getSize());
  id.EndianSwap();
  return *this;
}

template <>
CommandData &CommandData::Write(const frameID &id)
{
  frameID tmp = id;
  tmp.EndianSwap();
  WriteBytes(&tmp, frameID::getSize());
  return *this;
}
};