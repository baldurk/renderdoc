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

#include "DescriptorViewer.h"
#include <float.h>
#include <QFontDatabase>
#include "Code/QRDUtils.h"
#include "Code/Resources.h"
#include "toolwindowmanager/ToolWindowManager.h"
#include "ui_DescriptorViewer.h"

struct ButtonTag
{
  ButtonTag() = default;
  ButtonTag(bool buffer, const Descriptor &descriptor)
      : valid(true), buffer(buffer), descriptor(descriptor)
  {
  }
  ButtonTag(ResourceId heap) : valid(true), buffer(false), heap(heap) {}

  // all constructe tags compare equal so this value can contain data but still be used to enable buttons
  bool operator==(const ButtonTag &o) const { return valid && o.valid; }
  bool operator<(const ButtonTag &o) const { return valid < o.valid; }

  bool valid = false;
  bool buffer = false;
  Descriptor descriptor;
  ResourceId heap;
};

Q_DECLARE_METATYPE(ButtonTag);

class DescriptorItemModel : public QAbstractItemModel
{
public:
  DescriptorItemModel(ICaptureContext &ctx, DescriptorViewer &view, QObject *parent)
      : QAbstractItemModel(parent), m_Ctx(ctx), m_View(view)
  {
    m_API = m_Ctx.APIProps().pipelineType;
  }

  void refresh()
  {
    emit beginResetModel();

    if(m_View.m_D3D12RootSig.parameters.size() + 1 >= ParameterIndexMask)
    {
      qCritical() << "Too many root signature parameters, will be clipped";

      for(const D3D12Pipe::RootParam &param : m_View.m_D3D12RootSig.parameters)
        if(param.tableRanges.size() + 1 >= TableIndexMask)
          qCritical() << "Too many tables in parameter, will be clipped";
    }

    emit endResetModel();
  }

  QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override
  {
    if(row < 0 || row >= rowCount(parent) || column < 0 || column >= columnCount())
      return QModelIndex();

    // root signature has more levels of nesting so is more complex
    //
    // for ease, each (x, y) tuple is a QModelIndex(row=x, column, id=y). Column is omitted as it's
    // not important. We encode the levels of array indexing in the id assuming reasonable packing.
    //
    // For packing:
    //   param is at very most 63 because of root signature limits so it needs 6 bits
    //   tables can be at most 1 million long because of descriptor heap limits
    //   tables could be large but it's more likely to have a few tables that are very large
    //
    // With 64-bit ids we have plenty of bits.
    // For 32-bit we take 1 bit for parameter or sampler, 6 bits for param, 20 bits for descriptor,
    // and the remaining 5 bits for table index. We don't expect this code to run in 32-bit typically
    // (or ever) so it's a best-effort. For this case we also set the 'ParameterFlag' id specially to
    // avoid needing to take more bits. We use 1-based indices to be able to distinguish parent from
    // child.
    //
    // QModelIndex() <= root
    //   (ParametersRootNode, FixedNode)                               <= "Parameters"
    //     (param, ParameterFlag)                                      <= params[param]
    //       (0, ParameterData | param+1)                                <= params[param].data[0]
    //       (1..., ParameterData | param+1)                             <= params[param].data[1]... etc
    //                                                                    more for root consts/desc
    //
    //       (table, ParameterData | param+1)                          <= param.tables[table]
    //                                                                    with table >= fixedDataCount
    //         (0, ParameterData | param+1 | table+1)                  <= table.data[0]
    //         (1..., ParameterData | param+1 | table+1)               <= table.data[1]...
    //         (desc, ParameterData | param+1 | table+1)               <= table.descriptor[desc]
    //           (dataIdx, ParameterData | param+1 | table+1 | desc+1) <= desc.data[dataIdx]
    //
    //   (StaticSamplersRootNode, FixedNode)                           <= "StaticSamplers"
    //     (sampIdx, StaticSamplerData)                                <= samplers[sampIdx]
    //       (dataIdx, StaticSamplerData | sampIdx+1)                  <= samplers[sampIdx].data[dataIdx]
    //   (DescriptorHeapNode, FixedNode)                               <= "Descriptor Heap 1234"
    if(m_View.m_D3D12RootSig.resourceId != ResourceId())
    {
      // children of the root are the two fixed nodes
      if(parent == QModelIndex())
      {
        return createIndex(row, column, FixedNode);
      }

      // children of the fixed nodes are parameters or static samplers
      if(parent.internalId() == FixedNode)
      {
        if(parent.row() == ParametersRootNode)
          return createIndex(row, column, ParameterFlag);

        if(parent.row() == StaticSamplersRootNode)
          return createIndex(row, column, StaticSamplerData);

        // other root entries are descriptor heaps
        return QModelIndex();
      }

      // children of static samplers just add on their index (+1 to distinguish from the plain node)
      if(parent.internalId() == StaticSamplerData)
      {
        return createIndex(row, column, StaticSamplerData | (parent.row() + 1));
      }

      // other rows that aren't parameter data are static sampler properties themselves and have no children
      if((parent.internalId() & ParameterData) == 0)
        return QModelIndex();

      // children of a parameter node mask on the index into their id
      if(parent.internalId() == ParameterFlag)
      {
        return createIndex(row, column, Encode({uint8_t(parent.row() + 1), 0, 0}));
      }

      RootIdx parentIdx = Decode(parent.internalId());

      // should not be possible, the root is ParameterFlag and then after that we encode with
      // 1-based indexing so the values are not 0.
      if(parentIdx.parameter == 0)
        return QModelIndex();

      // this is a child of a parameter node, encode the range index from the parent's row
      if(parentIdx.range == 0 && parentIdx.descriptor == 0)
      {
        // the fixed parameters do not have children
        if(parent.row() < TableParameterFixedRowCount)
          return QModelIndex();

        return createIndex(row, column,
                           Encode({parentIdx.parameter,
                                   uint16_t(parent.row() - TableParameterFixedRowCount + 1), 0}));
      }

      // this is the child of a table node, encode the descriptor index from the parent's row
      if(parentIdx.descriptor == 0)
      {
        // the fixed parameters do not have children
        if(parent.row() < RangeFixedRowCount)
          return QModelIndex();

        return createIndex(row, column,
                           Encode({parentIdx.parameter, parentIdx.range,
                                   uint32_t(parent.row() - RangeFixedRowCount + 1)}));
      }

      // children of descriptors are data entries, and do not have children themselves
      return QModelIndex();
    }

    // otherwise it's a plain list of descriptors
    //
    // QModelIndex() <= root
    //   (descIdx, DescriptorFlag)                 <= descriptor[descIdx]
    //     (dataIdx, DescriptorDataFlag | descIdx) <= descriptor[descIdx].data[dataIdx]

    if(parent == QModelIndex())
      return createIndex(row, column, DescriptorFlag);
    if(parent.internalId() & DescriptorFlag)
      return createIndex(row, column, parent.row() | DescriptorDataFlag);

    // invalid, this would be a child of the data elements
    return QModelIndex();
  }

