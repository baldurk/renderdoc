/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 Baldur Karlsson
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


#include "gl_replay.h"
#include "gl_driver.h"
#include "gl_resources.h"

void GLReplay::PreContextInitCounters()
{
}

void GLReplay::PostContextInitCounters()
{
}

void GLReplay::PreContextShutdownCounters()
{
}

void GLReplay::PostContextShutdownCounters()
{
}

vector<uint32_t> GLReplay::EnumerateCounters()
{
	vector<uint32_t> ret;

	ret.push_back(eCounter_EventGPUDuration);

	return ret;
}

void GLReplay::DescribeCounter(uint32_t counterID, CounterDescription &desc)
{
	desc.counterID = counterID;

	if(counterID == eCounter_EventGPUDuration)
	{
		desc.name = "GPU Duration";
		desc.description = "Time taken for this event on the GPU, as measured by delta between two GPU timestamps.";
		desc.resultByteWidth = 8;
		desc.resultCompType = eCompType_Double;
		desc.units = eUnits_Seconds;
	}
	else
	{
		desc.name = "Unknown";
		desc.description = "Unknown counter ID";
		desc.resultByteWidth = 0;
		desc.resultCompType = eCompType_None;
		desc.units = eUnits_Absolute;
	}
}

struct GPUTimer
{
	GLuint obj;
	uint32_t eventID;
};

struct CounterContext
{
	uint32_t frameID;
	uint32_t minEID;
	uint32_t maxEID;
	uint32_t eventStart;
	vector<GPUTimer> timers;
	int reuseIdx;
};

void GLReplay::FillTimers(CounterContext &ctx, const DrawcallTreeNode &drawnode)
{
	if(drawnode.children.empty()) return;

	for(size_t i=0; i < drawnode.children.size(); i++)
	{
		const FetchDrawcall &d = drawnode.children[i].draw;
		FillTimers(ctx, drawnode.children[i]);

		if(d.events.count == 0) continue;

		GPUTimer *timer = NULL;
		
		bool includeEvent = (d.eventID >= ctx.minEID && d.eventID <= ctx.maxEID);

		if(includeEvent)
		{
			if(ctx.reuseIdx == -1)
			{
				ctx.timers.push_back(GPUTimer());

				timer = &ctx.timers.back();
				timer->eventID = d.eventID;
				timer->obj = 0;

				m_pDriver->glGenQueries(1, &timer->obj);
			}
			else
			{
				timer = &ctx.timers[ctx.reuseIdx++];
			}
		}

		m_pDriver->ReplayLog(ctx.frameID, ctx.eventStart, d.eventID, eReplay_WithoutDraw);
		
		if(includeEvent && timer->obj)
		{
			m_pDriver->glBeginQuery(eGL_TIME_ELAPSED, timer->obj);
			m_pDriver->ReplayLog(ctx.frameID, ctx.eventStart, d.eventID, eReplay_OnlyDraw);
			m_pDriver->glEndQuery(eGL_TIME_ELAPSED);
		}
		else
		{
			m_pDriver->ReplayLog(ctx.frameID, ctx.eventStart, d.eventID, eReplay_OnlyDraw);
		}
		
		ctx.eventStart = d.eventID+1;
	}
}

vector<CounterResult> GLReplay::FetchCounters(uint32_t frameID, uint32_t minEventID, uint32_t maxEventID, const vector<uint32_t> &counters)
{
	vector<CounterResult> ret;

	if(counters.empty())
	{
		RDCERR("No counters specified to FetchCounters");
		return ret;
	}
	
	MakeCurrentReplayContext(&m_ReplayCtx);
	
	uint32_t counterID = counters[0];
	RDCASSERT(counters.size() == 1);
	RDCASSERT(counterID == eCounter_EventGPUDuration);
	
	SCOPED_TIMER("Fetch Counters over %u-%u for %u", minEventID, maxEventID, counterID);

	CounterContext ctx;
	ctx.frameID = frameID;
	ctx.minEID = minEventID;
	ctx.maxEID = maxEventID;

	for(int loop=0; loop < 1; loop++)
	{
		ctx.eventStart = 0;
		ctx.reuseIdx = loop == 0 ? -1 : 0;
		FillTimers(ctx, m_pDriver->GetRootDraw());

		double nanosToSecs = 1.0/1000000000.0;

		GLuint prevbind = 0;
		m_pDriver->glGetIntegerv(eGL_QUERY_BUFFER_BINDING, (GLint *)&prevbind);
		m_pDriver->glBindBuffer(eGL_QUERY_BUFFER, 0);

		for(size_t i=0; i < ctx.timers.size(); i++)
		{
			if(ctx.timers[i].obj)
			{
				GLuint elapsed = 0;
				m_pDriver->glGetQueryObjectuiv(ctx.timers[i].obj, eGL_QUERY_RESULT, &elapsed);

				double duration = double(elapsed)*nanosToSecs;

				ret.push_back(CounterResult(ctx.timers[i].eventID, counterID, duration));
			}
			else
			{
				ret.push_back(CounterResult(ctx.timers[i].eventID, counterID, 0.0));
			}
		}

		m_pDriver->glBindBuffer(eGL_QUERY_BUFFER, prevbind);
	}

	for(size_t i=0; i < ctx.timers.size(); i++)
		m_pDriver->glDeleteQueries(1, &ctx.timers[i].obj);
	
	return ret;
}
