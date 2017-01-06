/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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

inline uint32_t RecvPacket(Network::Socket *sock)
{
  if(sock == NULL)
    return ~0U;

  uint32_t t = 0;
  if(!sock->RecvDataBlocking(&t, sizeof(t)))
    return ~0U;

  return t;
}

template <typename PacketTypeEnum>
bool RecvPacket(Network::Socket *sock, PacketTypeEnum &type, vector<byte> &payload)
{
  if(sock == NULL)
    return false;

  uint32_t t = 0;
  if(!sock->RecvDataBlocking(&t, sizeof(t)))
    return false;

  uint32_t payloadLength = 0;
  if(!sock->RecvDataBlocking(&payloadLength, sizeof(payloadLength)))
    return false;

  if(payloadLength > 0)
  {
    payload.resize(payloadLength);

    if(!sock->RecvDataBlocking(&payload[0], payloadLength))
      return false;
  }

  type = (PacketTypeEnum)t;

  return true;
}

template <typename PacketTypeEnum>
bool RecvPacket(Network::Socket *sock, PacketTypeEnum &type, Serialiser **ser)
{
  if(sock == NULL)
    return false;

  vector<byte> payload;
  bool ret = RecvPacket(sock, type, payload);
  if(!ret)
  {
    *ser = NULL;
    return false;
  }

  *ser = new Serialiser(payload.size(), &payload[0], false);

  return true;
}

template <typename PacketTypeEnum>
bool SendPacket(Network::Socket *sock, PacketTypeEnum type)
{
  if(sock == NULL)
    return false;

  uint32_t t = (uint32_t)type;
  if(!sock->SendDataBlocking(&t, sizeof(t)))
    return false;

  return true;
}

template <typename PacketTypeEnum>
bool SendPacket(Network::Socket *sock, PacketTypeEnum type, const Serialiser &ser)
{
  if(sock == NULL)
    return false;

  uint32_t t = (uint32_t)type;
  if(!sock->SendDataBlocking(&t, sizeof(t)))
    return false;

  uint32_t payloadLength = ser.GetOffset() & 0xffffffff;
  if(!sock->SendDataBlocking(&payloadLength, sizeof(payloadLength)))
    return false;

  if(!sock->SendDataBlocking(ser.GetRawPtr(0), payloadLength))
    return false;

  return true;
}

template <typename PacketTypeEnum>
bool RecvChunkedFile(Network::Socket *sock, PacketTypeEnum packetType, const char *logfile,
                     Serialiser *&ser, float *progress)
{
  if(sock == NULL)
    return false;

  vector<byte> payload;
  PacketTypeEnum type;

  if(!RecvPacket(sock, type, payload))
    return false;

  if(type != packetType)
    return false;

  ser = new Serialiser(payload.size(), &payload[0], false);

  uint64_t fileLength;
  uint32_t bufLength;
  uint32_t numBuffers;

  uint64_t sz = ser->GetSize();
  ser->SetOffset(sz - sizeof(uint64_t) - sizeof(uint32_t) * 2);

  ser->Serialise("", fileLength);
  ser->Serialise("", bufLength);
  ser->Serialise("", numBuffers);

  ser->SetOffset(0);

  FILE *f = FileIO::fopen(logfile, "wb");

  if(f == NULL)
  {
    return false;
  }

  if(progress)
    *progress = 0.0001f;

  for(uint32_t i = 0; i < numBuffers; i++)
  {
    if(!RecvPacket(sock, type, payload))
    {
      FileIO::fclose(f);
      return false;
    }

    if(type != packetType)
    {
      FileIO::fclose(f);
      return false;
    }

    FileIO::fwrite(&payload[0], 1, payload.size(), f);

    if(progress)
      *progress = float(i + 1) / float(numBuffers);
  }

  FileIO::fclose(f);

  return true;
}

template <typename PacketTypeEnum>
bool SendChunkedFile(Network::Socket *sock, PacketTypeEnum type, const char *logfile,
                     Serialiser &ser, float *progress)
{
  if(sock == NULL)
    return false;

  FILE *f = FileIO::fopen(logfile, "rb");

  if(f == NULL)
  {
    return false;
  }

  FileIO::fseek64(f, 0, SEEK_END);
  uint64_t fileLen = FileIO::ftell64(f);
  FileIO::fseek64(f, 0, SEEK_SET);

  uint32_t bufLen = (uint32_t)RDCMIN((uint64_t)4 * 1024 * 1024, fileLen);
  uint64_t n = fileLen / (uint64_t)bufLen;
  uint32_t numBufs = (uint32_t)n;
  if(fileLen % (uint64_t)bufLen > 0)
    numBufs++;    // last remaining buffer

  ser.Serialise("", fileLen);
  ser.Serialise("", bufLen);
  ser.Serialise("", numBufs);

  if(!SendPacket(sock, type, ser))
  {
    FileIO::fclose(f);
    return false;
  }

  byte *buf = new byte[bufLen];

  uint32_t t = (uint32_t)type;

  if(progress)
    *progress = 0.0001f;

  for(uint32_t i = 0; i < numBufs; i++)
  {
    uint32_t payloadLength = RDCMIN(bufLen, (uint32_t)(fileLen & 0xffffffff));

    FileIO::fread(buf, 1, payloadLength, f);

    if(!sock->SendDataBlocking(&t, sizeof(t)) ||
       !sock->SendDataBlocking(&payloadLength, sizeof(payloadLength)) ||
       !sock->SendDataBlocking(buf, payloadLength))
    {
      break;
    }

    fileLen -= payloadLength;
    if(progress)
      *progress = float(i + 1) / float(numBufs);
  }

  delete[] buf;

  FileIO::fclose(f);

  if(fileLen != 0)
  {
    return false;
  }

  return true;
}
