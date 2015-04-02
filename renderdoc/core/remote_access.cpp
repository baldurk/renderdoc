/******************************************************************************
 * The MIT License (MIT)
 * 
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


#include "api/replay/renderdoc_replay.h"
#include "replay/type_helpers.h"
#include "core/core.h"
#include "os/os_specific.h"
#include "serialise/serialiser.h"
#include "socket_helpers.h"

enum PacketType
{
	ePacket_Noop,
	ePacket_Handshake,
	ePacket_Busy,
	ePacket_NewCapture,
	ePacket_RegisterAPI,
	ePacket_TriggerCapture,
	ePacket_CopyCapture,
	ePacket_QueueCapture,
	ePacket_NewChild,
};

void RenderDoc::RemoteAccessClientThread(void *s)
{
	Threading::KeepModuleAlive();

	Network::Socket *client = (Network::Socket *)s;

	Serialiser ser("", Serialiser::WRITING, false);

	string api = "";
	RDCDriver driver;
	RenderDoc::Inst().GetCurrentDriver(driver, api);

	ser.Rewind();

	string target = RenderDoc::Inst().GetCurrentTarget();
	ser.Serialise("", target);
	ser.Serialise("", api);
	uint32_t mypid = Process::GetCurrentPID();
	ser.Serialise("", mypid);

	if(!SendPacket(client, ePacket_Handshake, ser))
	{
		SAFE_DELETE(client);

		{
			SCOPED_LOCK(RenderDoc::Inst().m_SingleClientLock);
			RenderDoc::Inst().m_SingleClientName = "";
		}
		
		Threading::ReleaseModuleExitThread();
		return;
	}

	const int pingtime = 1000; // ping every 1000ms
	const int ticktime = 10; // tick every 10ms
	int curtime = 0;

	vector<CaptureData> captures;
	vector< pair<uint32_t, uint32_t> > children;

	while(client)
	{
		if(RenderDoc::Inst().m_RemoteClientThreadShutdown || (client && !client->Connected()))
		{
			SAFE_DELETE(client);
			break;
		}

		ser.Rewind();

		Threading::Sleep(ticktime);
		curtime += ticktime;

		PacketType packetType = ePacket_Noop;

		string curapi;
		RenderDoc::Inst().GetCurrentDriver(driver, curapi);

		vector<CaptureData> caps = RenderDoc::Inst().GetCaptures();
		vector< pair<uint32_t, uint32_t> > childprocs = RenderDoc::Inst().GetChildProcesses();

		if(curapi != api)
		{
			api = curapi;

			ser.Serialise("", api);

			packetType = ePacket_RegisterAPI;
		}
		else if(caps.size() != captures.size())
		{
			uint32_t idx = (uint32_t)captures.size();

			captures.push_back(caps[idx]);

			packetType = ePacket_NewCapture;

			ser.Serialise("", idx);
			ser.Serialise("", captures.back().timestamp);
			ser.Serialise("", captures.back().path);

			uint32_t len = 0;
			RENDERDOC_GetThumbnail(captures.back().path.c_str(), NULL, len);
			byte *thumb = new byte[len];
			RENDERDOC_GetThumbnail(captures.back().path.c_str(), thumb, len);

			size_t l = len;
			ser.Serialise("", len);
			ser.SerialiseBuffer("", thumb, l);
			delete[] thumb;
		}
		else if(childprocs.size() != children.size())
		{
			uint32_t idx = (uint32_t)children.size();

			children.push_back(childprocs[idx]);

			packetType = ePacket_NewChild;

			ser.Serialise("", children.back().first);
			ser.Serialise("", children.back().second);
		}

		if(curtime < pingtime && packetType == ePacket_Noop)
		{
			if(client->IsRecvDataWaiting())
			{
				PacketType type;
				Serialiser *recvser = NULL;

				if(!RecvPacket(client, type, &recvser))
					SAFE_DELETE(client);

				if(client == NULL)
				{
					SAFE_DELETE(recvser);
					continue;
				}
				else if(type == ePacket_TriggerCapture)
				{
					RenderDoc::Inst().TriggerCapture();
				}
				else if(type == ePacket_QueueCapture)
				{
					uint32_t frameNum = 0;
					recvser->Serialise("", frameNum);
					
					RenderDoc::Inst().QueueCapture(frameNum);
				}
				else if(type == ePacket_CopyCapture)
				{
					caps = RenderDoc::Inst().GetCaptures();

					uint32_t id = 0;
					recvser->Serialise("", id);

					if(id < caps.size())
					{
						ser.Serialise("", id);

						if(!SendPacket(client, ePacket_CopyCapture, ser))
						{
							SAFE_DELETE(client);
							continue;
						}

						ser.Rewind();

						if(!SendChunkedFile(client, ePacket_CopyCapture, caps[id].path.c_str(), ser, NULL))
						{
							SAFE_DELETE(client);
							continue;
						}

						RenderDoc::Inst().MarkCaptureRetrieved(id);
					}
				}

				SAFE_DELETE(recvser);
			}

			continue;
		}

		curtime = 0;

		if(!SendPacket(client, packetType, ser))
		{
			SAFE_DELETE(client);
			continue;
		}
	}
	
	// give up our connection
	{
		SCOPED_LOCK(RenderDoc::Inst().m_SingleClientLock);
		RenderDoc::Inst().m_SingleClientName = "";
	}

	Threading::ReleaseModuleExitThread();
}

void RenderDoc::RemoteAccessServerThread(void *s)
{
	Threading::KeepModuleAlive();

	Network::Socket *sock = (Network::Socket *)s;
	
	RenderDoc::Inst().m_SingleClientName = "";

	Threading::ThreadHandle clientThread = 0;

	RenderDoc::Inst().m_RemoteClientThreadShutdown = false;

	while(!RenderDoc::Inst().m_RemoteServerThreadShutdown)
	{
		Network::Socket *client = sock->AcceptClient(false);

		if(client == NULL)
		{
			if(!sock->Connected())
			{
				RDCERR("Error in accept - shutting down server");

				SAFE_DELETE(sock);
				Threading::ReleaseModuleExitThread();
				return;
			}

			Threading::Sleep(5);

			continue;
		}

		string existingClient;
		string newClient;
		bool kick = false;

		// receive handshake from client and get its name
		{
			PacketType type;
			Serialiser *ser = NULL;
			if(!RecvPacket(client, type, &ser))
			{
				SAFE_DELETE(ser);
				SAFE_DELETE(client);
				continue;
			}

			if(type != ePacket_Handshake)
			{
				SAFE_DELETE(ser);
				SAFE_DELETE(client);
				continue;
			}

			ser->SerialiseString("", newClient);
			ser->Serialise("", kick);
			
			SAFE_DELETE(ser);

			if(newClient.empty())
			{
				SAFE_DELETE(client);
				continue;
			}
		}

		// see if we have a client
		{
			SCOPED_LOCK(RenderDoc::Inst().m_SingleClientLock);
			existingClient = RenderDoc::Inst().m_SingleClientName;
		}

		if(!existingClient.empty() && kick)
		{
			// forcibly close communication thread which will kill the connection
			RenderDoc::Inst().m_RemoteClientThreadShutdown = true;
			Threading::JoinThread(clientThread);
			Threading::CloseThread(clientThread);
			clientThread = 0;
			RenderDoc::Inst().m_RemoteClientThreadShutdown = false;
			existingClient = "";
		}

		if(existingClient.empty())
		{
			SCOPED_LOCK(RenderDoc::Inst().m_SingleClientLock);
			RenderDoc::Inst().m_SingleClientName = newClient;
		}
		
		// if we've claimed client status, spawn a thread to communicate
		if(existingClient.empty() || kick)
		{
			clientThread = Threading::CreateThread(RemoteAccessClientThread, client);
			continue;
		}
		else
		{
			// if we've been asked to kick the existing connection off
			// reject this connection and tell them who is busy
			Serialiser ser("", Serialiser::WRITING, false);

			string api = "";
			RDCDriver driver;
			RenderDoc::Inst().GetCurrentDriver(driver, api);

			string target = RenderDoc::Inst().GetCurrentTarget();
			ser.Serialise("", target);
			ser.Serialise("", api);

			ser.SerialiseString("", RenderDoc::Inst().m_SingleClientName);

			// don't care about errors, we're going to close the connection either way
			SendPacket(client, ePacket_Busy, ser);

			SAFE_DELETE(client);
		}
	}
	
	RenderDoc::Inst().m_RemoteClientThreadShutdown = true;
	// don't join, just close the thread, as we can't wait while in the middle of module unloading
	Threading::CloseThread(clientThread);
	clientThread = 0;

	Threading::ReleaseModuleExitThread();
}

struct RemoteAccess : public IRemoteAccess
{
	public:
		RemoteAccess(Network::Socket *sock, string clientName, bool forceConnection, bool localhost)
			: m_Socket(sock), m_Local(localhost)
		{
			PacketType type;
			vector<byte> payload;

			{
				Serialiser ser("", Serialiser::WRITING, false);

				ser.SerialiseString("", clientName);
				ser.Serialise("", forceConnection);

				if(!SendPacket(m_Socket, ePacket_Handshake, ser))
				{
					SAFE_DELETE(m_Socket);
					return;
				}
			}

			Serialiser *ser = NULL;
			GetPacket(type, ser);

			RDCASSERT(type == ePacket_Handshake || type == ePacket_Busy);

			if(type == ePacket_Handshake)
			{
				ser->Serialise("", m_Target);
				ser->Serialise("", m_API);
				ser->Serialise("", m_PID);

				RDCLOG("Got remote handshake: %s (%s) [%u]", m_Target.c_str(), m_API.c_str(), m_PID);
			}
			else if(type == ePacket_Busy)
			{
				ser->Serialise("", m_Target);
				ser->Serialise("", m_API);
				ser->Serialise("", m_BusyClient);

				RDCLOG("Got remote busy signal: %s (%s) owned by %s", m_Target.c_str(), m_API.c_str(), m_BusyClient.c_str());
			}

			SAFE_DELETE(ser);
		}

		bool Connected() { return m_Socket != NULL && m_Socket->Connected(); }
		
		void Shutdown()
		{
			SAFE_DELETE(m_Socket);
			delete this;
		}

		const char *GetTarget()
		{
			return m_Target.c_str();
		}

		const char *GetAPI()
		{
			return m_API.c_str();
		}

		uint32_t GetPID()
		{
			return m_PID;
		}

		const char *GetBusyClient()
		{
			return m_BusyClient.c_str();
		}

		void TriggerCapture()
		{
			if(!SendPacket(m_Socket, ePacket_TriggerCapture))
				SAFE_DELETE(m_Socket);
		}
		
		void QueueCapture(uint32_t frameNumber)
		{
			Serialiser ser("", Serialiser::WRITING, false);

			ser.Serialise("", frameNumber);
		
			if(!SendPacket(m_Socket, ePacket_QueueCapture, ser))
			{
				SAFE_DELETE(m_Socket);
				return;
			}
		}

		void CopyCapture(uint32_t remoteID, const char *localpath)
		{
			Serialiser ser("", Serialiser::WRITING, false);

			ser.Serialise("", remoteID);
		
			if(!SendPacket(m_Socket, ePacket_CopyCapture, ser))
			{
				SAFE_DELETE(m_Socket);
				return;
			}

			m_CaptureCopies[remoteID] = localpath;
		}

		void ReceiveMessage(RemoteMessage *msg)
		{
			if(m_Socket == NULL)
			{
				msg->Type = eRemoteMsg_Disconnected;
				return;
			}

			if(!m_Socket->IsRecvDataWaiting())
			{
				if(!m_Socket->Connected())
				{
					SAFE_DELETE(m_Socket);
					msg->Type = eRemoteMsg_Disconnected;
				}
				else
				{
					Threading::Sleep(2);
					msg->Type = eRemoteMsg_Noop;
				}

				return;
			}

			PacketType type;
			Serialiser *ser = NULL;

			GetPacket(type, ser);

			if(m_Socket == NULL)
			{
				SAFE_DELETE(ser);

				msg->Type = eRemoteMsg_Disconnected;
				return;
			}
			else
			{
				if(type == ePacket_Noop)
				{
					SAFE_DELETE(ser);

					msg->Type = eRemoteMsg_Noop;
					return;
				}
				else if(type == ePacket_Busy)
				{
					string existingClient;
					ser->Serialise("", existingClient);

					SAFE_DELETE(ser);
					
					SAFE_DELETE(m_Socket);

					RDCLOG("Got busy signal: '%s", existingClient.c_str());
					msg->Type = eRemoteMsg_Busy;
					msg->Busy.ClientName = existingClient;
					return;
				}
				else if(type == ePacket_CopyCapture)
				{
					msg->Type = eRemoteMsg_CaptureCopied;
					
					ser->Serialise("", msg->NewCapture.ID);

					SAFE_DELETE(ser);
					
					msg->NewCapture.localpath = m_CaptureCopies[msg->NewCapture.ID];

					if(!RecvChunkedFile(m_Socket, ePacket_CopyCapture, msg->NewCapture.localpath.elems, ser, NULL))
					{
						SAFE_DELETE(ser);
						SAFE_DELETE(m_Socket);

						msg->Type = eRemoteMsg_Disconnected;
						return;
					}

					m_CaptureCopies.erase(msg->NewCapture.ID);
					
					SAFE_DELETE(ser);

					return;
				}
				else if(type == ePacket_NewChild)
				{
					msg->Type = eRemoteMsg_NewChild;

					ser->Serialise("", msg->NewChild.PID);
					ser->Serialise("", msg->NewChild.ident);
					
					RDCLOG("Got a new child process: %u %u", msg->NewChild.PID, msg->NewChild.ident);

					SAFE_DELETE(ser);

					return;
				}
				else if(type == ePacket_NewCapture)
				{
					msg->Type = eRemoteMsg_NewCapture;

					ser->Serialise("", msg->NewCapture.ID);
					ser->Serialise("", msg->NewCapture.timestamp);
					
					string path;
					ser->Serialise("", path);
					msg->NewCapture.localpath = path;

					if(!m_Local)
						msg->NewCapture.localpath = "";
					
					uint32_t thumblen = 0;
					ser->Serialise("", thumblen);

					create_array_uninit(msg->NewCapture.thumbnail, thumblen);

					size_t l = 0;
					byte *buf = &msg->NewCapture.thumbnail[0];
					ser->SerialiseBuffer("", buf, l);
					
					RDCLOG("Got a new capture: %d (time %llu) %d byte thumbnail", msg->NewCapture.ID, msg->NewCapture.timestamp, thumblen);

					SAFE_DELETE(ser);

					return;
				}
				else if(type == ePacket_RegisterAPI)
				{
					msg->Type = eRemoteMsg_RegisterAPI;
					
					ser->Serialise("", m_API);
					msg->RegisterAPI.APIName = m_API;
					
					RDCLOG("Used API: %s", m_API.c_str());
					
					SAFE_DELETE(ser);

					return;
				}
			}

			SAFE_DELETE(ser);

			msg->Type = eRemoteMsg_Noop;
		}

	private:
		Network::Socket *m_Socket;
		bool m_Local;
		string m_Target, m_API, m_BusyClient;
		uint32_t m_PID;

		map<uint32_t, string> m_CaptureCopies;

		void GetPacket(PacketType &type, Serialiser *&ser)
		{
			if(!RecvPacket(m_Socket, type, &ser))
				SAFE_DELETE(m_Socket);
		}
};

extern "C" RENDERDOC_API void RENDERDOC_CC RemoteAccess_Shutdown(RemoteAccess *access)
{ access->Shutdown(); }
 
extern "C" RENDERDOC_API const char* RENDERDOC_CC RemoteAccess_GetTarget(RemoteAccess *access)
{ return access->GetTarget(); }
extern "C" RENDERDOC_API const char* RENDERDOC_CC RemoteAccess_GetAPI(RemoteAccess *access)
{ return access->GetAPI(); }
extern "C" RENDERDOC_API uint32_t RENDERDOC_CC RemoteAccess_GetPID(RemoteAccess *access)
{ return access->GetPID(); }
extern "C" RENDERDOC_API const char* RENDERDOC_CC RemoteAccess_GetBusyClient(RemoteAccess *access)
{ return access->GetBusyClient(); }

extern "C" RENDERDOC_API void RENDERDOC_CC RemoteAccess_TriggerCapture(RemoteAccess *access)
{ access->TriggerCapture(); }
extern "C" RENDERDOC_API void RENDERDOC_CC RemoteAccess_QueueCapture(RemoteAccess *access, uint32_t frameNumber)
{ access->QueueCapture(frameNumber); }
extern "C" RENDERDOC_API void RENDERDOC_CC RemoteAccess_CopyCapture(RemoteAccess *access, uint32_t remoteID, const char *localpath)
{ access->CopyCapture(remoteID, localpath); }

extern "C" RENDERDOC_API void RENDERDOC_CC RemoteAccess_ReceiveMessage(RemoteAccess *access, RemoteMessage *msg)
{ access->ReceiveMessage(msg); }

extern "C" RENDERDOC_API
RemoteAccess * RENDERDOC_CC RENDERDOC_CreateRemoteAccessConnection(const char *host, uint32_t ident, const char *clientName, bool32 forceConnection)
{
	string s = "localhost";
	if(host != NULL && host[0] != '\0')
		s = host;

	bool localhost = (s == "localhost");

	Network::Socket *sock = Network::CreateClientSocket(s.c_str(), ident&0xffff, 3000);

	if(sock == NULL)
		return NULL;

	RemoteAccess *remote = new RemoteAccess(sock, clientName, forceConnection != 0, localhost);

	if(remote->Connected())
		return remote;

	delete remote;
	return NULL;
}