  QModelIndex parent(const QModelIndex &index) const override
  {
    if(index == QModelIndex())
      return QModelIndex();

    if(m_View.m_D3D12RootSig.resourceId != ResourceId())
    {
      quintptr id = index.internalId();

      // the fixed nodes are under the root
      if(id == FixedNode)
        return QModelIndex();

      // parameter nodes have a specific id, they are under the fixed node
      if(id == ParameterFlag)
        return createIndex(ParametersRootNode, 0, FixedNode);

      // a static sampler node, parented under the other fixed node
      if(id == StaticSamplerData)
        return createIndex(StaticSamplersRootNode, 0, FixedNode);

      // other rows that aren't parameter data are static sampler properties and are parented under
      // their static sampler
      if((id & ParameterData) == 0)
      {
        // this is the row index + 1
        uint32_t sampIndex = id & ~StaticSamplerData;

        if(sampIndex == 0)
          return QModelIndex();

        return createIndex(sampIndex - 1, 0, StaticSamplerData);
      }

      // at this point the index should either be a range, a descriptor, or a descriptor
      // data row.

      RootIdx decodedIndex = Decode(id);

      // we basically knock the finest grained index out of the parentIdx and turn it into a row.
      // The only special case is ranges since their parent needs the magic ParameterFlag id

      // descriptor data node - parent is the descriptor node
      if(decodedIndex.descriptor != 0)
      {
        int row = decodedIndex.descriptor - 1;

        decodedIndex.descriptor = 0;

        return createIndex(RangeFixedRowCount + row, 0, Encode(decodedIndex));
      }

      // descriptor node - parent is the range
      if(decodedIndex.range != 0)
      {
        int row = decodedIndex.range - 1;

        decodedIndex.range = 0;

        return createIndex(TableParameterFixedRowCount + row, 0, Encode(decodedIndex));
      }

      // should not be possible here
      if(decodedIndex.parameter == 0)
        return QModelIndex();

      // range node - parent is the parameter which has a different index
      int row = decodedIndex.parameter - 1;

      return createIndex(row, 0, ParameterFlag);
    }
    else
    {
      // the descriptors are parented directly under the root
      if(index.internalId() & DescriptorFlag)
        return QModelIndex();

      // the children of the descriptor itself are under the descriptor
      if(index.internalId() & DescriptorDataFlag)
      {
        int row = index.internalId() & ~DescriptorDataFlag;
        return createIndex(row, 0, DescriptorFlag);
      }
    }

    return QModelIndex();
  }

  int rowCount(const QModelIndex &parent = QModelIndex()) const override
  {
    if(m_View.m_D3D12RootSig.resourceId != ResourceId())
    {
      // for root signature the root node has 2 children for parameters and static samplers
      if(parent == QModelIndex())
        return FirstHeapRootNode + m_View.m_D3D12Heaps.count();

      // those fixed nodes have a simple row count
      if(parent.internalId() == FixedNode)
      {
        if(parent.row() == ParametersRootNode)
          return m_View.m_D3D12RootSig.parameters.count();
        if(parent.row() == StaticSamplersRootNode)
          return m_View.m_D3D12RootSig.staticSamplers.count();

        // other members are descriptor heaps which have no members
        return 0;
      }

      // parameter nodes have a specific id
      if(parent.internalId() == ParameterFlag)
      {
        const D3D12Pipe::RootParam &param = m_View.m_D3D12RootSig.parameters[parent.row()];

        if(!param.constants.empty())
          return ConstParameterFixedRowCount;

        if(param.descriptor.type != DescriptorType::Unknown)
          return DescParameterFixedRowCount + rowCount(param.descriptor);

        return TableParameterFixedRowCount + param.tableRanges.count();
      }

      // static sampler node
      if(parent.internalId() == StaticSamplerData)
      {
        return StaticSamplerFixedRowCount + samplerRowCount();
      }

      // other rows that aren't parameter data are static sampler properties themselves and have no children
      if((parent.internalId() & ParameterData) == 0)
        return 0;

      // at this point the parent node should either be a range, a descriptor, or a descriptor
      // data row. Parameter node parents are handled above as the magic ParameterFlag id

      RootIdx parentIdx = Decode(parent.internalId());

      // 0 should be impossible, all parent nodes at this level should have at least a parameter encoded
      if(parentIdx.parameter == 0 ||
         parentIdx.parameter - 1 >= m_View.m_D3D12RootSig.parameters.count())
        return 0;

      const D3D12Pipe::RootParam &param = m_View.m_D3D12RootSig.parameters[parentIdx.parameter - 1];

      if(parentIdx.range > 0 && parentIdx.range - 1 >= param.tableRanges.count())
        return 0;

      // parameters with no tables don't have any more children
      if(!param.constants.empty() || param.descriptor.type != DescriptorType::Unknown)
        return 0;

      // if range is 0 on the parent's ID then this is a range node, so take the index from the parent's row
      const D3D12Pipe::RootTableRange &range =
          param.tableRanges[parentIdx.range == 0 ? parent.row() - TableParameterFixedRowCount
                                                 : parentIdx.range - 1];

      // if this is a range node, parent's range will be 0. We return the number of descriptors (plus fixed rows)
      if(parentIdx.range == 0)
      {
        // fixed rows in range nodes have no children
        if(parent.row() < TableParameterFixedRowCount)
          return 0;

        // Do a clamp here if we have descriptors to display
        if(!m_View.m_Descriptors.empty())
        {
          uint32_t fullOffset = param.heapByteOffset + range.tableByteOffset;
          uint32_t maxDescriptors;
          if(range.category == DescriptorCategory::Sampler)
            maxDescriptors = uint32_t(m_View.m_SamplerDescriptors.size() - fullOffset);
          else
            maxDescriptors = uint32_t(m_View.m_Descriptors.size() - fullOffset);

          return RangeFixedRowCount + qMin(maxDescriptors, range.count);
        }

        // otherwise we'll have no descriptor rows but we will have two extras to show the space and
        // register that would normally be listed in the descriptor names
        return RangeFixedRowCount + 2;
      }

      // if the *parent* has a descriptor index then this must be a descriptor data row, it has no children.
      if(parentIdx.descriptor != 0)
        return 0;

      // otherwise it is a descriptor node

      // fixed rows in a range have no children
      if(parent.row() < RangeFixedRowCount)
        return 0;

      if(range.category == DescriptorCategory::Sampler)
        return samplerRowCount();

      // out of bounds descriptor index shouldn't happen as we clamped the count above
      if(param.heapByteOffset + range.tableByteOffset + parent.row() - RangeFixedRowCount >=
         m_View.m_Descriptors.size())
        return 0;

      return RootSigDescriptorFixedRows +
             rowCount(m_View.m_Descriptors[param.heapByteOffset + range.tableByteOffset +
                                           parent.row() - RangeFixedRowCount]);
    }
    else
    {
      if(parent == QModelIndex())
        return m_View.m_Descriptors.count();

      // the children of the descriptor itself don't have any children
      if(parent.internalId() & DescriptorDataFlag)
        return 0;

      uint32_t sampIndex = parent.row();
      if(!m_View.m_DescriptorToSamplerLookup.empty())
        sampIndex = m_View.m_DescriptorToSamplerLookup[parent.row()];

      if(sampIndex != ~0U && m_View.m_SamplerDescriptors[sampIndex].type == DescriptorType::Sampler)
        return samplerRowCount();

      int ret = rowCount(m_View.m_Descriptors[parent.row()]);

      if(parent.row() < m_View.m_Locations.count())
        ret += DescriptorLocationFixedRowCount;

      return ret;
    }
  }
  int columnCount(const QModelIndex &parent = QModelIndex()) const override { return 2; }
  Qt::ItemFlags flags(const QModelIndex &index) const override
  {
    if(!index.isValid())
      return 0;

    return QAbstractItemModel::flags(index);
  }

