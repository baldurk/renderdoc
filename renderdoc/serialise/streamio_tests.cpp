/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2019 Baldur Karlsson
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

#include "streamio.h"
#include "common/timing.h"

#if ENABLED(ENABLE_UNIT_TESTS)

#include "3rdparty/catch/catch.hpp"

TEST_CASE("Test basic stream I/O operations", "[streamio]")
{
  StreamWriter writer(1024);

  CHECK(writer.GetOffset() == 0);
  CHECK_FALSE(writer.IsErrored());

  writer.Write<uint32_t>(5);

  CHECK(writer.GetOffset() == 4);
  CHECK_FALSE(writer.IsErrored());

  writer.Write<uint32_t>(6);

  CHECK(writer.GetOffset() == 8);
  CHECK_FALSE(writer.IsErrored());

  writer.Write<uint16_t>(7);

  CHECK(writer.GetOffset() == 10);
  CHECK_FALSE(writer.IsErrored());

  writer.AlignTo<16>();

  CHECK(writer.GetOffset() == 16);
  CHECK_FALSE(writer.IsErrored());

  struct
  {
    uint32_t a = 5;
    uint32_t b = 6;
    uint16_t c = 7;
    byte padding[6] = {0, 0, 0, 0, 0, 0};
  } compare;

  RDCCOMPILE_ASSERT(sizeof(compare) == 16, "Compare struct is the wrong size");

  CHECK_FALSE(memcmp(writer.GetData(), &compare, sizeof(compare)));

  StreamReader reader((const byte *)&compare, sizeof(compare));

  uint32_t test;
  reader.Read(test);
  CHECK(test == 5);
  reader.Read(test);
  CHECK(test == 6);

  uint16_t test2;
  reader.Read(test2);
  CHECK(test2 == 7);

  CHECK_FALSE(reader.IsErrored());

  reader.AlignTo<16>();

  CHECK_FALSE(reader.IsErrored());
  CHECK(reader.AtEnd());

  // reading off the end should read 0s and move to error state
  reader.Read(test);
  CHECK(test == 0);

  CHECK(reader.IsErrored());
};

