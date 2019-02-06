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

#pragma once

class RemoteHost;

// do not include any headers here, they must all be in QRDInterface.h
#include "QRDInterface.h"

DOCUMENT("A handle for interacting with a remote server on a given host.");
class RemoteHost
{
public:
  RemoteHost();

  VARIANT_CAST(RemoteHost);

  DOCUMENT("");
  bool operator==(const RemoteHost &o) const
  {
    return hostname == o.hostname && friendlyName == o.friendlyName && runCommand == o.runCommand &&
           serverRunning == o.serverRunning && connected == o.connected && busy == o.busy &&
           versionMismatch == o.versionMismatch;
  }
  bool operator!=(const RemoteHost &o) const { return !(*this == o); }
  DOCUMENT(
      "Ping the host to check current status - if the server is running, connection status, etc.");
  void CheckStatus();
  DOCUMENT(
      "Runs the command specified in :data:`runCommand`. Returns :class:`ReplayStatus` which "
      "indicates success or the type of failure.");
  ReplayStatus Launch();

  DOCUMENT("``True`` if a remote server is currently running on this host.");
  bool serverRunning : 1;
  DOCUMENT("``True`` if an active connection exists to this remote server.");
  bool connected : 1;
  DOCUMENT("``True`` if someone else is currently connected to this server.");
  bool busy : 1;
  DOCUMENT("``True`` if there is a code version mismatch with this server.");
  bool versionMismatch : 1;

  DOCUMENT("The hostname of this host.");
  rdcstr hostname;
  DOCUMENT("The friendly name for this host, if available (if empty, the Hostname is used).");
  rdcstr friendlyName;
  DOCUMENT("The command to run locally to try to launch the server remotely.");
  rdcstr runCommand;

  DOCUMENT("The last folder browser to on this host, to provide a reasonable default path.");
  rdcstr lastCapturePath;

  DOCUMENT(R"(
Returns the name to display for this host in the UI, either :data:`friendlyName` or :data:`hostname`
)");
  const rdcstr &Name() const { return !friendlyName.isEmpty() ? friendlyName : hostname; }
  DOCUMENT("Returns ``True`` if this host represents a connected ADB (Android) device.");
  bool IsADB() const
  {
    return hostname.count() > 4 && hostname[0] == 'a' && hostname[1] == 'd' && hostname[2] == 'b' &&
           hostname[3] == ':';
  }
  DOCUMENT("Returns ``True`` if this host represents the special localhost device.");
  bool IsLocalhost() const { return hostname == "localhost"; }
};

DECLARE_REFLECTION_STRUCT(RemoteHost);