  QVariant headerData(int section, Qt::Orientation orientation, int role) const override
  {
    if(orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
      if(section == 0)
        return tr("Index");
      else if(section == 1)
        return tr("Contents");
    }

    return QVariant();
  }

  int rowCount(const Descriptor &desc, bool includeSampler = true) const
  {
    int ret = 1;
    switch(desc.type)
    {
      case DescriptorType::ConstantBuffer:
      case DescriptorType::Buffer:
      case DescriptorType::ReadWriteBuffer:
      case DescriptorType::TypedBuffer:
      case DescriptorType::ReadWriteTypedBuffer:
      {
        // type, resource, offset, size, button to view
        ret = 5;

        if(desc.flags != DescriptorFlags::NoFlags)
          ret++;

        // format or structure size
        if(desc.type == DescriptorType::TypedBuffer ||
           desc.type == DescriptorType::ReadWriteTypedBuffer)
          ret++;
        else if(desc.elementByteSize != 0)
          ret++;

        // counter buffer, offset, value
        if(desc.secondary != ResourceId())
          ret += 3;
        break;
      }
      case DescriptorType::AccelerationStructure:
      {
        // type, resource, size
        ret = 3;
        break;
      }
      case DescriptorType::Image:
      case DescriptorType::ImageSampler:
      case DescriptorType::ReadWriteImage:
      {
        // type, texture type, resource, format, min LOD, button to view
        ret = 6;

        // first/num slices
        ret++;

        // first/num mips
        ret++;

        // swizzle
        ret++;

        if(desc.flags != DescriptorFlags::NoFlags)
          ret++;

        if(desc.view != ResourceId())
          ret++;

        if(desc.type == DescriptorType::ImageSampler && includeSampler)
          ret += samplerRowCount(true);
        break;
      }
      case DescriptorType::Sampler: ret = samplerRowCount(); break;
      case DescriptorType::Unknown: break;
    }
    return ret;
  }

