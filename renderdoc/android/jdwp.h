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

#pragma once

#include "serialise/streamio.h"

namespace JDWP
{
// enums and structs defined by the JDWP spec. These are defined in:
// https://docs.oracle.com/javase/7/docs/platform/jpda/jdwp/jdwp-protocol.html#JDWP_Tag

enum class CommandSet : byte
{
  Unknown = 0,
  VirtualMachine = 1,
  ReferenceType = 2,
  ClassType = 3,
  ArrayType = 4,
  InterfaceType = 5,
  Method = 6,
  Field = 8,
  ObjectReference = 9,
  StringReference = 10,
  ThreadReference = 11,
  ThreadGroupReference = 12,
  ArrayReference = 13,
  ClassLoaderReference = 14,
  EventRequest = 15,
  StackFrame = 16,
  ClassObjectReference = 17,
  Event = 64,
};

enum class TypeTag : byte
{
  Class = 1,
  Interface = 2,
  Arrary = 3,
};

enum class Tag : byte
{
  Unknown = '0',
  Array = '[',
  Byte = 'B',
  Char = 'C',
  Object = 'L',
  Float = 'F',
  Double = 'D',
  Int = 'I',
  Long = 'J',
  Short = 'S',
  Void = 'V',
  Boolean = 'Z',
  String = 's',
  Thread = 't',
  ThreadGroup = 'g',
  ClassLoader = 'l',
  ClassObject = 'c',
};

enum class EventKind : byte
{
  Unknown = 0,
  SingleStep = 1,
  Breakpoint = 2,
  FramePop = 3,
  Exception = 4,
  UserDefined = 5,
  ThreadStart = 6,
  ThreadDeath = 7,
  ThreadEnd = 7,
  ClassPrepare = 8,
  ClassUnload = 9,
  ClassLoad = 10,
  FieldAccess = 20,
  FieldModification = 21,
  ExceptionCatch = 30,
  MethodEntry = 40,
  MethodExit = 41,
  MethodExitWithReturnValue = 42,
  MonitorContendedEnter = 43,
  MonitorContendedEntered = 44,
  MonitorWait = 45,
  MonitorWaited = 46,
  VMStart = 90,
  VMInit = 90,
  VMDeath = 99,
  VMDisconnected = 100,
};

enum class ModifierKind : byte
{
  Count = 1,
  Conditional = 2,
  ThreadOnly = 3,
  ClassOnly = 4,
  ClassMatch = 5,
  ClassExclude = 6,
  LocationOnly = 7,
  ExceptionOnly = 8,
  FieldOnly = 9,
  Step = 10,
  InstanceOnly = 11,
  SourceNameMatch = 12,
};

enum class SuspendPolicy : byte
{
  None = 0,
  EventThread = 1,
  All = 2,
};

enum class InvokeOptions : int32_t
{
  SingleThreaded = 0x1,
  NonVirtual = 0x2,
};

enum class ClassStatus : int32_t
{
  Verified = 0x1,
  Prepared = 0x2,
  Initialized = 0x4,
  Error = 0x8,
};

enum class Error : uint16_t
{
  None = 0,
  InvalidThread = 10,
  InvalidThreadGroup = 11,
  InvalidPriority = 12,
  ThreadNotSuspended = 13,
  ThreadSuspended = 14,
  ThreadNotAlive = 15,
  InvalidObject = 20,
  InvalidClass = 21,
  ClassNotPrepared = 22,
  InvalidMethodid = 23,
  InvalidLocation = 24,
  InvalidFieldid = 25,
  InvalidFrameid = 30,
  NoMoreFrames = 31,
  OpaqueFrame = 32,
  NotCurrentFrame = 33,
  TypeMismatch = 34,
  InvalidSlot = 35,
  Duplicate = 40,
  NotFound = 41,
  InvalidMonitor = 50,
  NotMonitorOwner = 51,
  Interrupt = 52,
  InvalidClassFormat = 60,
  CircularClassDefinition = 61,
  FailsVerification = 62,
  AddMethodNotImplemented = 63,
  SchemaChangeNotImplemented = 64,
  InvalidTypestate = 65,
  HierarchyChangeNotImplemented = 66,
  DeleteMethodNotImplemented = 67,
  UnsupportedVersion = 68,
  NamesDontMatch = 69,
  ClassModifiersChangeNotImplemented = 70,
  MethodModifiersChangeNotImplemented = 71,
  NotImplemented = 99,
  NullPointer = 100,
  AbsentInformation = 101,
  InvalidEventType = 102,
  IllegalArgument = 103,
  OutOfMemory = 110,
  AccessDenied = 111,
  VmDead = 112,
  Internal = 113,
  UnattachedThread = 115,
  InvalidTag = 500,
  AlreadyInvoking = 502,
  InvalidIndex = 503,
  InvalidLength = 504,
  InvalidString = 506,
  InvalidClassLoader = 507,
  InvalidArray = 508,
  TransportLoad = 509,
  TransportInit = 510,
  NativeMethod = 511,
  InvalidCount = 512,
};

struct CommandData;

// different IDs in JDWP can be different sizes, but we want to basically treat them all the same.
// For that reason we have a templated struct (jdwpID) which has all the functions we need, we
// instantiate it for each *unique* ID type, and then further typedef for other IDs that are the
// same.

// we abstract the actual size away, and always treat an ID as a uint64 (if it's actually 4 bytes,
// we just only read/write the lower 4 bytes).
enum class IDType
{
  field,
  method,
  object,
  refType,
  frame
};

template <IDType t>
struct jdwpID
{
public:
  jdwpID() = default;
  jdwpID(uint64_t v) { data.u64 = v; }
  operator uint64_t() const { return size == 4 ? data.u32 : data.u64; }
  static int32_t getSize() { return size; }
  static void setSize(int32_t s)
  {
    RDCASSERT(s == 4 || s == 8);
    jdwpID<t>::size = s;
  }

