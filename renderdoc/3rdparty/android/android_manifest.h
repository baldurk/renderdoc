/*
 * Copyright (C) 2005 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <stdint.h>

//////////////////////////////////////////////////////////////////////////////////
//
// These constants are taken from ResourceTypes.h:
// https://android.googlesource.com/platform/frameworks/base/+/42ebcb80b50834a1ce4755cd4ca86918c96ca3c6/libs/androidfw/include/androidfw/ResourceTypes.h
//
// The ones we need are extracted here to not be dependent on the rest of the android framework.
// There are some slight modifications for ease of integration, but this is still all under the
// android license.

enum class ResType : uint16_t
{
  Null = 0x0000,
  StringPool = 0x0001,
  XML = 0x0003,
  NamespaceStart = 0x0100,
  NamespaceEnd = 0x0101,
  StartElement = 0x0102,
  EndElement = 0x0103,
  CDATA = 0x0104,
  ResourceMap = 0x0180,
};

/**
 * Header that appears at the front of every data chunk in a resource.
 */
struct ResChunk_header
{
  // Type identifier for this chunk.  The meaning of this value depends
  // on the containing chunk.
  ResType type;

  // Size of the chunk header (in bytes).  Adding this value to
  // the address of the chunk allows you to find its associated data
  // (if any).
  uint16_t headerSize;

  // Total size of this chunk (in bytes).  This is the chunkSize plus
  // the size of any data associated with the chunk.  Adding this value
  // to the chunk allows you to completely skip its contents (including
  // any child chunks).  If this value is the same as chunkSize, there is
  // no data associated with the chunk.
  uint32_t size;
};

/**
 * Reference to a string in a string pool.
 */
struct ResStringPool_ref
{
  // Index into the string pool table (uint32_t-offset from the indices
  // immediately after ResStringPool_header) at which to find the location
  // of the string data in the pool.
  uint32_t index;
};

/**
 * Representation of a value in a resource, supplying type
 * information.
 */
struct Res_value
{
  // Number of bytes in this structure.
  uint16_t size;

  // Always set to 0.
  uint8_t res0;

  // Type of the data value.
  enum class DataType : uint8_t
  {
    // The 'data' holds an index into the containing resource table's
    // global value string pool.
    String = 0x03,
    // The 'data' is either 0 or 1, for input "false" or "true" respectively.
    Boolean = 0x12,
  } dataType;

  // The data for this item, as interpreted according to dataType.
  uint32_t data;
};

/**
 * Definition for a pool of strings.  The data of this chunk is an
 * array of uint32_t providing indices into the pool, relative to
 * stringsStart.  At stringsStart are all of the UTF-16 strings
 * concatenated together; each starts with a uint16_t of the string's
 * length and each ends with a 0x0000 terminator.  If a string is >
 * 32767 characters, the high bit of the length is set meaning to take
 * those 15 bits as a high word and it will be followed by another
 * uint16_t containing the low word.
 *
 * If styleCount is not zero, then immediately following the array of
 * uint32_t indices into the string table is another array of indices
 * into a style table starting at stylesStart.  Each entry in the
 * style table is an array of ResStringPool_span structures.
 */
struct ResStringPool_header
{
  struct ResChunk_header header;

  // Number of strings in this pool (number of uint32_t indices that follow
  // in the data).
  uint32_t stringCount;

  // Number of style span arrays in the pool (number of uint32_t indices
  // follow the string indices).
  uint32_t styleCount;

  // Flags.
  enum StringFlags : uint32_t
  {
    // If set, the string index is sorted by the string values (based
    // on strcmp16()).
    SORTED_FLAG = 1 << 0,

    // String pool is encoded in UTF-8
    UTF8_FLAG = 1 << 8
  };
  StringFlags flags;

  // Index from header of the string data.
  uint32_t stringsStart;

  // Index from header of the style data.
  uint32_t stylesStart;
};

/**
 * Basic XML tree node.  A single item in the XML document.  Extended info
 * about the node can be found after header.headerSize.
 */
struct ResXMLTree_node
{
  struct ResChunk_header header;

  // Line number in original source file at which this element appeared.
  uint32_t lineNumber;

  // Optional XML comment that was associated with this element; -1 if none.
  ResStringPool_ref comment;
};

/**
 * Extended XML tree node for namespace start/end nodes.
 * Appears header.headerSize bytes after a ResXMLTree_node.
 */
struct ResXMLTree_namespaceExt
{
  // The prefix of the namespace.
  struct ResStringPool_ref prefix;

  // The URI of the namespace.
  struct ResStringPool_ref uri;
};

/**
 * Extended XML tree node for element start/end nodes.
 * Appears header.headerSize bytes after a ResXMLTree_node.
 */
struct ResXMLTree_endElementExt
{
  // String of the full namespace of this element.
  struct ResStringPool_ref ns;

  // String name of this node if it is an ELEMENT; the raw
  // character data if this is a CDATA node.
  struct ResStringPool_ref name;
};

/**
 * Extended XML tree node for start tags -- includes attribute
 * information.
 * Appears header.headerSize bytes after a ResXMLTree_node.
 */
struct ResXMLTree_attrExt
{
  // String of the full namespace of this element.
  struct ResStringPool_ref ns;

  // String name of this node if it is an ELEMENT; the raw
  // character data if this is a CDATA node.
  struct ResStringPool_ref name;

  // Byte offset from the start of this structure where the attributes start.
  uint16_t attributeStart;

  // Size of the ResXMLTree_attribute structures that follow.
  uint16_t attributeSize;

  // Number of attributes associated with an ELEMENT.  These are
  // available as an array of ResXMLTree_attribute structures
  // immediately following this node.
  uint16_t attributeCount;

  // Index (1-based) of the "id" attribute. 0 if none.
  uint16_t idIndex;

  // Index (1-based) of the "class" attribute. 0 if none.
  uint16_t classIndex;

  // Index (1-based) of the "style" attribute. 0 if none.
  uint16_t styleIndex;
};

struct ResXMLTree_attribute
{
  // Namespace of this attribute.
  struct ResStringPool_ref ns;

  // Name of this attribute.
  struct ResStringPool_ref name;

  // The original raw string value of this attribute.
  struct ResStringPool_ref rawValue;

  // Processesd typed value of this attribute.
  struct Res_value typedValue;
};

/**
 * Extended XML tree node for CDATA tags -- includes the CDATA string.
 * Appears header.headerSize bytes after a ResXMLTree_node.
 */
struct ResXMLTree_cdataExt
{
  // The raw CDATA character data.
  struct ResStringPool_ref data;

  // The typed value of the character data if this is a CDATA node.
  struct Res_value typedData;
};