  QVariant data(const Descriptor &desc, int row, int col) const
  {
    if(row == 0)
    {
      if(col == 0)
        return lit("Type");

      if(m_API == GraphicsAPI::Vulkan)
      {
        switch(desc.type)
        {
          case DescriptorType::ConstantBuffer: return lit("Uniform Buffer");
          case DescriptorType::Buffer:    // no such type on vulkan
          case DescriptorType::ReadWriteBuffer: return lit("Storage Buffer");
          case DescriptorType::TypedBuffer: return lit("Texel Buffer");
          case DescriptorType::ReadWriteTypedBuffer: return lit("Storage Texel Buffer");
          case DescriptorType::AccelerationStructure: return lit("Acceleration Structure");
          case DescriptorType::Image: return lit("Sampled Image");
          case DescriptorType::ImageSampler: return lit("Combined Image/Sampler");
          case DescriptorType::ReadWriteImage: return lit("Storage Image");
          case DescriptorType::Sampler: return lit("Sampler");
          case DescriptorType::Unknown: return lit("Uninitialised");
        }
      }
      else
      {
        switch(desc.type)
        {
          case DescriptorType::ConstantBuffer: return lit("Constant Buffer");
          case DescriptorType::ImageSampler:    // no such type on D3D12
          case DescriptorType::Buffer:
          case DescriptorType::Image:
          case DescriptorType::TypedBuffer:
          case DescriptorType::AccelerationStructure: return lit("Shader Resource View");
          case DescriptorType::ReadWriteBuffer:
          case DescriptorType::ReadWriteTypedBuffer:
          case DescriptorType::ReadWriteImage: return lit("Unordered Resource View");
          case DescriptorType::Sampler: return lit("Sampler");
          case DescriptorType::Unknown: return lit("Uninitialised");
        }
      }

      return QVariant();
    }

    switch(desc.type)
    {
      case DescriptorType::ConstantBuffer:
      case DescriptorType::Buffer:
      case DescriptorType::ReadWriteBuffer:
      case DescriptorType::TypedBuffer:
      case DescriptorType::ReadWriteTypedBuffer:
      {
        if(row == 1)
          return col == 0 ? lit("Buffer") : QVariant::fromValue(desc.resource);

        if(row == 2)
          return col == 0 ? lit("Byte Offset")
                          : Formatter::HumanFormat(desc.byteOffset, Formatter::OffsetSize);

        if(row == 3)
          return col == 0 ? lit("Byte Size")
                          : Formatter::HumanFormat(desc.byteSize, Formatter::OffsetSize);

        row -= 4;

        if(desc.flags != DescriptorFlags::NoFlags)
        {
          if(row == 0)
            return col == 0 ? lit("Flags") : ToQStr(desc.flags);
          row--;
        }

        // format or structure size
        if(desc.type == DescriptorType::TypedBuffer ||
           desc.type == DescriptorType::ReadWriteTypedBuffer)
        {
          if(row == 0)
            return col == 0 ? lit("Format") : QString(desc.format.Name());
          row--;
        }
        else if(desc.elementByteSize != 0)
        {
          if(row == 0)
            return col == 0 ? lit("Element Size")
                            : Formatter::HumanFormat(desc.elementByteSize, Formatter::OffsetSize);
          row--;
        }

        // counter buffer, offset, value
        if(desc.secondary != ResourceId())
        {
          if(row == 0)
            return col == 0 ? lit("Counter Buffer") : QVariant::fromValue(desc.secondary);

          if(row == 1)
            return col == 0 ? lit("Counter Byte Offset")
                            : Formatter::HumanFormat(desc.counterByteOffset, Formatter::OffsetSize);

          if(row == 2)
            return col == 0 ? lit("Counter Value") : Formatter::Format(desc.bufferStructCount);

          row -= 3;
        }

        if(row == 0)
          return col == 0 ? lit("Show Contents") : QVariant::fromValue(ButtonTag(true, desc));

        break;
      }
      case DescriptorType::AccelerationStructure:
      {
        if(row == 1)
          return col == 0 ? lit("Acceleration Structure") : QVariant::fromValue(desc.resource);

        if(row == 2)
          return col == 0 ? lit("Byte Size")
                          : Formatter::HumanFormat(desc.byteSize, Formatter::OffsetSize);

        break;
      }
      case DescriptorType::Image:
      case DescriptorType::ImageSampler:
      case DescriptorType::ReadWriteImage:
      {
        if(row == 1)
          return col == 0 ? lit("Texture Type") : ToQStr(desc.textureType);

        if(row == 2)
          return col == 0 ? lit("Texture") : QVariant::fromValue(desc.resource);

        if(row == 3)
          return col == 0 ? lit("Format") : QString(desc.format.Name());

        if(row == 4)
          return col == 0 ? lit("Min LOD") : Formatter::Format(desc.minLODClamp);

        row -= 5;

        // first/num slices
        if(row == 0)
          return col == 0
                     ? lit("Slice Range")
                     : QFormatStr("%1 - %2")
                           .arg(desc.firstSlice)
                           .arg(desc.numSlices == UINT16_MAX ? desc.numSlices
                                                             : desc.firstSlice + desc.numSlices);

        // first/num mips
        if(row == 1)
          return col == 0 ? lit("Mip Range")
                          : QFormatStr("%1 - %2")
                                .arg(desc.firstMip)
                                .arg(desc.numMips == UINT8_MAX ? desc.numMips
                                                               : desc.firstMip + desc.numMips);

        // swizzle
        if(row == 2)
          return col == 0 ? lit("Swizzle")
                          : QFormatStr("%1%2%3%4")
                                .arg(ToQStr(desc.swizzle.red))
                                .arg(ToQStr(desc.swizzle.green))
                                .arg(ToQStr(desc.swizzle.blue))
                                .arg(ToQStr(desc.swizzle.alpha));

        row -= 3;

        if(desc.flags != DescriptorFlags::NoFlags)
        {
          if(row == 0)
            return col == 0 ? lit("Flags") : ToQStr(desc.flags);
          row--;
        }

        if(desc.view != ResourceId())
        {
          if(row == 0)
            return col == 0 ? lit("View") : QVariant::fromValue(desc.view);
          row--;
        }

        if(row == 0)
          return col == 0 ? lit("Show Contents") : QVariant::fromValue(ButtonTag(false, desc));

        break;
      }
      case DescriptorType::Sampler: break;
      case DescriptorType::Unknown: break;
    }

    return QVariant();
  }

  int samplerRowCount(bool combinedSampler = false) const
  {
    // type, address U/V/W, filter min/mag/mip, filter function
    int ret = 8;

    // omit the type for combined samplers
    if(combinedSampler)
      ret--;

    // min/max LOD
    ret++;

    // mip bias
    ret++;

    if(m_API == GraphicsAPI::Vulkan)
    {
      // immutable
      ret++;

      // object
      ret++;

      // seamless and unnormalized
      ret += 2;

      // sRGB border
      ret++;

      // munged ycbcr stuff
      ret++;
    }

    return ret;
  }

  QString filter(FilterMode mode, float aniso) const
  {
    QString ret = ToQStr(mode);
    if(mode == FilterMode::Anisotropic)
      ret += QFormatStr(" %1x").arg(aniso);
    return ret;
  }

  QString filter(FilterFunction func, CompareFunction compare) const
  {
    QString ret = ToQStr(func);
    if(func == FilterFunction::Comparison)
      ret += QFormatStr(" %1").arg(ToStr(compare));
    return ret;
  }

