/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2021 Baldur Karlsson
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

#include "aosp/android_manifest.h"
#include "core/core.h"
#include "strings/string_utils.h"
#include "android_utils.h"

const uint32_t debuggableResourceId = 0x0101000f;
const uint32_t addingStringIndex = 0x8b8b8b8b;

// the string pool always immediately follows the XML header, which is just an empty header.
const size_t stringpoolOffset = sizeof(ResChunk_header);

namespace Android
{
template <typename T>
void SetFromBytes(bytebuf &bytes, size_t offs, const T &t)
{
  T *ptr = (T *)(bytes.data() + offs);
  memcpy(ptr, &t, sizeof(T));
}

template <typename T>
T GetFromBytes(bytebuf &bytes, size_t offs)
{
  T ret;
  T *ptr = (T *)(bytes.data() + offs);
  memcpy(&ret, ptr, sizeof(T));
  return ret;
}

template <typename T>
void InsertBytes(bytebuf &bytes, size_t offs, const T &data)
{
  byte *byteData = (byte *)&data;

  bytes.insert(offs, byteData, sizeof(T));
}

template <>
void InsertBytes(bytebuf &bytes, size_t offs, const bytebuf &data)
{
  bytes.insert(offs, data);
}

rdcstr GetStringPoolValue(bytebuf &bytes, ResStringPool_ref ref)
{
  ResStringPool_header stringpool = GetFromBytes<ResStringPool_header>(bytes, stringpoolOffset);

  byte *base = bytes.data() + stringpoolOffset;

  uint32_t stringCount = stringpool.stringCount;
  uint32_t *stringOffsets = (uint32_t *)(base + stringpool.header.headerSize);
  byte *stringData = base + stringpool.stringsStart;

  if(ref.index == ~0U)
    return "";

  if(ref.index >= stringCount)
    return "__invalid_string__";

  byte *strdata = stringData + stringOffsets[ref.index];

  // strdata now points at len characters of string. Check if it's UTF-8 or UTF-16
  if((stringpool.flags & ResStringPool_header::UTF8_FLAG) == 0)
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

    rdcwstr wstr(len);

    // wchar_t isn't always 2 bytes, so we iterate over the uint16_t and cast.
    for(uint32_t i = 0; i < len; i++)
      wstr[i] = wchar_t(str[i]);

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

    return rdcstr((char *)str, len);
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

bool PatchManifest(bytebuf &manifestBytes)
{
  if(manifestBytes.size() < sizeof(ResChunk_header))
  {
    RDCERR("Manifest is truncated, %zu bytes doesn't contain full XML header", manifestBytes.size());
    return false;
  }

  size_t cur = 0;

  ResChunk_header xmlroot = GetFromBytes<ResChunk_header>(manifestBytes, cur);

  if(xmlroot.type != ResType::XML)
  {
    RDCERR("XML Header is malformed, type is %u expected %u", xmlroot.type, ResType::XML);
    return false;
  }

  if(xmlroot.headerSize != sizeof(xmlroot))
  {
    RDCERR("XML Header is malformed, header size is reported as %u but expected %u",
           xmlroot.headerSize, sizeof(xmlroot));
    return false;
  }

  // this isn't necessarily fatal, but it is unexpected.
  if(xmlroot.size != manifestBytes.size())
    RDCWARN("XML header is malformed, size is reported as %u but %zu bytes found", xmlroot.size,
            manifestBytes.size());

  cur += xmlroot.headerSize;

  ResStringPool_header stringpool = GetFromBytes<ResStringPool_header>(manifestBytes, cur);

  if(stringpool.header.type != ResType::StringPool)
  {
    RDCERR("Manifest format is unsupported, expected string pool but got %u", stringpool.header.type);
    return false;
  }

  if(stringpool.header.headerSize != sizeof(stringpool))
  {
    RDCERR("String pool is malformed, header size is reported as %u but expected %u",
           stringpool.header.headerSize, sizeof(stringpool));
    return false;
  }

  if(cur + stringpool.header.size > manifestBytes.size())
  {
    RDCERR("String pool is truncated, expected %u more bytes but only have %u",
           stringpool.header.size, manifestBytes.size() - cur);
    return false;
  }

  cur += stringpool.header.size;

  ResChunk_header resMap = GetFromBytes<ResChunk_header>(manifestBytes, cur);
  const size_t resMapOffset = cur;

  if(resMap.type != ResType::ResourceMap)
  {
    RDCERR("Manifest format is unsupported, expected resource table but got %u", resMap.type);
    return false;
  }

  if(resMap.headerSize != sizeof(resMap))
  {
    RDCERR("Resource map is malformed, header size is reported as %u but expected %u",
           resMap.headerSize, sizeof(resMap));
    return false;
  }

  if(cur + resMap.size > manifestBytes.size())
  {
    RDCERR("Resource map is truncated, expected %u more bytes but only have %u", resMap.size,
           manifestBytes.size() - cur);
    return false;
  }

  const uint32_t resourceMappingCount = (resMap.size - resMap.headerSize) / sizeof(uint32_t);
  const rdcarray<uint32_t> resourceMapping(
      (const uint32_t *)(manifestBytes.data() + cur + resMap.headerSize), resourceMappingCount);

  cur += resMap.size;

  bool stringAdded = false;

  // now chunks will come along. There will likely first be a namespace begin, then XML tag open and
  // close. Since the <application> tag is only valid in one place in the XML we can just continue
  // iterating until we find it - we don't actually need to care about the structure of the XML
  // since we are identifying a unique tag and adding one attribute.
  while(cur < manifestBytes.size())
  {
    ResChunk_header node = GetFromBytes<ResChunk_header>(manifestBytes, cur);

    if(node.type != ResType::StartElement)
    {
      cur += node.size;
      continue;
    }

    ResXMLTree_attrExt startElement =
        GetFromBytes<ResXMLTree_attrExt>(manifestBytes, cur + node.headerSize);

    rdcstr name = GetStringPoolValue(manifestBytes, startElement.name);

    if(name != "application")
    {
      cur += node.size;
      continue;
    }

    // found the application tag! Now search its attribtues to see if it already has a debuggable
    // attribute (that might be set explicitly to false instead of defaulting)
    if(startElement.attributeSize != sizeof(ResXMLTree_attribute))
    {
      RDCWARN("Declared attribute size %u doesn't match what we expect %zu",
              startElement.attributeSize, sizeof(ResXMLTree_attribute));
    }

    if(startElement.attributeStart != sizeof(startElement))
    {
      RDCWARN("Declared attribute start offset %u doesn't match what we expect %zu",
              startElement.attributeStart, sizeof(startElement));
    }

    const size_t attributeStartOffset = cur + node.headerSize + startElement.attributeStart;
    byte *attributesStart = manifestBytes.data() + attributeStartOffset;

    bool found = false;

    for(uint32_t i = 0; i < startElement.attributeCount; i++)
    {
      ResXMLTree_attribute *attribute =
          (ResXMLTree_attribute *)(attributesStart + startElement.attributeSize * i);

      rdcstr attr = GetStringPoolValue(manifestBytes, attribute->name);

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
                    GetStringPoolValue(manifestBytes, attribute->rawValue).c_str());
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
                  GetStringPoolValue(manifestBytes, attribute->rawValue).c_str());
        }
      }