  void EndianSwap()
  {
    if(size == 4)
      data.u32 = ::EndianSwap(data.u32);
    else
      data.u64 = ::EndianSwap(data.u64);
  }

private:
  union
  {
    uint32_t u32;
    uint64_t u64;
  } data;
  static int32_t size;
};

typedef jdwpID<IDType::object> objectID;
typedef objectID threadID;
typedef objectID threadGroupID;
typedef objectID stringID;
typedef objectID classLoaderID;
typedef objectID classObjectID;
typedef objectID arrayID;

// docs are weird - the protocol says referenceTypeID size is "same as objectID", but it has a
// separate ID size. To be safe, keep it separate.
typedef jdwpID<IDType::refType> referenceTypeID;
typedef referenceTypeID classID;
typedef referenceTypeID interfaceID;
typedef referenceTypeID arrayTypeID;

typedef jdwpID<IDType::method> methodID;
typedef jdwpID<IDType::field> fieldID;
typedef jdwpID<IDType::frame> frameID;

template <IDType t>
int32_t jdwpID<t>::size = 8;

struct taggedObjectID
{
  Tag tag;
  objectID id;
};

struct value
{
  Tag tag;

  union
  {
    arrayID Array;
    byte Byte;
    char Char;
    objectID Object;
    referenceTypeID RefType;
    float Float;
    double Double;
    int32_t Int;
    int64_t Long;
    int16_t Short;
    bool Bool;
    objectID String;
    threadID Thread;
    threadGroupID ThreadGroup;
    classLoaderID ClassLoader;
    classObjectID ClassObject;
  };
};

struct Location
{
  TypeTag tag;
  classID clss;
  methodID meth;
  uint64_t index;
};

struct Method
{
  methodID id;
  std::string name;
  std::string signature;
  int32_t modBits;
};

struct Field
{
  fieldID id;
  std::string name;
  std::string signature;
  int32_t modBits;
};

struct VariableSlot
{
  uint64_t codeIndex;
  std::string name;
  std::string signature;
  uint32_t length;
  int32_t slot;
};

struct StackFrame
{
  frameID id;
  Location location;
};

struct EventFilter
{
  ModifierKind modKind;
  referenceTypeID ClassOnly;
};

struct Event
{
  EventKind eventKind;
  int32_t requestID;

  struct
  {
    threadID thread;
    Location location;
  } MethodEntry;

  struct
  {
    threadID thread;
    TypeTag refTypeTag;
    referenceTypeID typeID;
    std::string signature;
    union
    {
      // get around strict aliasing
      ClassStatus status;
      int32_t statusInt;
    };
  } ClassPrepare;
};

struct Command
{
  Command(CommandSet s = CommandSet::Unknown, byte c = 0) : commandset(s), command(c) {}
  uint32_t Send(StreamWriter &writer);
  void Recv(StreamReader &reader);

  // only these need to be publicly writeable
  CommandSet commandset;
  byte command;
  // get the ID assigned/received
  uint32_t GetID() { return id; }
  // get the error received (only for replies)
  Error GetError() { return error; }
  // read-write access to the data (encodes and endian swaps)
  CommandData GetData();

private:
  static uint32_t idalloc;

  // automatically calculated
  uint32_t length = 0;
  uint32_t id = 0;
  Error error = Error::None;
  std::vector<byte> data;
};

// a helper class for reading/writing the payload of a packet, with endian swapping.
struct CommandData
{
  CommandData(std::vector<byte> &dataStore) : data(dataStore) {}
  template <typename T>
  CommandData &Read(T &val)
  {
    RDCCOMPILE_ASSERT(sizeof(bool) == 1, "bool must be 1 byte for default template");
    ReadBytes(&val, sizeof(T));
    val = EndianSwap(val);
    return *this;
  }

  template <typename T>
  CommandData &Write(const T &val)
  {
    T tmp = EndianSwap(val);
    WriteBytes(&tmp, sizeof(T));
    return *this;
  }