  QVariant data(const SamplerDescriptor &desc, int row, int col, bool combinedSampler = false) const
  {
    if(combinedSampler)
    {    // omit the type
    }
    else
    {
      if(row == 0)
        return col == 0 ? lit("Type") : ToQStr(desc.type);
      row--;
    }

    if(m_Ctx.APIProps().pipelineType == GraphicsAPI::Vulkan)
    {
      if(row == 0)
        return col == 0 ? lit("Immutable") : Formatter::Format(desc.creationTimeConstant);

      row--;
    }

    if(row == 0)
      return col == 0 ? lit("U Addressing") : ToQStr(desc.addressU);

    if(row == 1)
      return col == 0 ? lit("V Addressing") : ToQStr(desc.addressV);

    if(row == 2)
      return col == 0 ? lit("W Addressing") : ToQStr(desc.addressW);

    if(row == 3)
      return col == 0 ? lit("Minify") : filter(desc.filter.minify, desc.maxAnisotropy);

    if(row == 4)
      return col == 0 ? lit("Magnify") : filter(desc.filter.magnify, desc.maxAnisotropy);

    if(row == 5)
      return col == 0 ? lit("Mip") : filter(desc.filter.mip, desc.maxAnisotropy);

    if(row == 6)
      return col == 0 ? lit("Filter") : filter(desc.filter.filter, desc.compareFunction);

    if(row == 7)
    {
      QString minLOD = Formatter::Format(desc.minLOD);
      QString maxLOD = Formatter::Format(desc.maxLOD);

      if(desc.minLOD == -FLT_MAX)
        minLOD = lit("0");
      if(desc.minLOD == -1000.0)
        minLOD = lit("VK_LOD_CLAMP_NONE");

      if(desc.maxLOD == FLT_MAX)
        maxLOD = lit("FLT_MAX");
      if(desc.maxLOD == 1000.0)
        maxLOD = lit("VK_LOD_CLAMP_NONE");

      return col == 0 ? lit("LOD Range") : QFormatStr("%1 - %2").arg(minLOD).arg(maxLOD);
    }

    if(row == 8)
      return col == 0 ? lit("Mip Bias") : Formatter::Format(desc.mipBias);

    if(m_Ctx.APIProps().pipelineType == GraphicsAPI::Vulkan)
    {
      if(row == 9)
        return col == 0 ? lit("Sampler") : QVariant::fromValue(desc.object);

      if(row == 10)
        return col == 0 ? lit("Seamless Cubemaps") : Formatter::Format(desc.seamlessCubemaps);

      if(row == 11)
        return col == 0 ? lit("Unnormalised") : Formatter::Format(desc.unnormalized);

      if(row == 12)
        return col == 0 ? lit("sRGB Border") : Formatter::Format(desc.srgbBorder);

      if(row == 13)
      {
        if(col == 0)
          return lit("yCbCr Sampling");

        QString data;

        if(desc.ycbcrSampler != ResourceId())
        {
          data = m_Ctx.GetResourceName(desc.ycbcrSampler);

          data += QFormatStr(", %1 %2").arg(ToQStr(desc.ycbcrModel)).arg(ToQStr(desc.ycbcrRange));

          data += tr(", Chroma %1 [%2,%3]")
                      .arg(ToQStr(desc.chromaFilter))
                      .arg(ToQStr(desc.xChromaOffset))
                      .arg(ToQStr(desc.yChromaOffset));

          if(desc.forceExplicitReconstruction)
            data += tr(" Explicit");
        }
        else
        {
          data = lit("N/A");
        }

        return data;
      }
    }

    return QVariant();
  }

  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
  {
    if(role != Qt::DisplayRole)
      return QVariant();

    if(index.isValid())
    {
      int row = index.row();
      int col = index.column();

      quintptr id = index.internalId();

      if(m_View.m_D3D12RootSig.resourceId != ResourceId())
      {
        // the fixed nodes are under the root
        if(id == FixedNode)
        {
          if(col == 0)
          {
            if(row == ParametersRootNode)
              return lit("Parameters");
            if(row == StaticSamplersRootNode)
              return lit("Static Samplers");

            QVariant ret =
                QFormatStr("Heap %1").arg(ToQStr(m_View.m_D3D12Heaps[row - FirstHeapRootNode]));
            RichResourceTextInitialise(ret, &m_Ctx, false);
            return ret;
          }

          if(col == 1 && row >= FirstHeapRootNode)
            return QVariant::fromValue(ButtonTag(m_View.m_D3D12Heaps[row - FirstHeapRootNode]));

          return QVariant();
        }

        if(id == ParameterFlag)
        {
          if(col == 0)
            return QFormatStr("Parameter %1").arg(row);
          return QVariant();
        }

        if(id == StaticSamplerData)
        {
          if(col == 0)
            return QFormatStr("Static Sampler %1").arg(row);
          return QVariant();
        }

        if((id & ParameterData) == 0)
        {
          // this is the index + 1
          uint32_t sampIndex = id & ~StaticSamplerData;

          if(sampIndex == 0)
            return QVariant();

          sampIndex--;
          const D3D12Pipe::StaticSampler &samp = m_View.m_D3D12RootSig.staticSamplers[sampIndex];

          if(row >= StaticSamplerFixedRowCount)
            return data(samp.descriptor, row - StaticSamplerFixedRowCount, col);

          if(row == 0)
            return col == 0 ? lit("Visibility") : ToQStr(samp.visibility);

          if(row == 1)
            return col == 0 ? lit("Register Space") : Formatter::Format(samp.space);

          if(row == 2)
            return col == 0 ? lit("Register") : Formatter::Format(samp.reg);

          return QVariant();
        }

        RootIdx decodedIndex = Decode(id);

        const D3D12Pipe::RootParam &param =
            m_View.m_D3D12RootSig.parameters[decodedIndex.parameter - 1];

        if(!param.constants.empty())
        {
          if(row == 0)
            return col == 0 ? lit("Visibility") : ToQStr(param.visibility);

          rdcarray<uint32_t> words;
          words.resize(param.constants.byteSize() >> 2);
          memcpy(words.data(), param.constants.data(), words.byteSize());

          if(col == 0)
          {
            if(row == 1)
              return lit("Data (Decimal)");
            if(row == 2)
              return lit("Data (Hexadecimal)");
            if(row == 3)
              return lit("Data (Float)");
          }

          QString data;
          if(row == 3)
          {
            float *f = (float *)words.data();
            for(size_t i = 0; i < words.size(); i++)
              data += Formatter::Format(f[i]) + lit(" ");
          }
          else
          {
            for(uint32_t w : words)
              data += Formatter::Format(w, row == 2) + lit(" ");
          }

          return data;
        }

        if(param.descriptor.type != DescriptorType::Unknown)
        {
          if(row >= DescParameterFixedRowCount)
            return data(param.descriptor, row - DescParameterFixedRowCount, col);

          if(row == 0)
            return col == 0 ? lit("Visibility") : ToQStr(param.visibility);

          return QVariant();
        }

        if(decodedIndex.range == 0)
        {
          if(row >= TableParameterFixedRowCount)
          {
            if(col == 0)
              return QFormatStr("Range %1").arg(row - TableParameterFixedRowCount);
            return QVariant();
          }

          if(row == 0)
            return col == 0 ? lit("Visibility") : ToQStr(param.visibility);

          if(row == 1)
            return col == 0 ? lit("Heap") : QVariant::fromValue(param.heap);

          if(row == 2)
            return col == 0 ? lit("Table Offset") : ToQStr(param.heapByteOffset);

          return QVariant();
        }

        const D3D12Pipe::RootTableRange &range = param.tableRanges[decodedIndex.range - 1];

        if(decodedIndex.descriptor == 0)
        {
          if(row >= RangeFixedRowCount)
          {
            if(col == 0)
            {
              // with no descriptors, we put the space/register here
              if(m_View.m_Descriptors.empty())
              {
                if(row == RangeFixedRowCount)
                  return col == 0 ? lit("Register Space") : ToQStr(range.space);

                return col == 0 ? lit("Base Register") : ToQStr(range.baseRegister);
              }

              // otherwise we name all the descriptors
              QLatin1Char regChar = QLatin1Char('?');

              if(range.category == DescriptorCategory::Sampler)
                regChar = QLatin1Char('s');
              if(range.category == DescriptorCategory::ConstantBlock)
                regChar = QLatin1Char('b');
              if(range.category == DescriptorCategory::ReadOnlyResource)
                regChar = QLatin1Char('t');
              if(range.category == DescriptorCategory::ReadWriteResource)
                regChar = QLatin1Char('u');

              return QFormatStr("%1%2, space %3")
                  .arg(regChar)
                  .arg(range.baseRegister + row - RangeFixedRowCount)
                  .arg(range.space);
            }
            return QVariant();
          }

          if(row == 0)
          {
            if(col == 0)
              return lit("Range Type");

            if(range.category == DescriptorCategory::Sampler)
              return lit("Sampler");
            if(range.category == DescriptorCategory::ConstantBlock)
              return lit("Constant Buffer");
            if(range.category == DescriptorCategory::ReadOnlyResource)
              return lit("SRV");
            if(range.category == DescriptorCategory::ReadWriteResource)
              return lit("UAV");

            return QVariant();
          }

          if(row == 1)
          {
            if(col == 0)
              return lit("Table offset");

            if(range.appended)
              return QFormatStr("D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND (%1)").arg(range.tableByteOffset);
            return Formatter::HumanFormat(range.tableByteOffset, Formatter::NoFlags);
          }

          if(row == 2)
            return col == 0 ? lit("Descriptor Count")
                            : Formatter::HumanFormat(range.count, Formatter::NoFlags);

          return QVariant();
        }

        // descriptor node data itself
        const Descriptor &descriptor =
            m_View.m_Descriptors[param.heapByteOffset + range.tableByteOffset +
                                 decodedIndex.descriptor - 1];

        if(row == 0)
          return col == 0 ? lit("Index in heap")
                          : Formatter::HumanFormat(param.heapByteOffset + range.tableByteOffset +
                                                       decodedIndex.descriptor - 1,
                                                   Formatter::NoFlags);

        return data(descriptor, row - RootSigDescriptorFixedRows, col);
      }
      else
      {
        // the descriptors are parented directly under the root
        if(index.internalId() & DescriptorFlag)
        {
          if(col != 0)
            return QVariant();

          if(row < m_View.m_Locations.count())
            return m_View.m_Locations[row].logicalBindName;
          else
            return QFormatStr("Descriptor %1").arg(row);
        }

        if(index.internalId() & DescriptorDataFlag)
        {
          uint32_t descIndex = index.internalId() & ~DescriptorDataFlag;
          if(descIndex < m_View.m_Locations.size())
          {
            if(row == 0)
              return col == 0 ? lit("Shader Mask") : ToQStr(m_View.m_Locations[descIndex].stageMask);

            row--;
          }

          const Descriptor &desc = m_View.m_Descriptors[descIndex];

          uint32_t sampIndex = descIndex;
          if(!m_View.m_DescriptorToSamplerLookup.empty())
            sampIndex = m_View.m_DescriptorToSamplerLookup[descIndex];

          SamplerDescriptor dummy;
          const SamplerDescriptor &samp = sampIndex < m_View.m_SamplerDescriptors.size()
                                              ? m_View.m_SamplerDescriptors[sampIndex]
                                              : dummy;

          if(desc.type == DescriptorType::Sampler)
          {
            return data(samp, row, col);
          }
          else if(desc.type == DescriptorType::ImageSampler)
          {
            int pureDescriptorRowCount = rowCount(desc, false);

            if(row >= pureDescriptorRowCount)
              return data(samp, row - pureDescriptorRowCount, col, true);
          }

          return data(desc, row, col);
        }
      }
    }

    return QVariant();
  }

private:
  ICaptureContext &m_Ctx;
  DescriptorViewer &m_View;