TEST_CASE("Test stream I/O operations over the network", "[streamio][network]")
{
  uint16_t port = 8235;
  Network::Socket *server = NULL;

  for(uint16_t probe = 0; probe < 20; probe++)
  {
    server = Network::CreateServerSocket("localhost", port, 2);

    if(server)
      break;

    port++;
  }

  REQUIRE(server);

  Network::Socket *sender = Network::CreateClientSocket("localhost", port, 10);

  REQUIRE(sender);

  Network::Socket *receiver = server->AcceptClient(250);

  REQUIRE(receiver);

  SECTION("Send/receive single int")
  {
    StreamWriter writer(sender, Ownership::Nothing);
    StreamReader reader(receiver, Ownership::Nothing);

    REQUIRE_FALSE(writer.IsErrored());
    REQUIRE_FALSE(reader.IsErrored());

    // we have to do the send/receive on threads since it is blocking

    uint32_t receivedValue = 0;

    Threading::ThreadHandle recvThread =
        Threading::CreateThread([&reader, &receivedValue]() { reader.Read(receivedValue); });

    Threading::ThreadHandle sendThread = Threading::CreateThread([&writer]() {
      uint32_t pi = 3141592;
      writer.Write(pi);
      writer.Flush();
    });

    Threading::Sleep(50);

    // REQUIRE that the value has propagated here. If not then something has gone wrong and we're
    // not getting forward progress, so we don't want to try to join the threads
    //
    // If this fails while debugging it's because the sleep above wasn't long enough to cover the
    // stepping process. We don't wait on threads to prevent holding up the whole process if there's
    // some deadlock between them.
    REQUIRE(receivedValue == 3141592);

    Threading::JoinThread(sendThread);
    Threading::CloseThread(sendThread);

    Threading::JoinThread(recvThread);
    Threading::CloseThread(recvThread);

    CHECK_FALSE(writer.IsErrored());
    CHECK_FALSE(reader.IsErrored());
  };

  SECTION("Send/receive multiple values")
  {
    StreamWriter writer(sender, Ownership::Nothing);
    StreamReader reader(receiver, Ownership::Nothing);

    REQUIRE_FALSE(writer.IsErrored());
    REQUIRE_FALSE(reader.IsErrored());

    // we have to do the send/receive on threads since it is blocking

    std::vector<uint64_t> receivedValues;
    std::vector<uint64_t> list = {1,  1,  2,   3,   5,   8,   13,  21,  34,
                                  55, 89, 144, 233, 377, 610, 987, 1597};

    // Tracks the lifetime of each thread.
    volatile int32_t threadA = 0, threadB = 0;

    Threading::ThreadHandle recvThread =
        Threading::CreateThread([&threadA, &reader, &receivedValues]() {
          int32_t sz = 0;
          reader.Read(sz);
          receivedValues.resize(sz);
          for(int32_t i = 0; i < sz; i++)
            reader.Read(receivedValues[i]);

          Atomic::Inc32(&threadA);
        });

    Threading::ThreadHandle sendThread = Threading::CreateThread([&threadB, &writer, list]() {
      int32_t sz = (int32_t)list.size();
      writer.Write(sz);
      for(int32_t i = 0; i < sz; i++)
        writer.Write(list[i]);

      writer.Flush();

      Atomic::Inc32(&threadB);
    });

    // wait up to 2 seconds for the threads to exit
    for(int i = 0; i < 2000 / 50; i++)
    {
      Threading::Sleep(50);
      if(threadA && threadB)
        break;
    }

    REQUIRE(threadA);
    REQUIRE(threadB);

    REQUIRE(receivedValues.size() == 17);
    CHECK(receivedValues == list);
    CHECK(writer.GetOffset() > 128);

    Threading::JoinThread(sendThread);
    Threading::CloseThread(sendThread);

    Threading::JoinThread(recvThread);
    Threading::CloseThread(recvThread);

    threadA = 0;
    threadB = 0;

    receivedValues.clear();

    uint64_t vals[10] = {1, 6, 0, 5, 3, 8, 7, 9, 2, 4};

    sendThread = Threading::CreateThread([&threadA, sender, &writer, &vals]() {

      PerformanceTimer timer;

      for(int32_t i = 0; i < 128; i++)
      {
        writer.Write(vals);

        // add random sleeps
        if(timer.GetMilliseconds() < i * 2)
          Threading::Sleep(15);
      }

      writer.Flush();

      // close the socket now
      sender->Shutdown();

      Atomic::Inc32(&threadA);
    });

    recvThread = Threading::CreateThread([&threadB, &reader, &receivedValues]() {
      uint64_t vals[10];

      reader.Read(vals);

      // keep reading indefinitely until we hit an error (i.e. socket disconnected)
      while(!reader.IsErrored())
      {
        receivedValues.insert(receivedValues.end(), vals, vals + ARRAY_COUNT(vals));
        reader.Read(vals);
      }

      Atomic::Inc32(&threadB);
    });

    // wait up to 2 seconds for the threads to exit
    for(int i = 0; i < 2000 / 50; i++)
    {
      Threading::Sleep(50);
      if(threadA && threadB)
        break;
    }

    REQUIRE(threadA);
    REQUIRE(threadB);

    Threading::JoinThread(sendThread);
    Threading::CloseThread(sendThread);

    Threading::JoinThread(recvThread);
    Threading::CloseThread(recvThread);

    // should have written 128 sets of 10 uint64s
    REQUIRE(receivedValues.size() == 1280);
    for(int i = 0; i < 128; i++)
    {
      for(int x = 0; x < 10; x++)
      {
        CHECK(receivedValues[i * 10 + x] == vals[x]);
      }
    }

    // reader *should* be errored now
    CHECK_FALSE(writer.IsErrored());
    CHECK(reader.IsErrored());

    // we shouldn't be able to write any more into the socket after it's been closed

    int32_t test = 42;
    bool success = writer.Write(test);
    success &= writer.Flush();
    CHECK_FALSE(success);
    CHECK(writer.IsErrored());
  };

  delete sender;
  delete receiver;
  delete server;
};

#endif    // ENABLED(ENABLE_UNIT_TESTS)