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

namespace JDWP
{
// short helper functions to read/write vectors - always preceeded by an int length
template <typename inner>
void ReadVector(CommandData &data, std::vector<inner> &vec,
                std::function<void(CommandData &data, inner &i)> process)
{
  int32_t count = 0;
  data.Read(count);

  vec.resize((size_t)count);
  for(int32_t i = 0; i < count; i++)
    process(data, vec[i]);
}

template <typename inner>
void WriteVector(CommandData &data, const std::vector<inner> &vec,
                 std::function<void(CommandData &data, const inner &i)> process)
{
  int32_t count = (int32_t)vec.size();
  data.Write(count);

  for(int32_t i = 0; i < count; i++)
    process(data, vec[i]);
}

Connection::Connection(Network::Socket *sock)
    : writer(sock, Ownership::Nothing), reader(sock, Ownership::Nothing)
{
  // the first thing we do is write the handshake bytes and expect them immediately echo'd back.
  const int handshakeLength = 14;
  const char handshake[handshakeLength + 1] = "JDWP-Handshake";

  writer.Write(handshake, handshakeLength);
  writer.Flush();

  char response[15] = {};
  reader.Read(response, handshakeLength);

  if(memcmp(handshake, response, 14))
  {
    RDCERR("handshake failed - received >%s< - expected >%s<", response, handshake);
    error = true;
  }
}

Connection::~Connection()
{
  // bail immediately if we're in error state
  if(IsErrored())
    return;

  // otherwise, undo any suspends we might have done that are still outstanding, in case a logic
  // error made us bail while we had the VM suspended. We copy the refcount since Resume()
  // decrements it.
  int ref = suspendRefCount;
  for(int i = 0; i < ref; i++)
    Resume();
}

bool Connection::SendReceive(Command &cmd)
{
  // send the command, and recieve the reply back into the same object.
  // save the auto-generated ID for this command so we can compare it to the reply - we expect a
  // synchronous reply, no other commands in the way.
  uint32_t id = cmd.Send(writer);
  cmd.Recv(reader);
  Threading::Sleep(10);

  if(id != cmd.GetID())
  {
    RDCERR("Didn't get matching reply packet for %d/%d", cmd.commandset, cmd.command);
    error = true;
    return false;
  }

  return true;
}

void Connection::SetupIDSizes()
{
  Command cmd(CommandSet::VirtualMachine, 7);
  if(!SendReceive(cmd))
    return;

  int32_t fieldIDSize = 0;
  int32_t methodIDSize = 0;
  int32_t objectIDSize = 0;
  int32_t referenceTypeIDSize = 0;
  int32_t frameIDSize = 0;

  cmd.GetData()
      .Read(fieldIDSize)
      .Read(methodIDSize)
      .Read(objectIDSize)
      .Read(referenceTypeIDSize)
      .Read(frameIDSize)
      .Done();

  if(objectIDSize != referenceTypeIDSize)
  {
    RDCWARN("objectID (%d) is not the same size as referenceTypeID (%d). Could cause problems!",
            objectIDSize, referenceTypeIDSize);
  }

  // we only support 4 or 8 bytes
  if(fieldIDSize != 4 && fieldIDSize != 8)
  {
    RDCLOG("fieldID size %d is unsupported!", fieldIDSize);
    error = true;
    return;
  }

  fieldID::setSize(fieldIDSize);

  // we only support 4 or 8 bytes
  if(methodIDSize != 4 && methodIDSize != 8)
  {
    RDCLOG("methodID size %d is unsupported!", methodIDSize);
    error = true;
    return;
  }

  methodID::setSize(methodIDSize);

  // we only support 4 or 8 bytes
  if(objectIDSize != 4 && objectIDSize != 8)
  {
    RDCLOG("objectID size %d is unsupported!", objectIDSize);
    error = true;
    return;
  }

  objectID::setSize(objectIDSize);

  // we only support 4 or 8 bytes
  if(referenceTypeIDSize != 4 && referenceTypeIDSize != 8)
  {
    RDCLOG("referenceTypeID size %d is unsupported!", referenceTypeIDSize);
    error = true;
    return;
  }

  referenceTypeID::setSize(referenceTypeIDSize);

  // we only support 4 or 8 bytes
  if(frameIDSize != 4 && frameIDSize != 8)
  {
    RDCLOG("frameID size %d is unsupported!", frameIDSize);
    error = true;
    return;
  }

  frameID::setSize(frameIDSize);
}

void Connection::Suspend()
{
  suspendRefCount++;

  Command cmd(CommandSet::VirtualMachine, 8);
  SendReceive(cmd);
}

void Connection::Resume()
{
  if(suspendRefCount > 0)
    suspendRefCount--;
  else
    RDCERR("Resuming while we are believed to be running! suspend refcount problem");

  Command cmd(CommandSet::VirtualMachine, 9);
  SendReceive(cmd);
}

referenceTypeID Connection::GetType(const std::string &signature)
{
  Command cmd(CommandSet::VirtualMachine, 2);
  cmd.GetData().Write(signature);

  if(!SendReceive(cmd))
    return {};

  int32_t numTypes = 0;
  byte typetag;
  referenceTypeID ret;
  int32_t status;    // unused

  cmd.GetData().Read(numTypes).Read(typetag).Read(ret).Read(status).Done();

  if(numTypes == 0)
    return {};
  else if(numTypes > 1)
    RDCWARN("Multiple types found for '%s'", signature.c_str());

  return ret;
}

referenceTypeID Connection::GetType(objectID obj)
{
  Command cmd(CommandSet::ObjectReference, 1);
  cmd.GetData().Write(obj);

  if(!SendReceive(cmd))
    return {};

  byte tag = 0;
  referenceTypeID ret = {};

  cmd.GetData().Read(tag).Read(ret).Done();
  return ret;
}

methodID Connection::GetMethod(referenceTypeID type, const std::string &name,
                               const std::string &signature, referenceTypeID *methClass)
{
  referenceTypeID searchClass = type;

  while(searchClass != 0)
  {
    /*
    RDCDEBUG("Searching for %s (%s) in '%s'", name.c_str(), signature.c_str(),
             GetSignature(searchClass).c_str());
             */

    std::vector<Method> methods = GetMethods(searchClass);

    for(const Method &m : methods)
    {
      if(m.name == name && (signature == "" || signature == m.signature))
      {
        if(methClass)
          *methClass = searchClass;
        return m.id;
      }
    }

    searchClass = GetSuper(searchClass);
  }

  return {};
}

int32_t Connection::GetLocalVariable(referenceTypeID type, methodID method, const std::string &name,
                                     const std::string &signature)
{
  Command cmd(CommandSet::Method, 2);
  cmd.GetData().Write(type).Write(method);

  if(!SendReceive(cmd))
    return -1;

  int32_t argumentCount = 0;
  std::vector<VariableSlot> slots;

  CommandData data = cmd.GetData();
  data.Read(argumentCount);    // unused for now
  ReadVector<VariableSlot>(data, slots, [](CommandData &data, VariableSlot &s) {
    data.Read(s.codeIndex).Read(s.name).Read(s.signature).Read(s.length).Read(s.slot);
  });
  data.Done();

  for(const VariableSlot &s : slots)
    if(s.name == name && (signature == "" || signature == s.signature))
      return s.slot;

  return -1;
}

fieldID Connection::GetField(referenceTypeID type, const std::string &name,
                             const std::string &signature)
{
  Command cmd(CommandSet::ReferenceType, 4);
  cmd.GetData().Write(type);

  if(!SendReceive(cmd))
    return {};

  std::vector<Field> fields;
  CommandData data = cmd.GetData();
  ReadVector<Field>(data, fields, [](CommandData &data, Field &f) {
    data.Read(f.id).Read(f.name).Read(f.signature).Read(f.modBits);
  });
  data.Done();

  for(const Field &f : fields)
    if(f.name == name && (signature == "" || signature == f.signature))
      return f.id;

  return {};
}

value Connection::GetFieldValue(referenceTypeID type, fieldID field)
{
  Command cmd(CommandSet::ReferenceType, 6);
  cmd.GetData().Write(type).Write<int32_t>(1).Write(field);

  if(!SendReceive(cmd))
    return {};

  int32_t numVals = 0;
  value ret;

  cmd.GetData().Read(numVals).Read(ret).Done();

  if(numVals != 1)
    RDCWARN("Unexpected number of values found in GetValue: %d", numVals);

  return ret;
}

std::vector<StackFrame> Connection::GetCallStack(threadID thread)
{
  Command cmd(CommandSet::ThreadReference, 6);
  // always fetch full stack
  cmd.GetData().Write(thread).Write<int32_t>(0).Write<int32_t>(-1);

  if(!SendReceive(cmd))
    return {};

  std::vector<StackFrame> ret;
  CommandData data = cmd.GetData();
  ReadVector<StackFrame>(
      data, ret, [](CommandData &data, StackFrame &f) { data.Read(f.id).Read(f.location); });
  data.Done();

  // simplify error handling, if the stack came back as nonsense then clear it
  if(!ret.empty() && ret[0].id == 0)
    ret.clear();

  return ret;
}

objectID Connection::GetThis(threadID thread, frameID frame)
{
  Command cmd(CommandSet::StackFrame, 3);
  cmd.GetData().Write(thread).Write(frame);

  if(!SendReceive(cmd))
    return {};

  taggedObjectID ret = {};
  cmd.GetData().Read(ret).Done();
  return ret.id;
}

void Connection::ReadEvent(CommandData &data, Event &ev)
{
  data.Read((byte &)ev.eventKind).Read(ev.requestID);

  switch(ev.eventKind)
  {
    case EventKind::ClassPrepare:
    {
      data.Read(ev.ClassPrepare.thread)
          .Read((byte &)ev.ClassPrepare.refTypeTag)
          .Read(ev.ClassPrepare.typeID)
          .Read(ev.ClassPrepare.signature)
          .Read(ev.ClassPrepare.statusInt);
      break;
    }
    case EventKind::MethodEntry:
    {
      data.Read(ev.MethodEntry.thread).Read(ev.MethodEntry.location);
      break;
    }
    default: RDCERR("Unexpected event! Add handling");
  }
}

Event Connection::WaitForEvent(EventKind kind, const std::vector<EventFilter> &eventFilters,
                               EventFilterFunction filterCallback)
{
  int32_t eventRequestID = 0;

  {
    Command cmd(CommandSet::EventRequest, 1);
    CommandData data = cmd.GetData();

    // always suspend all threads
    data.Write((byte)kind).Write((byte)SuspendPolicy::All);

    WriteVector<EventFilter>(data, eventFilters, [](CommandData &data, const EventFilter &f) {
      data.Write((byte)f.modKind);
      if(f.modKind == ModifierKind::ClassOnly)
        data.Write(f.ClassOnly);
      else
        RDCERR("Unsupported event filter %d", f.modKind);
    });

    if(!SendReceive(cmd))
      return {};

    cmd.GetData().Read(eventRequestID).Done();
  }

  if(eventRequestID == 0)
  {
    RDCERR("Failed to set event");
    error = true;
    return {};
  }

  // now resume execution
  Resume();

  // wait for method entry on the method we care about
  while(!reader.IsErrored())
  {
    SuspendPolicy suspendPolicy;
    std::vector<Event> events;

    Command msg;
    msg.Recv(reader);

    if(msg.commandset != CommandSet::Event || msg.command != 100)
    {
      RDCERR("Expected event packet, but got %d/%d", msg.commandset, msg.command);
      error = true;
      return {};
    }

    CommandData data = msg.GetData();

    data.Read((byte &)suspendPolicy);
    ReadVector<Event>(data, events, [this](CommandData &data, Event &ev) { ReadEvent(data, ev); });
    data.Done();

    // event arrived, we're now suspended
    if(suspendPolicy != SuspendPolicy::None)
      suspendRefCount++;

    for(const Event &event : events)
    {
      if(event.eventKind == kind && event.requestID == eventRequestID)
      {
        // check callback filter
        if(filterCallback(event))
        {
          // stop listening to this event, and leave VM suspended
          Command cmd(CommandSet::EventRequest, 2);
          cmd.GetData().Write((byte)EventKind::MethodEntry).Write(eventRequestID);
          SendReceive(cmd);

          // return the matching event
          return event;
        }
      }
    }

    // resume to get the next event
    Resume();
  }

  // something went wrong, we stopped receiving events before the found we wanted.
  return {};
}

value Connection::NewString(threadID thread, const std::string &str)
{
  Command cmd(CommandSet::VirtualMachine, 11);
  cmd.GetData().Write(str);

  if(!SendReceive(cmd))
    return {};

  stringID ret;
  cmd.GetData().Read(ret).Done();
  return {Tag::String, {ret}};
}

value Connection::GetLocalValue(threadID thread, frameID frame, int32_t slot, Tag tag)
{
  Command cmd(CommandSet::StackFrame, 1);
  // request one value
  cmd.GetData().Write(thread).Write(frame).Write<int32_t>(1).Write(slot).Write((byte)tag);

  SendReceive(cmd);

  int32_t numVals = 0;
  value ret;

  cmd.GetData().Read(numVals).Read(ret).Done();

  if(numVals != 1)
    RDCWARN("Unexpected number of values found in GetValue: %d", numVals);

  return ret;
}

void Connection::SetLocalValue(threadID thread, frameID frame, int32_t slot, value val)
{
  Command cmd(CommandSet::StackFrame, 2);
  // set one value
  cmd.GetData().Write(thread).Write(frame).Write<int32_t>(1).Write(slot).Write(val);

  SendReceive(cmd);
}

value Connection::InvokeInstance(threadID thread, classID clazz, methodID method, objectID object,
                                 const std::vector<value> &arguments, InvokeOptions options)
{
  Command cmd;
  CommandData data = cmd.GetData();

  // static or instance invoke
  if(object == 0)
  {
    cmd.commandset = CommandSet::ClassType;
    cmd.command = 3;

    data.Write(clazz).Write(thread).Write(method);
  }
  else
  {
    cmd.commandset = CommandSet::ObjectReference;
    cmd.command = 6;

    data.Write(object).Write(thread).Write(clazz).Write(method);
  }

  WriteVector<value>(data, arguments, [](CommandData &data, const value &v) { data.Write(v); });

  data.Write((int32_t)options);

  if(!SendReceive(cmd))
    return {};

  value returnValue;
  taggedObjectID exception;

  cmd.GetData().Read(returnValue).Read(exception).Done();

  if(exception.id != 0)
  {
    RDCERR("Exception encountered while invoking method");
    return {};
  }

  return returnValue;
}

std::string Connection::GetString(objectID str)
{
  Command cmd(CommandSet::StringReference, 1);
  cmd.GetData().Write(str);

  if(!SendReceive(cmd))
    return "";

  std::string ret;
  cmd.GetData().Read(ret).Done();
  return ret;
}

classID Connection::GetSuper(classID clazz)
{
  Command cmd(CommandSet::ClassType, 1);
  cmd.GetData().Write(clazz);

  if(!SendReceive(cmd))
    return {};

  classID ret;
  cmd.GetData().Read(ret).Done();
  return ret;
}

std::string Connection::GetSignature(referenceTypeID typeID)
{
  Command cmd(CommandSet::ReferenceType, 1);
  cmd.GetData().Write(typeID);

  if(!SendReceive(cmd))
    return {};

  std::string ret;
  cmd.GetData().Read(ret).Done();
  return ret;
}

std::vector<Method> Connection::GetMethods(referenceTypeID searchClass)
{
  Command cmd(CommandSet::ReferenceType, 5);
  cmd.GetData().Write(searchClass);

  if(!SendReceive(cmd))
    return {};

  std::vector<Method> ret;
  CommandData data = cmd.GetData();
  ReadVector<Method>(data, ret, [](CommandData &data, Method &m) {
    data.Read(m.id).Read(m.name).Read(m.signature).Read(m.modBits);
  });
  data.Done();
  return ret;
}
};