  GraphicsAPI m_API;

#define BITS (QT_POINTER_SIZE * 8)

  static const int ParametersRootNode = 0;
  static const int StaticSamplersRootNode = 1;
  static const int FirstHeapRootNode = 2;

  // the number of rows in a table parameter before the ranges: visibility, heap, heap offset
  static const int TableParameterFixedRowCount = 3;
  // the number of rows in a range before the descriptors: category, table offset, count
  static const int RangeFixedRowCount = 3;
  // visibility only
  static const int DescParameterFixedRowCount = 1;
  // visibility, and 3 forms of interpretation of constants (float, decimal, hex)
  static const int ConstParameterFixedRowCount = 4;
  // 3 for space/reg/visibility, plus sampler properties
  static const int StaticSamplerFixedRowCount = 3;
  // one extra row for root-signature based descriptors to give the absolute heap offset
  static const int RootSigDescriptorFixedRows = 1;
  // extra row for the descripor visibility from location
  static const int DescriptorLocationFixedRowCount = 1;

  // top bit indicates parameter data or static sampler data
  static const quintptr ParameterData = 1ULL << (BITS - 1);
  static const quintptr StaticSamplerData = 0;

#if BITS == 32
  // 32-bit packing:
  // | 1 bit ParameterData flag |
  // | 6 bits Parameter Index |
  // | 5 bits Table Index |
  // | 20 bits Descriptor Index |

  static const quintptr ParameterFlag = ~0U;
  static const quintptr FixedNode = ~0U - 1;
  static const quintptr ParameterIndexMask = 0x3f;
  static const quintptr ParameterIndexShift = 25;
  static const quintptr TableIndexMask = 0x1f;
  static const quintptr TableIndexShift = 20;
  static const quintptr DescriptorIndexMask = 0xfffff;
#else
  // 64-bit packing:
  // | 1 bit ParameterData flag |
  // | 1 bit Parameter Node flag |
  // | 1 bit Fixed Node flag |
  // | 5 bits padding |
  // | 8-bit Parameter Index |
  // | 16 bit Table Index |
  // | 32 bits Descriptor Index |

  static const quintptr ParameterFlag = 3ULL << (BITS - 2);
  static const quintptr FixedNode = 1ULL << (BITS - 3);
  static const quintptr ParameterIndexMask = 0xff;
  static const quintptr ParameterIndexShift = 48;
  static const quintptr TableIndexMask = 0xffff;
  static const quintptr TableIndexShift = 32;
  static const quintptr DescriptorIndexMask = 0xffffffff;
#endif

