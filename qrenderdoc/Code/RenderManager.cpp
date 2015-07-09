#include "RenderManager.h"

#include "Core.h"

#include <QMutexLocker>

RenderManager::RenderManager()
{
	m_Running = false;
	m_Thread = NULL;
}

RenderManager::~RenderManager()
{

}

void RenderManager::Init(int proxyRenderer, QString replayHost, QString logfile, float *progress)
{
	if(m_Running)
		return;

	m_ProxyRenderer = proxyRenderer;
	m_ReplayHost = replayHost;
	m_Logfile = logfile;
	m_Progress = progress;

	*progress = 0.0f;

	m_Thread = new LambdaThread([this]() { run(); });
	m_Thread->start(QThread::HighestPriority);

	while(m_Thread->isRunning() && !m_Running) {}
}

bool RenderManager::IsRunning()
{
	return m_Thread->isRunning() && m_Running;
}

void RenderManager::AsyncInvoke(RenderManager::InvokeMethod m)
{
	InvokeHandle *cmd = new InvokeHandle(m);
	cmd->selfdelete = true;

	PushInvoke(cmd);
}

void RenderManager::BlockInvoke(RenderManager::InvokeMethod m)
{
	InvokeHandle *cmd = new InvokeHandle(m);

	PushInvoke(cmd);

	while(!cmd->processed) {}
}

void RenderManager::CloseThread()
{
	m_Running = false;

	m_RenderCondition.wakeAll();

	// wait for the thread to close and clean up
	while(m_Thread->isRunning()) {}

	m_Thread->deleteLater();
}

void RenderManager::PushInvoke(RenderManager::InvokeHandle *cmd)
{
	if(m_Thread == NULL || !m_Thread->isRunning() || !m_Running)
	{
		cmd->processed = true;
		if(cmd->selfdelete) delete cmd;
		return;
	}

	m_RenderLock.lock();
	m_RenderQueue.push_back(cmd);
	m_RenderLock.unlock();

	m_RenderCondition.wakeAll();
}

void RenderManager::run()
{
	IReplayRenderer *renderer = NULL;
	IRemoteRenderer *remote = NULL;

	if(m_ProxyRenderer < 0)
	{
		m_CreateStatus = RENDERDOC_CreateReplayRenderer(m_Logfile.toUtf8(), m_Progress, &renderer);
	}
	else
	{
		m_CreateStatus = RENDERDOC_CreateRemoteReplayConnection(m_ReplayHost.toUtf8(), &remote);

		if(remote == NULL)
		{
			return;
		}

		m_CreateStatus = remote->CreateProxyRenderer(m_ProxyRenderer, m_Logfile.toUtf8(), m_Progress, &renderer);

		if(renderer == NULL)
		{
			remote->Shutdown();
			remote = NULL;
			return;
		}
	}

	if(renderer == NULL)
		return;

	RENDERDOC_LogText(QString("QRenderDoc - renderer created for %1").arg(m_Logfile).toUtf8());

	m_Running = true;

	// main render command loop
	while(m_Running)
	{
		QQueue<InvokeHandle *> queue;

		// wait for the condition to be woken, grab current queue,
		// unlock again.
		{
			m_RenderLock.lock();
			m_RenderCondition.wait(&m_RenderLock);
			m_RenderQueue.swap(queue);
			m_RenderLock.unlock();
		}

		// process all the commands
		for(InvokeHandle *cmd : queue)
		{
			if(cmd == NULL) continue;

			if(cmd->method != NULL)
				cmd->method(renderer);

			cmd->processed = true;

			// if it's a throwaway command, delete it
			if(cmd->selfdelete)
				delete cmd;
		}
	}

	// clean up anything left in the queue
	{
		QQueue<InvokeHandle *> queue;

		m_RenderLock.lock();
		m_RenderQueue.swap(queue);
		m_RenderLock.unlock();

		for(InvokeHandle *cmd : queue)
		{
			if(cmd == NULL) continue;

			cmd->processed = true;

			if(cmd->selfdelete)
				delete cmd;
		}
	}

	// close the core renderer
	renderer->Shutdown();
	if(remote) remote->Shutdown();
}