      found = true;
      break;
    }

    if(found)
      break;

    if(startElement.attributeSize != sizeof(ResXMLTree_attribute))
    {
      RDCERR("Unexpected attribute size %u, can't add missing attribute", startElement.attributeSize);
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
        rdcstr str = GetStringPoolValue(manifestBytes, {i});

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
    for(uint32_t i = 0; i < stringpool.stringCount; i++)
    {
      rdcstr val = GetStringPoolValue(manifestBytes, {i});
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

    for(uint32_t i = 0; i < startElement.attributeCount; i++)
    {
      ResXMLTree_attribute *attr =
          (ResXMLTree_attribute *)(attributesStart + startElement.attributeSize * i);

      if(attr->name.index >= resourceMappingCount)
      {
        attributeInsertIndex = i;
        RDCDEBUG("Inserting attribute before %s, with no resource ID",
                 GetStringPoolValue(manifestBytes, attr->name).c_str());
        break;
      }

      uint32_t resourceId = resourceMapping[attr->name.index];

      if(resourceId >= debuggableResourceId)
      {
        attributeInsertIndex = i;
        RDCDEBUG("Inserting attribute before %s, with resource ID %x",
                 GetStringPoolValue(manifestBytes, attr->name).c_str(), resourceId);
        break;
      }

      RDCDEBUG("Skipping past attribute %s, with resource ID %x",
               GetStringPoolValue(manifestBytes, attr->name).c_str(), resourceId);
    }

    InsertBytes(manifestBytes,
                attributeStartOffset + startElement.attributeSize * attributeInsertIndex, debuggable);

    // update header
    node.size += sizeof(ResXMLTree_attribute);
    SetFromBytes(manifestBytes, cur, node);

    startElement.attributeCount++;
    SetFromBytes(manifestBytes, cur + node.headerSize, startElement);

    stringAdded = (stringIndex.index == addingStringIndex);

    break;
  }

  // if we added the string, we need to update the string pool and resource map, then finally update
  // all stringrefs in the nodes. We do this in reverse order so that we don't invalidate pointers
  // with insertions
  if(stringAdded)
  {
    uint32_t insertIdx = resourceMappingCount;

    // add to the resource map first because it's after the string pool, that way we don't have to
    // account for string pool modifications in resMapOffset
    {
      InsertBytes(manifestBytes, resMapOffset + resMap.size, debuggableResourceId);
      resMap.size += sizeof(uint32_t);
      SetFromBytes(manifestBytes, resMapOffset, resMap);
    }

    // add to the string pool next
    {
      // add the offset
      stringpool.header.size += sizeof(uint32_t);
      stringpool.stringCount++;
      stringpool.stringsStart += sizeof(uint32_t);
      // if we're adding a string we don't bother to do it sorted, so remove the sorted flag
      stringpool.flags =
          ResStringPool_header::StringFlags(stringpool.flags & ~ResStringPool_header::SORTED_FLAG);

      size_t stringpoolStringOffsetsOffset = stringpoolOffset + stringpool.header.headerSize;

      // we insert a zero offset at the position we're inserting. Then when we fix up that and all
      // subsequent offsets
      InsertBytes(manifestBytes, stringpoolStringOffsetsOffset + sizeof(uint32_t) * insertIdx,
                  uint32_t(0));

      bytebuf stringbytes;

      // construct the string, with length prefix and trailing NULL
      if(stringpool.flags & ResStringPool_header::UTF8_FLAG)
      {
        stringbytes = {0xA, 0xA, 'd', 'e', 'b', 'u', 'g', 'g', 'a', 'b', 'l', 'e', 0};
      }
      else
      {
        stringbytes = {0xA, 0x0, 'd', 0, 'e', 0, 'b', 0, 'u', 0, 'g', 0,
                       'g', 0,   'a', 0, 'b', 0, 'l', 0, 'e', 0, 0,   0};
      }

      // account for added string
      stringpool.header.size += stringbytes.count();

      // shift all the offsets *after* the string we inserted (we inserted precisely at that
      // offset).
      uint32_t *stringOffsets = (uint32_t *)(manifestBytes.data() + stringpoolStringOffsetsOffset);

      // the one we inserted will be inserted at the offset of whichever was previously at that
      // index (which is now one further on)
      stringOffsets[insertIdx] = stringOffsets[insertIdx + 1];

      for(uint32_t i = insertIdx + 1; i < stringpool.stringCount; i++)
        stringOffsets[i] += stringbytes.count();

      // now insert the string bytes
      InsertBytes(manifestBytes,
                  stringpoolOffset + stringpool.stringsStart + stringOffsets[insertIdx], stringbytes);

      // if the stringpool isn't integer aligned, add padding bytes
      uint32_t alignedSize = AlignUp4(stringpool.header.size);

      if(alignedSize > stringpool.header.size)
      {
        uint32_t paddingLen = alignedSize - stringpool.header.size;

        RDCDEBUG("Inserting %u padding bytes to align %u up to %u", paddingLen,
                 stringpool.header.size, alignedSize);

        bytebuf padding;
        padding.resize(paddingLen);

        InsertBytes(manifestBytes, stringpoolOffset + stringpool.header.size, padding);

        stringpool.header.size += paddingLen;
      }

      // write the updated stringpool
      SetFromBytes(manifestBytes, stringpoolOffset, stringpool);
    }

    // now iterate over all nodes and fixup any stringrefs pointing after our insert point
    byte *ptr = manifestBytes.data() + xmlroot.headerSize;
    // skip string pool, whatever size it is now
    ptr += ((ResChunk_header *)ptr)->size;
    // skip resource map, whatever size it is now
    ptr += ((ResChunk_header *)ptr)->size;

    while(ptr < manifestBytes.end())
    {
      ResXMLTree_node *node = (ResXMLTree_node *)ptr;

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
          ResXMLTree_namespaceExt *ns = (ResXMLTree_namespaceExt *)(ptr + node->header.headerSize);

          ShiftStringPoolValue(ns->prefix, insertIdx);
          ShiftStringPoolValue(ns->uri, insertIdx);
          break;
        }
        case ResType::EndElement:
        {
          ResXMLTree_endElementExt *endElement =
              (ResXMLTree_endElementExt *)(ptr + node->header.headerSize);

          ShiftStringPoolValue(endElement->ns, insertIdx);
          ShiftStringPoolValue(endElement->name, insertIdx);
          break;
        }
        case ResType::CDATA:
        {
          ResXMLTree_cdataExt *cdata = (ResXMLTree_cdataExt *)(ptr + node->header.headerSize);

          ShiftStringPoolValue(cdata->data, insertIdx);
          ShiftStringPoolValue(cdata->typedData, insertIdx);
          break;
        }
        case ResType::StartElement:
        {
          ResXMLTree_attrExt *startElement = (ResXMLTree_attrExt *)(ptr + node->header.headerSize);

          ShiftStringPoolValue(startElement->ns, insertIdx);
          ShiftStringPoolValue(startElement->name, insertIdx);

          // update attributes
          byte *attributesStart = ptr + node->header.headerSize + startElement->attributeStart;

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

      ptr += node->header.size;
    }
  }

  xmlroot.size = (uint32_t)manifestBytes.size();
  SetFromBytes(manifestBytes, 0, xmlroot);

  return true;
}
};