  static_assert(ParameterFlag & ParameterData, "ParameterFlag should have ParameterData bit set");

  static_assert((DescriptorIndexMask & (ParameterIndexMask << ParameterIndexShift)) == 0,
                "Mask overlaps");
  static_assert((DescriptorIndexMask & (TableIndexMask << TableIndexShift)) == 0, "Mask overlaps");
  static_assert(((ParameterIndexMask << ParameterIndexShift) &
                 (TableIndexMask << TableIndexShift)) == 0,
                "Mask overlaps");

  static_assert(quintptr(quintptr(ParameterIndexMask << ParameterIndexShift) >>
                         ParameterIndexShift) == ParameterIndexMask,
                "Mask is clipped");
  static_assert(quintptr(quintptr(TableIndexMask << TableIndexShift) >> TableIndexShift) ==
                    TableIndexMask,
                "Mask is clipped");

  struct RootIdx
  {
    uint8_t parameter;
    uint16_t range;
    uint32_t descriptor;
  };

  static_assert(ParameterIndexMask <= UINT8_MAX, "Parameter mask is too large for decoded storage");
  static_assert(TableIndexMask <= UINT16_MAX, "Table mask is too large for decoded storage");
  static_assert(DescriptorIndexMask <= UINT32_MAX,
                "Descriptor mask is too large for decoded storage");

  RootIdx Decode(quintptr id) const
  {
    return {uint8_t((id >> ParameterIndexShift) & ParameterIndexMask),
            uint16_t((id >> TableIndexShift) & TableIndexMask), uint32_t(id & DescriptorIndexMask)};
  }

  quintptr Encode(RootIdx idx) const
  {
    return ParameterData | (quintptr(idx.parameter & ParameterIndexMask) << ParameterIndexShift) |
           (quintptr(idx.range & TableIndexMask) << TableIndexShift) |
           quintptr(idx.descriptor & DescriptorIndexMask);
  }

  // simple flags for plain descriptor index
  static const quintptr DescriptorDataFlag = 1ULL << (BITS - 1);
  static const quintptr DescriptorFlag = 1ULL << (BITS - 2);

#undef BITS
};

DescriptorViewer::DescriptorViewer(ICaptureContext &ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::DescriptorViewer), m_Ctx(ctx)
{
  ui->setupUi(this);

  {
    static bool registered = false;
    if(!registered)
    {
      registered = true;
      QMetaType::registerComparators<ButtonTag>();
    }
  }

  m_Model = new DescriptorItemModel(ctx, *this, this);

  ui->descriptors->setModel(m_Model);

  ui->descriptors->setFont(Formatter::PreferredFont());
  ui->descriptors->header()->setStretchLastSection(true);
  ui->descriptors->header()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  ui->descriptors->header()->setSectionResizeMode(0, QHeaderView::Interactive);

  ui->descriptors->header()->setMinimumSectionSize(40);
  ui->descriptors->header()->resizeSection(0, 150);

  ui->descriptors->header()->setSectionsMovable(false);

  ui->descriptors->header()->setCascadingSectionResizes(false);

  ButtonDelegate *viewDelegate = new ButtonDelegate(Icons::action_hover(), QString(), this);

  viewDelegate->setVisibleTrigger(Qt::DisplayRole,
                                  QVariant::fromValue(ButtonTag(false, Descriptor())));
  viewDelegate->setCentred(false);

  ui->descriptors->setItemDelegate(viewDelegate);

  QObject::connect(viewDelegate, &ButtonDelegate::messageClicked, [this](const QModelIndex &idx) {
    ButtonTag tag = idx.data(Qt::DisplayRole).value<ButtonTag>();

    if(tag.heap != ResourceId())
    {
      IDescriptorViewer *viewer = m_Ctx.ViewDescriptorStore(tag.heap);

      m_Ctx.AddDockWindow(viewer->Widget(), DockReference::AddTo, this);
      return;
    }

    if(tag.descriptor.type == DescriptorType::Unknown)
      return;

    if(tag.buffer && tag.descriptor.resource != ResourceId())
    {
      QString format;

      if(tag.descriptor.type == DescriptorType::TypedBuffer ||
         tag.descriptor.type == DescriptorType::ReadWriteTypedBuffer)
        format = BufferFormatter::GetBufferFormatString(Packing::C, ResourceId(), ShaderResource(),
                                                        tag.descriptor.format);

      IBufferViewer *viewer = m_Ctx.ViewBuffer(tag.descriptor.byteOffset, tag.descriptor.byteSize,
                                               tag.descriptor.resource, format);

      m_Ctx.AddDockWindow(viewer->Widget(), DockReference::AddTo, this);
    }
    else
    {
      TextureDescription *tex = NULL;
      CompType typeCast = CompType::Typeless;

      tex = m_Ctx.GetTexture(tag.descriptor.resource);
      typeCast = tag.descriptor.format.compType;

      if(tex)
      {
        if(tex->type == TextureType::Buffer)
        {
          IBufferViewer *viewer = m_Ctx.ViewTextureAsBuffer(
              tex->resourceId, Subresource(), BufferFormatter::GetTextureFormatString(*tex));

          m_Ctx.AddDockWindow(viewer->Widget(), DockReference::AddTo, this);
        }
        else
        {
          if(!m_Ctx.HasTextureViewer())
            m_Ctx.ShowTextureViewer();
          ITextureViewer *viewer = m_Ctx.GetTextureViewer();
          viewer->ViewTexture(tex->resourceId, typeCast, true);
        }
      }
    }
  });

  m_Ctx.AddCaptureViewer(this);
}

DescriptorViewer::~DescriptorViewer()
{
  m_Ctx.RemoveCaptureViewer(this);
  delete ui;
}

void DescriptorViewer::ViewDescriptorStore(ResourceId id)
{
  DescriptorStoreDescription *desc = m_Ctx.GetDescriptorStore(id);

  if(!desc)
  {
    qCritical() << "Invalid ID passed to ViewDescriptorStore";
    return;
  }

  m_DescriptorStore = *desc;

  setWindowTitle(tr("%1 contents").arg(m_Ctx.GetResourceName(m_DescriptorStore.resourceId)));

  ui->pipeLabel->setText(
      tr("The pipeline state viewer shows the current bindings in an easier format."));

  // refresh contents for the descriptor store
  OnEventChanged(m_Ctx.CurEvent());
}

