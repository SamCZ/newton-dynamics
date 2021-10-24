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

#ifndef _D_THREAD_POOL_H_
#define _D_THREAD_POOL_H_

#include "dCoreStdafx.h"
#include "dTypes.h"
#include "dArray.h"
#include "dThread.h"
#include "dSyncMutex.h"
#include "dSemaphore.h"
#include "dClassAlloc.h"

#define	D_MAX_THREADS_COUNT	16

class dThreadPoolJob
{
	public:
	dThreadPoolJob() 
	{
	}

	virtual ~dThreadPoolJob() 
	{
	}

	dInt32 GetThreadId() const 
	{ 
		return m_threadIndex; 
	}

	virtual void Execute() = 0;

	private:
	dInt32 m_threadIndex;
	friend class dThreadPool;
};

class dThreadPool: public dSyncMutex, public dThread
{
	class dWorkerThread: public dThread
	{
		public:
		D_CORE_API dWorkerThread();
		D_CORE_API virtual ~dWorkerThread();

		private:
		void ExecuteJob(dThreadPoolJob* const job);
		virtual void ThreadFunction();

		dThreadPoolJob* m_job;
		dThreadPool* m_owner;
		dInt32 m_threadIndex;
		friend class dThreadPool;
	};

	class dThreadLockFreeUpdate: public dThreadPoolJob
	{
		public:
		dThreadLockFreeUpdate()
			:dThreadPoolJob()
			,m_job(nullptr)
			,m_begin(false)
			,m_joindInqueue(nullptr)
		{
		}

		virtual void Execute();
		private:
		dAtomic<dThreadPoolJob*> m_job;
		dAtomic<bool> m_begin;
		dAtomic<dInt32>* m_joindInqueue;
		friend class dThreadPool;
	};

	public:
	D_CORE_API dThreadPool(const char* const baseName);
	D_CORE_API virtual ~dThreadPool();

	dInt32 GetCount() const;
	D_CORE_API void SetCount(dInt32 count);

	D_CORE_API void TickOne();
	D_CORE_API void ExecuteJobs(dThreadPoolJob** const jobs);

	D_CORE_API void Begin();
	D_CORE_API void End();

	private:
	D_CORE_API virtual void Release();

	dSyncMutex m_sync;
	dWorkerThread* m_workers;
	dInt32 m_count;
	char m_baseName[32];
	dAtomic<dInt32> m_joindInqueue;
	dThreadLockFreeUpdate m_lockFreeJobs[D_MAX_THREADS_COUNT];
};

inline dInt32 dThreadPool::GetCount() const
{
	return m_count + 1;
}

#endif