  // called when we've finished reading, to ensure we consumed all the data
  void Done() { RDCASSERT(offs == data.size(), offs, data.size()); }
private:
  std::vector<byte> &data;
  size_t offs = 0;

  void ReadBytes(void *bytes, size_t length);
  void WriteBytes(const void *bytes, size_t length);
};

// overloads for the basic types we want to read and write
template <>
CommandData &CommandData::Read(std::string &str);
template <>
CommandData &CommandData::Write(const std::string &str);
template <>
CommandData &CommandData::Read(taggedObjectID &id);
template <>
CommandData &CommandData::Write(const taggedObjectID &id);
template <>
CommandData &CommandData::Read(value &val);
template <>
CommandData &CommandData::Write(const value &val);
template <>
CommandData &CommandData::Read(Location &loc);
template <>
CommandData &CommandData::Write(const Location &loc);

// objectID variants
template <>
CommandData &CommandData::Read(objectID &id);
template <>
CommandData &CommandData::Write(const objectID &id);
template <>
CommandData &CommandData::Read(referenceTypeID &id);
template <>
CommandData &CommandData::Write(const referenceTypeID &id);
template <>
CommandData &CommandData::Read(methodID &id);
template <>
CommandData &CommandData::Write(const methodID &id);
template <>
CommandData &CommandData::Read(fieldID &id);
template <>
CommandData &CommandData::Write(const fieldID &id);
template <>
CommandData &CommandData::Read(frameID &id);
template <>
CommandData &CommandData::Write(const frameID &id);

typedef std::function<bool(const Event &evData)> EventFilterFunction;

// A JDWP connection, with high-level helper functions that implement the protocol underneath
class Connection
{
public:
  Connection(Network::Socket *sock);
  ~Connection();

  bool IsErrored() { return error || writer.IsErrored() || reader.IsErrored(); }
  // suspend or resume the whole VM's execution.
  void Suspend();
  void Resume();

  // this must be called first before any of the commands that use IDs. It's separated out because
  // depending on the circumstance it might be necessary to suspend the VM first before sending this
  // command.
  void SetupIDSizes();

  // get the type handle for a given JNI signature
  referenceTypeID GetType(const std::string &signature);

  // get the type handle for an object
  referenceTypeID GetType(objectID obj);

  // get a field handle. If signature is empty, it's ignored for matching
  fieldID GetField(referenceTypeID type, const std::string &name, const std::string &signature = "");

  // get the value of a static field
  value GetFieldValue(referenceTypeID type, fieldID field);

  // get a method handle. If signature is empty, it's ignored for matching
  // The actual class for the method (possibly a parent of 'type') will be returned in methClass if
  // non-NULL
  methodID GetMethod(referenceTypeID type, const std::string &name,
                     const std::string &signature = "", referenceTypeID *methClass = NULL);

  // get a local variable slot. If signature is empty, it's ignored for matching. Returns -1 if not
  // found
  int32_t GetLocalVariable(referenceTypeID type, methodID method, const std::string &name,
                           const std::string &signature = "");

  // get a thread's stack frames
  std::vector<StackFrame> GetCallStack(threadID thread);

  // get the this pointer for a given stack frame
  objectID GetThis(threadID thread, frameID frame);

  // get the value of a string object
  std::string GetString(objectID str);

  // resume the VM and wait for an event to happen, filtered by some built-in filters or a callback.
  // Returns the matching event and leaves the VM suspended, or an empty event if there was a
  // problem
  Event WaitForEvent(EventKind kind, const std::vector<EventFilter> &eventFilters,
                     EventFilterFunction filterCallback);

  // create a new string reference on the given thread
  value NewString(threadID thread, const std::string &str);

  // get/set local variables
  value GetLocalValue(threadID thread, frameID frame, int32_t slot, Tag tag);
  void SetLocalValue(threadID thread, frameID frame, int32_t slot, value val);

  // invoke a static method
  value InvokeStatic(threadID thread, classID clazz, methodID method,
                     const std::vector<value> &arguments,
                     InvokeOptions options = InvokeOptions::SingleThreaded)
  {
    // instance detects if object is empty, and invokes as static
    return InvokeInstance(thread, clazz, method, 0, arguments, options);
  }
  // invoke an instance method
  value InvokeInstance(threadID thread, classID clazz, methodID method, objectID object,
                       const std::vector<value> &arguments,
                       InvokeOptions options = InvokeOptions::SingleThreaded);

private:
  StreamWriter writer;
  StreamReader reader;
  bool error = false;

  int suspendRefCount = 0;

  bool SendReceive(Command &cmd);
  void ReadEvent(CommandData &data, Event &ev);

  classID GetSuper(classID clazz);
  std::string GetSignature(referenceTypeID typeID);
  std::vector<Method> GetMethods(referenceTypeID searchClass);
};
};