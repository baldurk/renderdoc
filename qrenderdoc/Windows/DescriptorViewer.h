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

#pragma once

#include <QFrame>
#include "Code/Interface/QRDInterface.h"

namespace Ui
{
class DescriptorViewer;
}

class DescriptorItemModel;

class DescriptorViewer : public QFrame, public IDescriptorViewer, public ICaptureViewer
{
  Q_OBJECT

public:
  explicit DescriptorViewer(ICaptureContext &ctx, QWidget *parent = 0);
  ~DescriptorViewer();

  void ViewDescriptorStore(ResourceId id);
  void ViewDescriptors(const rdcarray<Descriptor> &descriptors,
                       const rdcarray<SamplerDescriptor> &samplerDescriptors);
  void ViewD3D12State();

  // IDescriptorViewer
  QWidget *Widget() override { return this; }
  // ICaptureViewer
  void OnCaptureLoaded() override;
  void OnCaptureClosed() override;
  void OnSelectedEventChanged(uint32_t eventId) override {}
  void OnEventChanged(uint32_t eventId) override;

private slots:
  void on_pipeButton_clicked();

private:
  Ui::DescriptorViewer *ui;
  ICaptureContext &m_Ctx;

  DescriptorStoreDescription m_DescriptorStore;

  rdcarray<Descriptor> m_Descriptors;
  rdcarray<SamplerDescriptor> m_SamplerDescriptors;
  rdcarray<DescriptorLogicalLocation> m_Locations;

  // the descriptors array is always full (we don't worry about the overallocation for only-samplers),
  // but if we fetched these ourselves we will have fetched samplers sparsely only when necessary.
  // This array is the same size as m_Descriptors in that case containing the lookup indices in the samplers array
  rdcarray<uint32_t> m_DescriptorToSamplerLookup;

  rdcarray<ResourceId> m_D3D12Heaps;
  D3D12Pipe::RootSignature m_D3D12RootSig;

  // make the model a friend for simplicity
  friend class DescriptorItemModel;

  DescriptorItemModel *m_Model;
};
