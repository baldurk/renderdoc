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


#include <winsock2.h>
#include <ws2tcpip.h>

#include "os/os_specific.h"

#ifndef WSA_FLAG_NO_HANDLE_INHERIT
#define WSA_FLAG_NO_HANDLE_INHERIT 0x80
#endif

namespace Network
{

void Init()
{
	WSAData wsaData = {0};
	WSAStartup(MAKEWORD(2, 2), &wsaData);
}

void Shutdown()
{
	WSACleanup();
}

Socket::~Socket()
{
	Shutdown();
}

void Socket::Shutdown()
{
	if(Connected())
	{
		shutdown((SOCKET)socket, SD_BOTH);
		closesocket((SOCKET)socket);
		socket = -1;
	}
}

bool Socket::Connected() const
{
	return (SOCKET)socket != INVALID_SOCKET;
}

Socket *Socket::AcceptClient(bool wait)
{
	do
	{
		SOCKET s = accept(socket, NULL, NULL);

		if(s != INVALID_SOCKET)
		{
			u_long enable = 1;
			ioctlsocket(s, FIONBIO, &enable);

			BOOL nodelay = TRUE;
			setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&nodelay, sizeof(nodelay));

			return new Socket((ptrdiff_t)s);
		}

		int err = WSAGetLastError();

		if(err != WSAEWOULDBLOCK)
		{
			RDCWARN("accept: %d", err);
			Shutdown();
		}

		Threading::Sleep(4);
	} while(wait);

	return NULL;
}

bool Socket::SendDataBlocking(const void *buf, uint32_t length)
{
	if(length == 0) return true;

	uint32_t sent = 0;

	char *src = (char *)buf;

	u_long enable = 0;
	ioctlsocket(socket, FIONBIO, &enable);
	
	DWORD timeout = 3000;
	setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof(timeout));

	while(sent < length)
	{
		int ret = send(socket, src, length-sent, 0);

		if(ret <= 0)
		{
			int err = WSAGetLastError();

			if(err == WSAEWOULDBLOCK)
			{
				ret = 0;
			}
			else
			{
				RDCWARN("send: %d", err);
				Shutdown();
				return false;
			}
		}

		sent += ret;
		src += ret;
	}

	enable = 1;
	ioctlsocket(socket, FIONBIO, &enable);

	timeout = 600000;
	setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof(timeout));

	RDCASSERT(sent == length);

	return true;
}

bool Socket::IsRecvDataWaiting()
{
	char dummy;
	int ret = recv(socket, &dummy, 1, MSG_PEEK);

	if(ret == 0)
	{
		Shutdown();
		return false;
	}
	else if(ret <= 0)
	{
		int err = WSAGetLastError();

		if(err == WSAEWOULDBLOCK)
		{
			ret = 0;
		}
		else
		{
			RDCWARN("recv: %d", err);
			Shutdown();
			return false;
		}
	}

	return ret > 0;
}

bool Socket::RecvDataBlocking(void *buf, uint32_t length)
{
	if(length == 0) return true;

	uint32_t received = 0;

	char *dst = (char *)buf;
	
	u_long enable = 0;
	ioctlsocket(socket, FIONBIO, &enable);
	
	DWORD timeout = 3000;
	setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));

	while(received < length)
	{
		int ret = recv(socket, dst, length-received, 0);

		if(ret == 0)
		{
			Shutdown();
			return false;
		}
		else if(ret <= 0)
		{
			int err = WSAGetLastError();

			if(err == WSAEWOULDBLOCK)
			{
				ret = 0;
			}
			else
			{
				RDCWARN("recv: %d", err);
				Shutdown();
				return false;
			}
		}

		received += ret;
		dst += ret;
	}
	
	enable = 1;
	ioctlsocket(socket, FIONBIO, &enable);
	
	timeout = 600000;
	setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));

	RDCASSERT(received == length);

	return true;
}

Socket *CreateServerSocket(const char *bindaddr, uint16_t port, int queuesize)
{
	SOCKET s = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_NO_HANDLE_INHERIT);

	if(s == INVALID_SOCKET)
		return NULL;

	sockaddr_in addr;
	RDCEraseEl(addr);

	addr.sin_family = AF_INET;
	inet_pton(AF_INET, bindaddr, &addr.sin_addr);
	addr.sin_port = htons(port);

	int result = bind(s, (SOCKADDR *)&addr, sizeof(addr));
	if(result == SOCKET_ERROR)
	{
		RDCWARN("Failed to bind to %hs:%d - %d", bindaddr, port, WSAGetLastError());
		closesocket(s);
		return NULL;
	}

	result = listen(s, queuesize);
	if(result == SOCKET_ERROR)
	{
		RDCWARN("Failed to listen on %hs:%d - %d", bindaddr, port, WSAGetLastError());
		closesocket(s);
		return NULL;
	}
	
	u_long nonblock = 1;
	ioctlsocket(s, FIONBIO, &nonblock);

	return new Socket((ptrdiff_t)s);
}

Socket *CreateClientSocket(const wchar_t *host, uint16_t port, int timeoutMS)
{
	wchar_t portstr[7] = {0};
	StringFormat::wsnprintf(portstr, 6, L"%d", port);
	
    addrinfoW hints;
	RDCEraseEl(hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	
    addrinfoW *result = NULL;
	GetAddrInfoW(host, portstr, &hints, &result);
	
    for(addrinfoW *ptr = result; ptr != NULL; ptr = ptr->ai_next)
	{
		SOCKET s = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_NO_HANDLE_INHERIT);

		if(s == INVALID_SOCKET)
			return NULL;
		
		u_long enable = 1;
		ioctlsocket(s, FIONBIO, &enable);
		
		int result = connect(s, ptr->ai_addr, (int)ptr->ai_addrlen);
		if(result == SOCKET_ERROR)
		{
			fd_set set;
			FD_ZERO(&set);
			FD_SET(s, &set);

			int err = WSAGetLastError();

			if(err == WSAEWOULDBLOCK)
			{
				timeval timeout;
				timeout.tv_sec = (timeoutMS/1000);
				timeout.tv_usec = (timeoutMS%1000)*1000;
				result = select(0, NULL, &set, NULL, &timeout);

				if(result <= 0)
				{
					RDCDEBUG("connect timed out");
					closesocket(s);
					continue;
				}
				else
				{
					RDCDEBUG("connect before timeout");
				}
			}
			else
			{
				RDCDEBUG("problem other than blocking");
				closesocket(s);
				continue;
			}
		}
		else
		{
			RDCDEBUG("connected immediately");
		}

		BOOL nodelay = TRUE;
		setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&nodelay, sizeof(nodelay));
		
		return new Socket((ptrdiff_t)s);
	}

	RDCWARN("Failed to connect to %ls:%d", host, port);
	return NULL;
}

};