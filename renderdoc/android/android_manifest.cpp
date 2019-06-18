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

#include "3rdparty/android/android_manifest.h"
#include "core/core.h"
#include "strings/string_utils.h"
#include "android_utils.h"

const uint32_t debuggableResourceId = 0x0101000f;
const uint32_t addingStringIndex = 0x8b8b8b8b;

namespace Android
{
std::string GetStringPoolValue(ResStringPool_header *stringpool, ResStringPool_ref ref)
{
  byte *base = (byte *)stringpool;

  uint32_t stringCount = stringpool->stringCount;
  uint32_t *stringOffsets = (uint32_t *)(base + stringpool->header.headerSize);
  byte *stringData = base + stringpool->stringsStart;

  if(ref.index == ~0U)
    return "";

  if(ref.index >= stringCount)
    return "__invalid_string__";

  byte *strdata = stringData + stringOffsets[ref.index];

  // strdata now points at len characters of string. Check if it's UTF-8 or UTF-16
  if((stringpool->flags & ResStringPool_header::UTF8_FLAG) == 0)
  {
    uint16_t *str = (uint16_t *)strdata;

    uint32_t len = *(str++);

    // see comment above ResStringPool_header - if high bit is set, then this string is >32767
    // characters, so it's followed by another uint16_t with the low word
    if(len & 0x8000)
    {
      len &= 0x7fff;
      len <<= 16;
      len |= *(str++);
    }

    std::wstring wstr;

    // wchar_t isn't always 2 bytes, so we iterate over the uint16_t and cast.
    for(uint32_t i = 0; i < len; i++)
      wstr.push_back(wchar_t(str[i]));

    return StringFormat::Wide2UTF8(wstr);
  }
  else
  {
    byte *str = (byte *)strdata;

    uint32_t len = *(str++);

    // the length works similarly for UTF-8 data but with single bytes instead of uint16s.
    if(len & 0x80)
    {
      len &= 0x7f;
      len <<= 8;
      len |= *(str++);
    }

    // the length is encoded twice. I can only assume to preserve uint16 size although I don't see
    // why that would be necessary - it can't be fully backwards compatible even with the alignment
    // except with readers that ignore the length entirely and look for trailing NULLs.
    if(len < 0x80)
      str++;
    else
      str += 2;

    return std::string((char *)str, (char *)(str + len));
  }
}

void ShiftStringPoolValue(ResStringPool_ref &ref, uint32_t insertedLocation)
{
  // if we found our added attribute, then set the index here (otherwise we'd remap it with the
  // others!)
  if(ref.index == addingStringIndex)
    ref.index = insertedLocation;
  else if(ref.index != ~0U && ref.index >= insertedLocation)
    ref.index++;
}

void ShiftStringPoolValue(Res_value &val, uint32_t insertedLocation)
{
  if(val.dataType == Res_value::DataType::String && val.data >= insertedLocation)
    val.data++;
}

template <typename T>
void InsertBytes(std::vector<byte> &bytes, byte *pos, const T &data)
{
  byte *start = &bytes[0];
  byte *byteData = (byte *)&data;

  size_t offs = pos - start;

  bytes.insert(bytes.begin() + offs, byteData, byteData + sizeof(T));
}

template <>
void InsertBytes(std::vector<byte> &bytes, byte *pos, const std::vector<byte> &data)
{
  byte *start = &bytes[0];

  size_t offs = pos - start;

  bytes.insert(bytes.begin() + offs, data.begin(), data.end());
}

bool PatchManifest(std::vector<byte> &manifestBytes)
{
  // Whether to insert a new string & resource ID at the start or end of the resource map table. I
  // can't find anything that indicates there is any required ordering to these, so either should be
  // valid.
  const bool insertStringAtStart = false;

  // reserve room for our modifications up front, to be sure that if we do make them we'll never
  // invalidate any pointers. We could add:
  manifestBytes.reserve(
      manifestBytes.size() +
      // - a string (uint32 offset, uint16 length and string characters (possibly
      //   in UTF-16) including NULL)
      sizeof(uint32_t) + sizeof(uint16_t) + sizeof("debuggable") * 2 +
      // - a resource ID mapping (one uint32)
      sizeof(uint32_t) +
      // - an attribute (ResXMLTree_attribute)
      sizeof(ResXMLTree_attribute) +
      // and we add 16 bytes more just for a safety margin with any necessary padding
      16);

  // save the capacity so we can check we never resize
  size_t capacity = manifestBytes.capacity();

  byte *start = &manifestBytes.front();

  byte *cur = start;

  ResChunk_header *xmlroot = (ResChunk_header *)cur;

  if((byte *)(xmlroot + 1) > &manifestBytes.back())
  {
    RDCERR("Manifest is truncated, %zu bytes doesn't contain full XML header", manifestBytes.size());
    return false;
  }

  if(xmlroot->type != ResType::XML)
  {
    RDCERR("XML Header is malformed, type is %u expected %u", xmlroot->type, ResType::XML);
    return false;
  }

  if(xmlroot->headerSize != sizeof(*xmlroot))
  {
    RDCERR("XML Header is malformed, header size is reported as %u but expected %u",
           xmlroot->headerSize, sizeof(*xmlroot));
    return false;
  }

  // this isn't necessarily fatal, but it is unexpected.
  if(xmlroot->size != manifestBytes.size())
    RDCWARN("XML header is malformed, size is reported as %u but %zu bytes found", xmlroot->size,
            manifestBytes.size());

  cur += xmlroot->headerSize;

  ResStringPool_header *stringpool = (ResStringPool_header *)cur;

  if(stringpool->header.type != ResType::StringPool)
  {
    RDCERR("Manifest format is unsupported, expected string pool but got %u",
           stringpool->header.type);
    return false;
  }

  if(stringpool->header.headerSize != sizeof(*stringpool))
  {
    RDCERR("String pool is malformed, header size is reported as %u but expected %u",
           stringpool->header.headerSize, sizeof(*stringpool));
    return false;
  }

  if(cur + stringpool->header.size > &manifestBytes.back())
  {
    RDCERR("String pool is truncated, expected %u more bytes but only have %u",
           stringpool->header.size, uint32_t(&manifestBytes.back() - cur));
    return false;
  }

  cur += stringpool->header.size;

  ResChunk_header *resMap = (ResChunk_header *)cur;

  if(resMap->type != ResType::ResourceMap)
  {
    RDCERR("Manifest format is unsupported, expected resource table but got %u", resMap->type);
    return false;
  }

  if(resMap->headerSize != sizeof(*resMap))
  {
    RDCERR("Resource map is malformed, header size is reported as %u but expected %u",
           resMap->headerSize, sizeof(*resMap));
    return false;
  }

  if(cur + resMap->size > &manifestBytes.back())
  {
    RDCERR("Resource map is truncated, expected %u more bytes but only have %u", resMap->size,
           uint32_t(&manifestBytes.back() - cur));
    return false;
  }

  uint32_t *resourceMapping = (uint32_t *)(cur + resMap->headerSize);
  uint32_t resourceMappingCount = (resMap->size - resMap->headerSize) / sizeof(uint32_t);

  cur += resMap->size;

  bool stringAdded = false;

  // now chunks will come along. There will likely first be a namespace begin, then XML tag open and
  // close. Since the <application> tag is only valid in one place in the XML we can just continue
  // iterating until we find it - we don't actually need to care about the structure of the XML
  // since we are identifying a unique tag and adding one attribute.
  while(cur < &manifestBytes.back())
  {
    ResChunk_header *node = (ResChunk_header *)cur;

    if(node->type != ResType::StartElement)
    {
      cur += node->size;
      continue;
    }

    ResXMLTree_attrExt *startElement = (ResXMLTree_attrExt *)(cur + node->headerSize);

    std::string name = GetStringPoolValue(stringpool, startElement->name);

    if(name != "application")
    {
      cur += node->size;
      continue;
    }

    // found the application tag! Now search its attribtues to see if it already has a debuggable
    // attribute (that might be set explicitly to false instead of defaulting)
    if(startElement->attributeSize != sizeof(ResXMLTree_attribute))
    {
      RDCWARN("Declared attribute size %u doesn't match what we expect %zu",
              startElement->attributeSize, sizeof(ResXMLTree_attribute));
    }

    if(startElement->attributeStart != sizeof(*startElement))
    {
      RDCWARN("Declared attribute start offset %u doesn't match what we expect %zu",
              startElement->attributeStart, sizeof(*startElement));
    }

    byte *attributesStart = cur + node->headerSize + startElement->attributeStart;

    bool found = false;

    for(uint32_t i = 0; i < startElement->attributeCount; i++)
    {
      ResXMLTree_attribute *attribute =
          (ResXMLTree_attribute *)(attributesStart + startElement->attributeSize * i);

      std::string attr = GetStringPoolValue(stringpool, attribute->name);

      if(attr != "debuggable")
        continue;

      uint32_t resourceId = 0;

      if(attribute->name.index < resourceMappingCount)
      {
        resourceId = resourceMapping[attribute->name.index];
      }
      else
      {
        RDCWARN("Found debuggable attribute, but it's not linked to any resource ID");

        if(attribute->typedValue.dataType != Res_value::DataType::Boolean)
        {
          RDCERR("Found debuggable attribute that isn't boolean typed! Not modifying");
          return false;
        }
        else
        {
          RDCDEBUG("Setting non-resource ID debuggable attribute to true");
          attribute->typedValue.data = ~0U;

          if(attribute->rawValue.index != ~0U)
          {
            RDCWARN("attribute has raw value '%s' which we aren't patching",
                    GetStringPoolValue(stringpool, attribute->rawValue).c_str());
          }

          // we'll still add a debuggable attribute that is resource ID linked, so we don't mark the
          // attribute as found and break out of the loop yet
          continue;
        }
      }

      if(resourceId != debuggableResourceId)
      {
        RDCERR(
            "Found debuggable attribute mapped to resource %x, not %x as we expect! Not modifying",
            resourceId, debuggableResourceId);
        return false;
      }

      RDCDEBUG("Found debuggable attribute.");

      if(attribute->typedValue.dataType != Res_value::DataType::Boolean)
      {
        RDCERR("Found debuggable attribute that isn't boolean typed! Not modifying");
        return false;
      }
      else
      {
        RDCDEBUG("Setting resource ID debuggable attribute to true");
        attribute->typedValue.data = ~0U;

        if(attribute->rawValue.index != ~0U)
        {
          RDCWARN("attribute has raw value '%s' which we aren't patching",
                  GetStringPoolValue(stringpool, attribute->rawValue).c_str());
        }
      }

      found = true;
      break;
    }

    if(found)
      break;

    if(startElement->attributeSize != sizeof(ResXMLTree_attribute))
    {
      RDCERR("Unexpected attribute size %u, can't add missing attribute",
             startElement->attributeSize);
      return false;
    }

    // default to an invalid value (the manifest would have to be GBs to have this as a valid string
    // index.
    // If we don't find the existing string to use, then this will be remapped below when we're
    // remapping all the other indices.
    ResStringPool_ref stringIndex = {addingStringIndex};

    // we didn't find the attribute, so we need to search for the appropriate string, add it if not
    // there, and add the attribute.
    for(uint32_t i = 0; i < resourceMappingCount; i++)
    {
      if(resourceMapping[i] == debuggableResourceId)
      {
        std::string str = GetStringPoolValue(stringpool, {i});

        if(str != "debuggable")
        {
          RDCWARN("Found debuggable resource ID, but it was linked to string '%s' not 'debuggable'",
                  str.c_str());
          continue;
        }

        stringIndex = {i};
      }
    }

    // declare the debuggable attribute
    ResXMLTree_attribute debuggable;
    debuggable.ns.index = ~0U;
    debuggable.name = stringIndex;
    debuggable.rawValue.index = ~0U;
    debuggable.typedValue.size = sizeof(Res_value);
    debuggable.typedValue.res0 = 0;
    debuggable.typedValue.dataType = Res_value::DataType::Boolean;
    debuggable.typedValue.data = ~0U;

    // search the stringpool for the schema, it should be there already.
    for(uint32_t i = 0; i < stringpool->stringCount; i++)
    {
      std::string val = GetStringPoolValue(stringpool, {i});
      if(val == "http://schemas.android.com/apk/res/android")
      {
        debuggable.ns.index = i;
        break;
      }
    }

    if(debuggable.ns.index == ~0U)
      RDCWARN("Couldn't find android schema, declaring attribute without schema");

    // it seems the attribute must be added so that the attributes are sorted in resource ID order.
    // We assume the attributes are already sorted according to this order, so we insert at the
    // index of the first attribute we encounter with either no resource ID (i.e. if we only
    // encountered lower resource IDs then we hit a non-resource ID attribute), or a higher resource
    // ID than ours (in which case we're inserting it in the right place).
    uint32_t attributeInsertIndex = 0;

    for(uint32_t i = 0; i < startElement->attributeCount; i++)
    {
      ResXMLTree_attribute *attr =
          (ResXMLTree_attribute *)(attributesStart + startElement->attributeSize * i);

      if(attr->name.index >= resourceMappingCount)
      {
        attributeInsertIndex = i;
        RDCDEBUG("Inserting attribute before %s, with no resource ID",
                 GetStringPoolValue(stringpool, attr->name).c_str());
        break;
      }

      uint32_t resourceId = resourceMapping[attr->name.index];

      if(resourceId >= debuggableResourceId)
      {
        attributeInsertIndex = i;
        RDCDEBUG("Inserting attribute before %s, with resource ID %x",
                 GetStringPoolValue(stringpool, attr->name).c_str(), resourceId);
        break;
      }

      RDCDEBUG("Skipping past attribute %s, with resource ID %x",
               GetStringPoolValue(stringpool, attr->name).c_str(), resourceId);
    }

    InsertBytes(manifestBytes, attributesStart + startElement->attributeSize * attributeInsertIndex,
                debuggable);

    // update header
    startElement->attributeCount++;
    node->size += sizeof(ResXMLTree_attribute);

    stringAdded = (stringIndex.index == addingStringIndex);

    break;
  }

  // if we added the string, we need to update the string pool and resource map, then finally update
  // all stringrefs in the nodes. We do this in reverse order so that we don't invalidate pointers
  // with insertions
  if(stringAdded)
  {
    uint32_t insertIdx = insertStringAtStart ? 0 : resourceMappingCount;

    // add to the resource map
    {
      if(insertIdx == 0)
        InsertBytes(manifestBytes, (byte *)resMap + resMap->headerSize, debuggableResourceId);
      else
        InsertBytes(manifestBytes, (byte *)resMap + resMap->size, debuggableResourceId);
      resMap->size += sizeof(uint32_t);
    }

    // add to the string pool
    {
      // add the offset
      stringpool->header.size += sizeof(uint32_t);
      stringpool->stringCount++;
      stringpool->stringsStart += sizeof(uint32_t);
      // if we're adding a string we don't bother to do it sorted, so remove the sorted flag
      stringpool->flags =
          ResStringPool_header::StringFlags(stringpool->flags & ~ResStringPool_header::SORTED_FLAG);

      byte *base = (byte *)stringpool;

      uint32_t *stringOffsets = (uint32_t *)(base + stringpool->header.headerSize);

      // we duplicate the offset at the position we're inserting. Then when we fix up all the other
      // offsets the duplicated one shifts by the right amount.
      InsertBytes(manifestBytes,
                  (byte *)stringpool + stringpool->header.headerSize + sizeof(uint32_t) * insertIdx,
                  stringOffsets[insertIdx]);

      uint32_t shift = 0;

      byte *stringData = (byte *)stringpool + stringpool->stringsStart;

      // insert the string, with length prefix and trailing NULL
      if(stringpool->flags & ResStringPool_header::UTF8_FLAG)
      {
        std::vector<byte> bytes = {0xA, 0xA, 'd', 'e', 'b', 'u', 'g', 'g', 'a', 'b', 'l', 'e', 0};
        shift = (uint32_t)bytes.size();

        InsertBytes(manifestBytes, stringData + stringOffsets[insertIdx], bytes);
      }
      else
      {
        std::vector<byte> bytes = {0xA, 0x0, 'd', 0, 'e', 0, 'b', 0, 'u', 0, 'g', 0,
                                   'g', 0,   'a', 0, 'b', 0, 'l', 0, 'e', 0, 0,   0};
        shift = (uint32_t)bytes.size();

        InsertBytes(manifestBytes, stringData + stringOffsets[insertIdx], bytes);
      }

      // account for added string
      stringpool->header.size += shift;

      // shift all the offsets *after* the string we inserted (we inserted precisely at that
      // offset).
      for(uint32_t i = insertIdx + 1; i < stringpool->stringCount; i++)
        stringOffsets[i] += shift;

      // if the stringpool isn't integer aligned, add padding bytes
      uint32_t alignedSize = AlignUp4(stringpool->header.size);

      if(alignedSize > stringpool->header.size)
      {
        uint32_t paddingLen = alignedSize - stringpool->header.size;

        RDCDEBUG("Inserting %u padding bytes to align %u up to %u", paddingLen,
                 stringpool->header.size, alignedSize);

        InsertBytes(manifestBytes, base + stringpool->header.size,
                    std::vector<byte>((size_t)paddingLen, 0));

        stringpool->header.size += paddingLen;
      }
    }

    // now iterate over all nodes and fixup any stringrefs pointing after our insert point
    cur = start + xmlroot->headerSize;
    // skip string pool
    cur += ((ResChunk_header *)cur)->size;
    // skip resource map
    cur += ((ResChunk_header *)cur)->size;

    while(cur < &manifestBytes.back())
    {
      ResXMLTree_node *node = (ResXMLTree_node *)cur;

      if(node->header.headerSize != sizeof(*node))
        RDCWARN("Headersize was reported as %u, but we expected ResXMLTree_node size %zu",
                node->header.headerSize, sizeof(*node));

      ShiftStringPoolValue(node->comment, insertIdx);

      switch(node->header.type)
      {
        // namespace start and end are identical
        case ResType::NamespaceStart:
        case ResType::NamespaceEnd:
        {
          ResXMLTree_namespaceExt *ns = (ResXMLTree_namespaceExt *)(cur + node->header.headerSize);

          ShiftStringPoolValue(ns->prefix, insertIdx);
          ShiftStringPoolValue(ns->uri, insertIdx);
          break;
        }
        case ResType::EndElement:
        {
          ResXMLTree_endElementExt *endElement =
              (ResXMLTree_endElementExt *)(cur + node->header.headerSize);

          ShiftStringPoolValue(endElement->ns, insertIdx);
          ShiftStringPoolValue(endElement->name, insertIdx);
          break;
        }
        case ResType::CDATA:
        {
          ResXMLTree_cdataExt *cdata = (ResXMLTree_cdataExt *)(cur + node->header.headerSize);

          ShiftStringPoolValue(cdata->data, insertIdx);
          ShiftStringPoolValue(cdata->typedData, insertIdx);
          break;
        }
        case ResType::StartElement:
        {
          ResXMLTree_attrExt *startElement = (ResXMLTree_attrExt *)(cur + node->header.headerSize);

          ShiftStringPoolValue(startElement->ns, insertIdx);
          ShiftStringPoolValue(startElement->name, insertIdx);

          // update attributes
          byte *attributesStart = cur + node->header.headerSize + startElement->attributeStart;

          for(uint32_t i = 0; i < startElement->attributeCount; i++)
          {
            ResXMLTree_attribute *attr =
                (ResXMLTree_attribute *)(attributesStart + startElement->attributeSize * i);

            ShiftStringPoolValue(attr->ns, insertIdx);
            ShiftStringPoolValue(attr->name, insertIdx);
            ShiftStringPoolValue(attr->rawValue, insertIdx);
            ShiftStringPoolValue(attr->typedValue, insertIdx);
          }
          break;
        }
        default:
          RDCERR("Unhandled chunk %x, can't patch stringpool references", node->header.type);
          return false;
      }

      cur += node->header.size;
    }
  }

  xmlroot->size = (uint32_t)manifestBytes.size();

  if(manifestBytes.capacity() > capacity)
  {
    RDCERR(
        "manifest vector resized during patching! Update reserve() at the start of "
        "Android::PatchManifest");
  }

  return true;
}
};