void DescriptorViewer::ViewDescriptors(const rdcarray<Descriptor> &descriptors,
                                       const rdcarray<SamplerDescriptor> &samplerDescriptors)
{
  m_Descriptors = descriptors;
  m_SamplerDescriptors = samplerDescriptors;

  m_Descriptors.resize(qMin(descriptors.size(), samplerDescriptors.size()));
  m_SamplerDescriptors.resize(qMin(descriptors.size(), samplerDescriptors.size()));

  setWindowTitle(tr("Descriptors"));

  ui->pipeLabel->setText(QString());

  m_Model->refresh();
}

void DescriptorViewer::ViewD3D12State()
{
  m_D3D12Heaps = m_Ctx.CurD3D12PipelineState()->descriptorHeaps;
  m_D3D12RootSig = m_Ctx.CurD3D12PipelineState()->rootSignature;

  setWindowTitle(
      tr("%1 at EID %2").arg(m_Ctx.GetResourceName(m_D3D12RootSig.resourceId)).arg(m_Ctx.CurEvent()));

  ui->pipeLabel->setText(
      tr("The pipeline state viewer shows the current bindings in an easier format.\n"
         "This is a snapshot of the root signature & bound parameters at EID %1.")
          .arg(m_Ctx.CurEvent()));

  // fetch the descriptor contents for both heaps

  m_Ctx.Replay().AsyncInvoke([this](IReplayController *r) {
    ResourceId resourceHeap, samplerHeap;

    for(const D3D12Pipe::RootParam &param : m_D3D12RootSig.parameters)
    {
      for(const D3D12Pipe::RootTableRange &range : param.tableRanges)
      {
        if(range.category == DescriptorCategory::Sampler)
        {
          if(param.heap != ResourceId())
            samplerHeap = param.heap;
        }
        else
        {
          if(param.heap != ResourceId())
            resourceHeap = param.heap;
        }

        if(resourceHeap != ResourceId() && samplerHeap != ResourceId())
          break;
      }

      if(resourceHeap != ResourceId() && samplerHeap != ResourceId())
        break;
    }

    rdcarray<DescriptorRange> ranges;
    ranges.resize(1);

    rdcarray<Descriptor> descriptors;
    rdcarray<SamplerDescriptor> samplerDescriptors;

    if(resourceHeap != ResourceId())
    {
      DescriptorStoreDescription *resourceDesc = m_Ctx.GetDescriptorStore(resourceHeap);

      if(resourceDesc)
      {
        ranges[0].count = resourceDesc->descriptorCount;
        ranges[0].descriptorSize = resourceDesc->descriptorByteSize;
        ranges[0].offset = resourceDesc->firstDescriptorOffset;
        descriptors = r->GetDescriptors(resourceDesc->resourceId, ranges);
      }
    }
    else if(samplerHeap != ResourceId())
    {
      DescriptorStoreDescription *samplerDesc = m_Ctx.GetDescriptorStore(samplerHeap);

      if(samplerDesc)
      {
        ranges[0].count = samplerDesc->descriptorCount;
        ranges[0].descriptorSize = samplerDesc->descriptorByteSize;
        ranges[0].offset = samplerDesc->firstDescriptorOffset;
        samplerDescriptors = r->GetSamplerDescriptors(samplerDesc->resourceId, ranges);
      }
    }

    GUIInvoke::call(this, [this, descriptors = std::move(descriptors),
                           samplerDescriptors = std::move(samplerDescriptors)] {
      m_Descriptors = std::move(descriptors);
      m_SamplerDescriptors = std::move(samplerDescriptors);
      m_DescriptorToSamplerLookup.clear();
      m_Locations.clear();

      m_Model->refresh();
    });
  });
}

void DescriptorViewer::OnCaptureClosed()
{
  ToolWindowManager::closeToolWindow(this);
}

void DescriptorViewer::OnCaptureLoaded()
{
}

void DescriptorViewer::OnEventChanged(uint32_t eventId)
{
  // each time, re-fetch the descriptors to get up to date contents
  if(m_DescriptorStore.resourceId != ResourceId())
  {
    m_Ctx.Replay().AsyncInvoke([this](IReplayController *r) {
      uint32_t descSize = m_DescriptorStore.descriptorByteSize;

      rdcarray<DescriptorRange> ranges;
      ranges.resize(1);
      ranges[0].count = m_DescriptorStore.descriptorCount;
      ranges[0].descriptorSize = descSize;
      ranges[0].offset = m_DescriptorStore.firstDescriptorOffset;

      rdcarray<Descriptor> descriptors = r->GetDescriptors(m_DescriptorStore.resourceId, ranges);
      rdcarray<DescriptorLogicalLocation> locations =
          r->GetDescriptorLocations(m_DescriptorStore.resourceId, ranges);

      // fetch only sampler descriptors that we need

      rdcarray<uint32_t> descriptorToSamplerLookup;
      descriptorToSamplerLookup.fill(descriptors.size(), ~0u);

      ranges.clear();

      uint32_t idx = 0;
      for(size_t i = 0; i < descriptors.size(); i++)
      {
        if(descriptors[i].type == DescriptorType::Sampler ||
           descriptors[i].type == DescriptorType::ImageSampler)
        {
          descriptorToSamplerLookup[i] = idx;
          idx++;

          // combine contiguous ranges
          if(!ranges.empty() && ranges.back().offset + ranges.back().count * descSize == i * descSize)
          {
            ranges.back().count++;
          }
          else
          {
            DescriptorRange range;
            range.offset = uint32_t(i * descSize);
            range.descriptorSize = descSize;
            range.count = 1;
            ranges.push_back(range);
          }
        }
      }

      rdcarray<SamplerDescriptor> samplerDescriptors =
          r->GetSamplerDescriptors(m_DescriptorStore.resourceId, ranges);

      GUIInvoke::call(this,
                      [this, descriptors = std::move(descriptors), locations = std::move(locations),
                       descriptorToSamplerLookup = std::move(descriptorToSamplerLookup),
                       samplerDescriptors = std::move(samplerDescriptors)] {
                        m_Descriptors = std::move(descriptors);
                        m_Locations = std::move(locations);
                        m_SamplerDescriptors = std::move(samplerDescriptors);
                        m_DescriptorToSamplerLookup = std::move(descriptorToSamplerLookup);

                        RDTreeViewExpansionState state;
                        ui->descriptors->saveExpansion(state, 0);

                        m_Model->refresh();

                        ui->descriptors->applyExpansion(state, 0);
                      });
    });
  }
}

void DescriptorViewer::on_pipeButton_clicked()
{
  m_Ctx.ShowPipelineViewer();
}
