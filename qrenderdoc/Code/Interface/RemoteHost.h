/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Baldur Karlsson
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

class RemoteHost;

// do not include any headers here, they must all be in QRDInterface.h
#include "QRDInterface.h"

DOCUMENT("A handle for interacting with a remote servers on a given host.");
class RemoteHost
{
public:
  RemoteHost();

  VARIANT_CAST(RemoteHost);

  DOCUMENT(
      "Ping the host to check current status - if the server is running, connection status, etc.");
  void CheckStatus();
  DOCUMENT("Runs the command specified in :data:`RunCommand`.");
  void Launch();

  DOCUMENT("``True`` if a remote server is currently running on this host.");
  bool ServerRunning : 1;
  DOCUMENT("``True`` if an active connection exists to this remote server.");
  bool Connected : 1;
  DOCUMENT("``True`` if someone else is currently connected to this server.");
  bool Busy : 1;
  DOCUMENT("``True`` if there is a code version mismatch with this server.");
  bool VersionMismatch : 1;

  DOCUMENT("The hostname of this host.");
  QString Hostname;
  DOCUMENT("The command to run locally to try to launch the server remotely.");
  QString RunCommand;

  bool IsHostADB() const { return Hostname.startsWith(lit("adb:")); }
};

DECLARE_REFLECTION_STRUCT(RemoteHost);