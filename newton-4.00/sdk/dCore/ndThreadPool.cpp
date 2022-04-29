/* Copyright (c) <2003-2021> <Julio Jerez, Newton Game Dynamics>
* 
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
* 
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely, subject to the following restrictions:
* 
* 1. The origin of this software must not be misrepresented; you must not
* claim that you wrote the original software. If you use this software
* in a product, an acknowledgment in the product documentation would be
* appreciated but is not required.
* 
* 2. Altered source versions must be plainly marked as such, and must not be
* misrepresented as being the original software.
* 
* 3. This notice may not be removed or altered from any source distribution.
*/

#include "ndCoreStdafx.h"
#include "ndTypes.h"
#include "ndUtils.h"
#include "ndProfiler.h"
#include "ndThreadPool.h"

ndThreadPool::ndWorker::ndWorker()
	:ndThread()
	,m_owner(nullptr)
	,m_begin(false)
	,m_stillLooping(true)
	,m_task(nullptr)
	,m_threadIndex(0)
{
}

ndThreadPool::ndWorker::~ndWorker()
{
	Finish();
}

void ndThreadPool::ndWorker::ThreadFunction()
{
#ifndef	D_USE_THREAD_EMULATION
	D_SET_TRACK_NAME(m_name);
	m_begin.store(true);
	m_stillLooping.store(true);
	while (m_begin.load())
	{
		ndTask* const task = m_task.load();
		if (task)
		{
			task->Execute();
			m_task.store(nullptr);
		}
		else
		{
			ndYield();
		}
	}
	m_stillLooping.store(false);
#endif
}

ndThreadPool::ndThreadPool(const char* const baseName)
	:ndSyncMutex()
	,ndThread()
	,m_workers(nullptr)
	,m_count(0)
{
	char name[256];
	strncpy(m_baseName, baseName, sizeof (m_baseName));
	sprintf(name, "%s_%d", m_baseName, 0);
	SetName(name);
}

ndThreadPool::~ndThreadPool()
{
	SetThreadCount(0);
}

void ndThreadPool::SetThreadCount(ndInt32 count)
{
#ifdef	D_USE_THREAD_EMULATION
	m_count = dClamp(count, 1, D_MAX_THREADS_COUNT) - 1;
#else
	count = dClamp(count, 1, D_MAX_THREADS_COUNT) - 1;
	if (count != m_count)
	{
		if (m_workers)
		{
			m_count = 0;
			delete[] m_workers;
			m_workers = nullptr;
		}
		if (count)
		{
			m_count = count;
			m_workers = new ndWorker[count];
			for (ndInt32 i = 0; i < count; ++i)
			{
				char name[256];
				m_workers[i].m_owner = this;
				m_workers[i].m_threadIndex = i;
				sprintf(name, "%s_%d", m_baseName, i + 1);
				m_workers[i].SetName(name);
			}
		}
	}
#endif
}

void ndThreadPool::Begin()
{
	D_TRACKTIME();
	for (ndInt32 i = 0; i < m_count; ++i)
	{
		m_workers[i].Signal();
	}
}

void ndThreadPool::End()
{
	#ifndef	D_USE_THREAD_EMULATION
	for (ndInt32 i = 0; i < m_count; ++i)
	{
		m_workers[i].m_begin.store(false);
	}

	bool stillLooping = true;
	do 
	{
		bool looping = false;
		for (ndInt32 i = 0; i < m_count; ++i)
		{
			looping = looping | m_workers[i].m_stillLooping.load();
		}
		stillLooping = stillLooping & looping;
	} while (stillLooping);
	#endif
}

void ndThreadPool::Release()
{
	ndSyncMutex::Release();
}

void ndThreadPool::TickOne()
{
	ndSyncMutex::Tick();
	ndSemaphore::Signal();
#ifdef D_USE_THREAD_EMULATION	
	ThreadFunction();
#endif
}
