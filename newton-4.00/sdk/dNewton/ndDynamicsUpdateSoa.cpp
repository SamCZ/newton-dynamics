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

#include "dCoreStdafx.h"
#include "ndNewtonStdafx.h"
#include "ndWorld.h"
#include "ndBodyDynamic.h"
#include "ndSkeletonList.h"
#include "ndDynamicsUpdateSoa.h"
#include "ndJointBilateralConstraint.h"

#define D_SSE_WORK_GROUP 4 
using namespace ndSoa;

ndDynamicsUpdateSoa::ndDynamicsUpdateSoa(ndWorld* const world)
	:ndDynamicsUpdate(world)
	,m_soaJointRows(D_DEFAULT_BUFFER_SIZE * 4)
{
	m_ordinals.m_i[0] = 0;
	m_ordinals.m_i[1] = 1;
	m_ordinals.m_i[2] = 2;
	m_ordinals.m_i[3] = 3;
}

ndDynamicsUpdateSoa::~ndDynamicsUpdateSoa()
{
	Clear();
	m_soaJointRows.Resize(D_DEFAULT_BUFFER_SIZE * 4);
}

const char* ndDynamicsUpdateSoa::GetStringId() const
{
	return "sse soa";
}


void ndDynamicsUpdateSoa::DetermineSleepStates()
{
	D_TRACKTIME();
	class ndDetermineSleepStates : public ndScene::ndBaseJob
	{
		public:
		virtual void Execute()
		{
			D_TRACKTIME();
			ndWorld* const world = m_owner->GetWorld();
			ndDynamicsUpdateSoa* const me = (ndDynamicsUpdateSoa*)world->m_solver;
			const dArray<ndIsland>& islandArray = me->m_islands;

			const dInt32 threadIndex = GetThreadId();
			const dInt32 threadCount = m_owner->GetThreadCount();
			const dInt32 islandCount = islandArray.GetCount();

			for (dInt32 i = threadIndex; i < islandCount; i += threadCount)
			{
				const ndIsland& island = islandArray[i];
				me->UpdateIslandState(island);
			}
		}
	};

	ndScene* const scene = m_world->GetScene();
	scene->SubmitJobs<ndDetermineSleepStates>();
}

void ndDynamicsUpdateSoa::UpdateIslandState(const ndIsland& island)
{
	dFloat32 velocityDragCoeff = D_FREEZZING_VELOCITY_DRAG;

	const dInt32 count = island.m_count;
	if (count <= D_SMALL_ISLAND_COUNT)
	{
		velocityDragCoeff = dFloat32(0.9999f);
	}

	dFloat32 maxAccel = dFloat32(0.0f);
	dFloat32 maxAlpha = dFloat32(0.0f);
	dFloat32 maxSpeed = dFloat32(0.0f);
	dFloat32 maxOmega = dFloat32(0.0f);

	const dFloat32 speedFreeze = m_world->m_freezeSpeed2;
	const dFloat32 accelFreeze = m_world->m_freezeAccel2 * ((count <= D_SMALL_ISLAND_COUNT) ? dFloat32(0.01f) : dFloat32(1.0f));
	const dFloat32 acc2 = D_SOLVER_MAX_ERROR * D_SOLVER_MAX_ERROR;
	const dFloat32 maxAccNorm2 = (count > 4) ? acc2 : acc2 * dFloat32(0.0625f);
	const dVector velocDragVect(velocityDragCoeff, velocityDragCoeff, velocityDragCoeff, dFloat32(0.0f));

	dInt32 stackSleeping = 1;
	dInt32 sleepCounter = 10000;
	ndBodyKinematic** const bodyIslands = &m_bodyIslandOrder[island.m_start];
	for (dInt32 i = 0; i < count; i++)
	{
		ndBodyDynamic* const dynBody = bodyIslands[i]->GetAsBodyDynamic();
		if (dynBody)
		{
			dAssert(dynBody->m_accel.m_w == dFloat32(0.0f));
			dAssert(dynBody->m_alpha.m_w == dFloat32(0.0f));
			dAssert(dynBody->m_veloc.m_w == dFloat32(0.0f));
			dAssert(dynBody->m_omega.m_w == dFloat32(0.0f));

			dUnsigned32 equilibrium = (dynBody->GetInvMass() == dFloat32(0.0f)) ? 1 : dynBody->m_autoSleep;
			const dVector isMovingMask(dynBody->m_veloc + dynBody->m_omega + dynBody->m_accel + dynBody->m_alpha);
			const dVector mask(isMovingMask.TestZero());
			const dInt32 test = mask.GetSignMask() & 7;
			if (test != 7)
			{
				const dFloat32 accel2 = dynBody->m_accel.DotProduct(dynBody->m_accel).GetScalar();
				const dFloat32 alpha2 = dynBody->m_alpha.DotProduct(dynBody->m_alpha).GetScalar();
				const dFloat32 speed2 = dynBody->m_veloc.DotProduct(dynBody->m_veloc).GetScalar();
				const dFloat32 omega2 = dynBody->m_omega.DotProduct(dynBody->m_omega).GetScalar();

				dVector accelTest((dynBody->m_accel.DotProduct(dynBody->m_accel) > maxAccNorm2) | (dynBody->m_alpha.DotProduct(dynBody->m_alpha) > maxAccNorm2));
				dynBody->m_accel = dynBody->m_accel & accelTest;
				dynBody->m_alpha = dynBody->m_alpha & accelTest;

				maxAccel = dMax(maxAccel, accel2);
				maxAlpha = dMax(maxAlpha, alpha2);
				maxSpeed = dMax(maxSpeed, speed2);
				maxOmega = dMax(maxOmega, omega2);
				dUnsigned32 equilibriumTest = (accel2 < accelFreeze) && (alpha2 < accelFreeze) && (speed2 < speedFreeze) && (omega2 < speedFreeze);

				if (equilibriumTest)
				{
					const dVector veloc(dynBody->m_veloc * velocDragVect);
					const dVector omega(dynBody->m_omega * velocDragVect);
					const dVector velocMask(veloc.DotProduct(veloc) > m_velocTol);
					const dVector omegaMask(omega.DotProduct(omega) > m_velocTol);
					dynBody->m_veloc = velocMask & veloc;
					dynBody->m_omega = omegaMask & omega;
				}

				equilibrium &= equilibriumTest;
				stackSleeping &= equilibrium;
				sleepCounter = dMin(sleepCounter, dynBody->m_sleepingCounter);
				dynBody->m_sleepingCounter++;
			}
			if (dynBody->m_equilibrium != equilibrium)
			{
				dynBody->m_equilibrium = equilibrium;
			}
		}
		else
		{
			ndBodyKinematic* const kinBody = bodyIslands[i]->GetAsBodyKinematic();
			dAssert(kinBody);
			dUnsigned32 equilibrium = (kinBody->GetInvMass() == dFloat32(0.0f)) ? 1 : (kinBody->m_autoSleep & ~kinBody->m_equilibriumOverride);
			const dVector isMovingMask(kinBody->m_veloc + kinBody->m_omega);
			const dVector mask(isMovingMask.TestZero());
			const dInt32 test = mask.GetSignMask() & 7;
			if (test != 7)
			{
				const dFloat32 speed2 = kinBody->m_veloc.DotProduct(kinBody->m_veloc).GetScalar();
				const dFloat32 omega2 = kinBody->m_omega.DotProduct(kinBody->m_omega).GetScalar();

				maxSpeed = dMax(maxSpeed, speed2);
				maxOmega = dMax(maxOmega, omega2);
				dUnsigned32 equilibriumTest = (speed2 < speedFreeze) && (omega2 < speedFreeze);

				if (equilibriumTest)
				{
					const dVector veloc(kinBody->m_veloc * velocDragVect);
					const dVector omega(kinBody->m_omega * velocDragVect);
					const dVector velocMask(veloc.DotProduct(veloc) > m_velocTol);
					const dVector omegaMask(omega.DotProduct(omega) > m_velocTol);
					kinBody->m_veloc = velocMask & veloc;
					kinBody->m_omega = omegaMask & omega;
				}

				equilibrium &= equilibriumTest;
				stackSleeping &= equilibrium;
				sleepCounter = dMin(sleepCounter, kinBody->m_sleepingCounter);
			}
			if (kinBody->m_equilibrium != equilibrium)
			{
				kinBody->m_equilibrium = equilibrium;
			}
		}
	}

	if (stackSleeping)
	{
		for (dInt32 i = 0; i < count; i++)
		{
			// force entire island to equilibriumTest
			ndBodyDynamic* const body = bodyIslands[i]->GetAsBodyDynamic();
			if (body)
			{
				body->m_accel = dVector::m_zero;
				body->m_alpha = dVector::m_zero;
				body->m_veloc = dVector::m_zero;
				body->m_omega = dVector::m_zero;
				body->m_equilibrium = (body->GetInvMass() == dFloat32(0.0f)) ? 1 : body->m_autoSleep;
			}
			else
			{
				ndBodyKinematic* const kinBody = bodyIslands[i]->GetAsBodyKinematic();
				dAssert(kinBody);
				kinBody->m_veloc = dVector::m_zero;
				kinBody->m_omega = dVector::m_zero;
				kinBody->m_equilibrium = (kinBody->GetInvMass() == dFloat32(0.0f)) ? 1 : kinBody->m_autoSleep;
			}
		}
	}
	else if ((count > 1) || bodyIslands[0]->m_bodyIsConstrained)
	{
		const bool state =
			(maxAccel > m_world->m_sleepTable[D_SLEEP_ENTRIES - 1].m_maxAccel) ||
			(maxAlpha > m_world->m_sleepTable[D_SLEEP_ENTRIES - 1].m_maxAlpha) ||
			(maxSpeed > m_world->m_sleepTable[D_SLEEP_ENTRIES - 1].m_maxVeloc) ||
			(maxOmega > m_world->m_sleepTable[D_SLEEP_ENTRIES - 1].m_maxOmega);

		if (state)
		{
			for (dInt32 i = 0; i < count; i++)
			{
				ndBodyDynamic* const body = bodyIslands[i]->GetAsBodyDynamic();
				dAssert(body);
				if (body)
				{
					body->m_sleepingCounter = 0;
				}
			}
		}
		else
		{
			if (count < D_SMALL_ISLAND_COUNT)
			{
				// delay small islandArray for about 10 seconds
				sleepCounter >>= 8;
				for (dInt32 i = 0; i < count; i++)
				{
					ndBodyKinematic* const body = bodyIslands[i];
					body->m_equilibrium = 0;
				}
			}
			dInt32 timeScaleSleepCount = dInt32(dFloat32(60.0f) * sleepCounter * m_timestep);

			dInt32 index = D_SLEEP_ENTRIES;
			for (dInt32 i = 1; i < D_SLEEP_ENTRIES; i++)
			{
				if (m_world->m_sleepTable[i].m_steps > timeScaleSleepCount)
				{
					index = i;
					break;
				}
			}
			index--;

			bool state1 =
				(maxAccel < m_world->m_sleepTable[index].m_maxAccel) &&
				(maxAlpha < m_world->m_sleepTable[index].m_maxAlpha) &&
				(maxSpeed < m_world->m_sleepTable[index].m_maxVeloc) &&
				(maxOmega < m_world->m_sleepTable[index].m_maxOmega);
			if (state1)
			{
				for (dInt32 i = 0; i < count; i++)
				{
					ndBodyKinematic* const body = bodyIslands[i];
					body->m_veloc = dVector::m_zero;
					body->m_omega = dVector::m_zero;
					body->m_equilibrium = body->m_autoSleep;
					ndBodyDynamic* const dynBody = body->GetAsBodyDynamic();
					if (dynBody)
					{
						dynBody->m_accel = dVector::m_zero;
						dynBody->m_alpha = dVector::m_zero;
						dynBody->m_sleepingCounter = 0;
					}
				}
			}
		}
	}
}

dInt32 ndDynamicsUpdateSoa::CompareIslands(const ndIsland* const islandA, const ndIsland* const islandB, void* const)
{
	dUnsigned32 keyA = islandA->m_count * 2 + islandA->m_root->m_bodyIsConstrained;
	dUnsigned32 keyB = islandB->m_count * 2 + islandB->m_root->m_bodyIsConstrained;
	if (keyA < keyB)
	{
		return 1;
	}
	else if (keyA > keyB)
	{
		return -1;
	}
	return 0;
}

void ndDynamicsUpdateSoa::RadixSort()
{
	dInt32 elements = m_islands.GetCount();
	if (elements < 256)
	{
		dSort(&m_islands[0], elements, CompareIslands);
	}
	else
	{
		const dInt32 radixBits = 9;
		const dInt32 radix = (1 << radixBits) - 1;
		dInt32 histogram[2][1 << radixBits];

		memset(histogram, 0, sizeof(histogram));
		for (dInt32 i = 0; i < elements; i++)
		{
			dUnsigned32 key = m_islands[i].m_count * 2 + m_islands[i].m_root->m_bodyIsConstrained;
			dInt32 radix0 = radix - (key & radix);
			dInt32 radix1 = radix - (key >> radixBits);
			histogram[0][radix0]++;
			histogram[1][radix1]++;
		}

		dInt32 acc0 = 0;
		dInt32 acc1 = 0;
		for (dInt32 i = 0; i <= radix; i++)
		{
			const dInt32 n0 = histogram[0][i];
			const dInt32 n1 = histogram[1][i];
			histogram[0][i] = acc0;
			histogram[1][i] = acc1;
			acc0 += n0;
			acc1 += n1;
		}

		//ndIsland* const buffer = dAlloca(ndIsland, elements);
		ndIsland* const buffer = (ndIsland*)GetTempBuffer();

		dInt32* const scan0 = &histogram[0][0];
		for (dInt32 i = 0; i < elements; i++)
		{
			dUnsigned32 key = radix - ((m_islands[i].m_count * 2 + m_islands[i].m_root->m_bodyIsConstrained) & radix);
			const dInt32 index = scan0[key];
			buffer[index] = m_islands[i];
			scan0[key] = index + 1;
		}

		dInt32* const scan1 = &histogram[1][0];
		for (dInt32 i = 0; i < elements; i++)
		{
			dUnsigned32 key = radix - ((buffer[i].m_count * 2 + buffer[i].m_root->m_bodyIsConstrained) >> radixBits);
			const dInt32 index = scan1[key];
			m_islands[index] = buffer[i];
			scan1[key] = index + 1;
		}

#ifdef _DEBUG
		for (dInt32 i = elements - 1; i; i--)
		{
			dUnsigned32 key0 = m_islands[i - 1].m_count * 2 + m_islands[i - 1].m_root->m_bodyIsConstrained;
			dUnsigned32 key1 = m_islands[i + 0].m_count * 2 + m_islands[i + 0].m_root->m_bodyIsConstrained;
			dAssert(key0 >= key1);
		}
#endif
	}
}

void ndDynamicsUpdateSoa::SortJoints()
{
	D_TRACKTIME();

	class ndSleep0 : public ndScene::ndBaseJob
	{
		public:
		virtual void Execute()
		{
			D_TRACKTIME();
			ndWorld* const world = m_owner->GetWorld();
			ndScene* const scene = world->GetScene();
			ndConstraintArray& jointArray = scene->GetActiveContactArray();
			const dInt32 count = jointArray.GetCount();
			const dInt32 threadIndex = GetThreadId();
			const dInt32 threadCount = m_owner->GetThreadCount();

			const dInt32 stride = count / threadCount;
			const dInt32 start = threadIndex * stride;
			const dInt32 blockSize = (threadIndex != (threadCount - 1)) ? stride : count - start;

			for (dInt32 i = 0; i < blockSize; i++)
			{
				ndConstraint* const joint = jointArray[i + start];
				ndBodyKinematic* const body0 = joint->GetBody0();
				ndBodyKinematic* const body1 = joint->GetBody1();

				const dInt32 rows = joint->GetRowsCount();
				joint->m_rowCount = rows;

				const dInt32 equilibrium = body0->m_equilibrium & body1->m_equilibrium;
				if (!equilibrium)
				{
					body0->m_solverSleep0 = 0;
					if (body1->GetInvMass() > dFloat32(0.0f))
					{
						body1->m_solverSleep0 = 0;
					}
				}

				body0->m_bodyIsConstrained = 1;
				body0->m_resting = body0->m_resting & equilibrium;
				if (body1->GetInvMass() > dFloat32(0.0f))
				{
					body1->m_bodyIsConstrained = 1;
					body1->m_resting = body1->m_resting & equilibrium;
				}
			}
		}
	};

	class ndSleep1 : public ndScene::ndBaseJob
	{
		public:
		virtual void Execute()
		{
			D_TRACKTIME();
			ndWorld* const world = m_owner->GetWorld();
			ndScene* const scene = world->GetScene();
			ndConstraintArray& jointArray = scene->GetActiveContactArray();
			const dInt32 count = jointArray.GetCount();
			const dInt32 threadIndex = GetThreadId();
			const dInt32 threadCount = m_owner->GetThreadCount();
			const dInt32 stride = count / threadCount;
			const dInt32 start = threadIndex * stride;
			const dInt32 blockSize = (threadIndex != (threadCount - 1)) ? stride : count - start;

			dInt32 activeJointCount = 0;
			for (dInt32 i = 0; i < blockSize; i++)
			{
				ndConstraint* const joint = jointArray[i + start];
				ndBodyKinematic* const body0 = joint->GetBody0();
				ndBodyKinematic* const body1 = joint->GetBody1();

				const dInt32 resting = body0->m_resting & body1->m_resting;
				activeJointCount += (1 - resting);
				joint->m_resting = resting;

				const dInt32 solverSleep0 = body0->m_solverSleep0 & body1->m_solverSleep0;
				if (!solverSleep0)
				{
					body0->m_solverSleep1 = 0;
					if (body1->GetInvMass() > dFloat32(0.0f))
					{
						body1->m_solverSleep1 = 0;
					}
				}
			}
			*(((dInt32*)m_context) + threadIndex) = activeJointCount;
		}
	};

	class ndScan0 : public ndScene::ndBaseJob
	{
		public:
		virtual void Execute()
		{
			D_TRACKTIME();
			ndWorld* const world = m_owner->GetWorld();
			ndScene* const scene = world->GetScene();
			ndDynamicsUpdate* const me = world->m_solver;
			ndConstraintArray& jointArray = scene->GetActiveContactArray();
			const dInt32 count = jointArray.GetCount();
			const dInt32 threadIndex = GetThreadId();
			const dInt32 threadCount = m_owner->GetThreadCount();
			const dInt32 stride = count / threadCount;
			const dInt32 start = threadIndex * stride;
			const dInt32 blockSize = (threadIndex != (threadCount - 1)) ? stride : count - start;

			dInt32* const histogram = ((dInt32*)m_context) + 2 * threadIndex;
			ndConstraint** const sortBuffer = (ndConstraint**)me->GetTempBuffer();

			histogram[0] = 0;
			histogram[1] = 0;
			for (dInt32 i = 0; i < blockSize; i++)
			{
				ndConstraint* const joint = jointArray[i + start];
				sortBuffer[i + start] = joint;
				ndBodyKinematic* const body0 = joint->GetBody0();
				ndBodyKinematic* const body1 = joint->GetBody1();
				const dInt32 key = body0->m_solverSleep1 & body1->m_solverSleep1;
				histogram[key]++;
			}
		}
	};

	class ndSort0 : public ndScene::ndBaseJob
	{
		public:
		virtual void Execute()
		{
			D_TRACKTIME();
			ndWorld* const world = m_owner->GetWorld();
			ndScene* const scene = world->GetScene();
			ndDynamicsUpdate* const me = world->m_solver;
			ndConstraintArray& jointArray = scene->GetActiveContactArray();
			const dInt32 count = jointArray.GetCount();
			const dInt32 threadIndex = GetThreadId();
			const dInt32 threadCount = m_owner->GetThreadCount();
			const dInt32 stride = count / threadCount;
			const dInt32 start = threadIndex * stride;
			const dInt32 blockSize = (threadIndex != (threadCount - 1)) ? stride : count - start;

			dInt32* const histogram = ((dInt32*)m_context) + 2 * threadIndex;
			ndConstraint** const sortBuffer = (ndConstraint**)me->GetTempBuffer();
			for (dInt32 i = 0; i < blockSize; i++)
			{
				ndConstraint* const joint = sortBuffer[i + start];
				ndBodyKinematic* const body0 = joint->GetBody0();
				ndBodyKinematic* const body1 = joint->GetBody1();
				const dInt32 key = body0->m_solverSleep1 & body1->m_solverSleep1;
				const dInt32 entry = histogram[key];
				jointArray[entry] = joint;
				histogram[key] = entry + 1;
			}
		}
	};

	class ndRowScan : public ndScene::ndBaseJob
	{
		public:
		virtual void Execute()
		{
			D_TRACKTIME();
			ndWorld* const world = m_owner->GetWorld();
			ndScene* const scene = world->GetScene();
			ndDynamicsUpdate* const me = world->m_solver;
			ndConstraintArray& jointArray = scene->GetActiveContactArray();
			const dInt32 count = jointArray.GetCount();
			const dInt32 threadIndex = GetThreadId();
			const dInt32 threadCount = m_owner->GetThreadCount();
			const dInt32 stride = count / threadCount;
			const dInt32 start = threadIndex * stride;
			const dInt32 blockSize = (threadIndex != (threadCount - 1)) ? stride : count - start;

			dInt32* const jointCountSpans = ((dInt32*)m_context) + 128 * threadIndex;
			ndConstraint** const sortBuffer = (ndConstraint**)me->GetTempBuffer();

			memset(jointCountSpans, 0, sizeof(dInt32) * 128);
			for (dInt32 i = 0; i < blockSize; i++)
			{
				ndConstraint* const joint = jointArray[i + start];
				sortBuffer[i + start] = joint;

				dAssert((joint->GetBody0()->m_resting & joint->GetBody1()->m_resting) == joint->m_resting);
				const ndSortKey key(joint->m_resting, joint->m_rowCount);

				dAssert(key.m_value >= 0);
				dAssert(key.m_value <= 127);
				jointCountSpans[key.m_value] ++;
			}
		}
	};

	class ndSortByRows : public ndScene::ndBaseJob
	{
		public:
		virtual void Execute()
		{
			D_TRACKTIME();
			ndWorld* const world = m_owner->GetWorld();
			ndScene* const scene = world->GetScene();
			ndDynamicsUpdate* const me = world->m_solver;
			ndConstraintArray& jointArray = scene->GetActiveContactArray();
			const dInt32 count = jointArray.GetCount();
			const dInt32 threadIndex = GetThreadId();
			const dInt32 threadCount = m_owner->GetThreadCount();
			const dInt32 stride = count / threadCount;
			const dInt32 start = threadIndex * stride;
			const dInt32 blockSize = (threadIndex != (threadCount - 1)) ? stride : count - start;

			dInt32* const jointCountSpans = ((dInt32*)m_context) + 128 * threadIndex;
			ndConstraint** const sortBuffer = (ndConstraint**)me->GetTempBuffer();
			for (dInt32 i = 0; i < blockSize; i++)
			{
				ndConstraint* const joint = sortBuffer[i + start];
				dAssert((joint->GetBody0()->m_resting & joint->GetBody1()->m_resting) == joint->m_resting);
				const ndSortKey key(joint->m_resting, joint->m_rowCount);
				dAssert(key.m_value >= 0);
				dAssert(key.m_value <= 127);

				const dInt32 entry = jointCountSpans[key.m_value];
				jointArray[entry] = joint;
				jointCountSpans[key.m_value] = entry + 1;
			}
		}
	};

	class ndSetRowStarts : public ndScene::ndBaseJob
	{
		public:
		class ndRowsCount
		{
			public:
			dInt32 m_rowsCount;
			dInt32 m_soaJointRowCount;
		};

		void SetRowsCount()
		{
			ndWorld* const world = m_owner->GetWorld();
			ndScene* const scene = world->GetScene();
			ndConstraintArray& jointArray = scene->GetActiveContactArray();
			const dInt32 count = jointArray.GetCount();
		
			dInt32 rowCount = 1;
			for (dInt32 i = 0; i < count; i++)
			{
				ndConstraint* const joint = jointArray[i];
				joint->m_rowStart = rowCount;
				rowCount += joint->m_rowCount;
			}
			ndRowsCount* const counters = (ndRowsCount*)m_context;
			counters->m_rowsCount = rowCount;
		}

		void SetSoaRowsCount()
		{
			ndWorld* const world = m_owner->GetWorld();
			ndScene* const scene = world->GetScene();
			ndDynamicsUpdateSoa* const me = (ndDynamicsUpdateSoa*)world->m_solver;
			ndConstraintArray& jointArray = scene->GetActiveContactArray();
			const dInt32 count = jointArray.GetCount();

			dInt32 soaJointRowCount = 0;
			dArray<dInt32>& soaJointRows = me->m_soaJointRows;
			dInt32 soaJointCountBatches = soaJointRows.GetCount();
			for (dInt32 i = 0; i < soaJointCountBatches; i++)
			{
				const ndConstraint* const joint = jointArray[i * D_SSE_WORK_GROUP];
				soaJointRows[i] = soaJointRowCount;
				soaJointRowCount += joint->m_rowCount;
			}

			ndRowsCount* const counters = (ndRowsCount*)m_context;
			counters->m_soaJointRowCount = soaJointRowCount;
		}

		virtual void Execute()
		{
			D_TRACKTIME();
			const dInt32 threadIndex = GetThreadId();
			const dInt32 threadCount = m_owner->GetThreadCount();

			if (threadCount == 1)
			{
				SetRowsCount();
				SetSoaRowsCount();
			}
			else if (threadIndex == 0)
			{
				SetRowsCount();
			}
			else if (threadIndex == (threadCount - 1))
			{
				SetSoaRowsCount();
			}
		}
	};

	ndScene* const scene = m_world->GetScene();
	for (ndSkeletonList::dNode* node = m_world->GetSkeletonList().GetFirst(); node; node = node->GetNext())
	{
		ndSkeletonContainer* const skeleton = &node->GetInfo();
		skeleton->CheckSleepState();
	}

	const ndJointList& jointList = m_world->GetJointList();
	ndConstraintArray& jointArray = scene->GetActiveContactArray();

	dInt32 index = jointArray.GetCount();
	jointArray.SetCount(index + jointList.GetCount());

	for (ndJointList::dNode* node = jointList.GetFirst(); node; node = node->GetNext())
	{
		ndJointBilateralConstraint* const joint = node->GetInfo();
		if (joint->IsActive())
		{
			jointArray[index] = joint;
			index++;
		}
	}
	jointArray.SetCount(index);
	m_leftHandSide.SetCount(jointArray.GetCount() + 32);

	dInt32 histogram[D_MAX_THREADS_COUNT][2];
	dInt32 movingJoints[D_MAX_THREADS_COUNT];
	const dInt32 threadCount = scene->GetThreadCount();

	scene->SubmitJobs<ndSleep0>();
	scene->SubmitJobs<ndSleep1>(movingJoints);
	scene->SubmitJobs<ndScan0>(histogram);

	dInt32 scan[2];
	scan[0] = 0;
	scan[1] = 0;
	dInt32 movingJointCount = 0;
	for (dInt32 i = 0; i < threadCount; i++)
	{
		scan[0] += histogram[i][0];
		scan[1] += histogram[i][1];
		movingJointCount += movingJoints[i];
	}

	m_activeJointCount = scan[0];
	dAssert(m_activeJointCount <= jointArray.GetCount());
	if (!m_activeJointCount)
	{
		jointArray.SetCount(0);
		return;
	}

	dInt32 sum = 0;
	for (dInt32 i = 0; i < 2; i++)
	{
		for (dInt32 j = 0; j < threadCount; j++)
		{
			dInt32 partialSum = histogram[j][i];
			histogram[j][i] = sum;
			sum += partialSum;
		}
	}
	scene->SubmitJobs<ndSort0>(histogram);

	#ifdef _DEBUG
		for (dInt32 i = 0; i < (jointArray.GetCount() - 1); i++)
		{
			const ndConstraint* const joint0 = jointArray[i];
			const ndConstraint* const joint1 = jointArray[i + 1];
			const dInt32 key0 = (joint0->GetBody0()->m_solverSleep1 & joint0->GetBody1()->m_solverSleep1) ? 1 : 0;
			const dInt32 key1 = (joint1->GetBody0()->m_solverSleep1 & joint1->GetBody1()->m_solverSleep1) ? 1 : 0;
			dAssert(key0 <= key1);
		}
	#endif

	dAssert(m_activeJointCount <= jointArray.GetCount());
	jointArray.SetCount(m_activeJointCount);

	for (dInt32 i = jointArray.GetCount() - 1; i >= 0; i--)
	{
		ndConstraint* const joint = jointArray[i];
		ndBodyKinematic* const body0 = joint->GetBody0();
		ndBodyKinematic* const body1 = joint->GetBody1();

		if (body1->GetInvMass() > dFloat32(0.0f))
		{
			ndBodyKinematic* root0 = FindRootAndSplit(body0);
			ndBodyKinematic* root1 = FindRootAndSplit(body1);
			if (root0 != root1)
			{
				if (root0->m_rank > root1->m_rank)
				{
					dSwap(root0, root1);
				}
				root0->m_islandParent = root1;
				if (root0->m_rank == root1->m_rank)
				{
					root1->m_rank += 1;
					dAssert(root1->m_rank <= 6);
				}
			}

			const dInt32 sleep = body0->m_islandSleep & body1->m_islandSleep;
			if (!sleep)
			{
				dAssert(root1->m_islandParent == root1);
				root1->m_islandSleep = 0;
			}
		}
		else
		{
			if (!body0->m_islandSleep)
			{
				ndBodyKinematic* const root = FindRootAndSplit(body0);
				root->m_islandSleep = 0;
			}
		}
	}
	m_activeJointCount = movingJointCount;

	dInt32 jointRowScans[D_MAX_THREADS_COUNT][128];
	scene->SubmitJobs<ndRowScan>(jointRowScans);
	dInt32 acc = 0;
	for (dInt32 i = 0; i < 128; i++)
	{
		for (dInt32 j = 0; j < threadCount; j++)
		{
			dInt32 temp = jointRowScans[j][i];
			jointRowScans[j][i] = acc;
			acc += temp;
		}
	}
	scene->SubmitJobs<ndSortByRows>(jointRowScans);

	#ifdef _DEBUG
		for (dInt32 i = 1; i < m_activeJointCount; i++)
		{
			ndConstraint* const joint0 = jointArray[i - 1];
			ndConstraint* const joint1 = jointArray[i - 0];
			dAssert(!joint0->m_resting);
			dAssert(!joint1->m_resting);
			dAssert(joint0->m_rowCount >= joint1->m_rowCount);
			dAssert(!(joint0->GetBody0()->m_resting & joint0->GetBody1()->m_resting));
			dAssert(!(joint1->GetBody0()->m_resting & joint1->GetBody1()->m_resting));
		}

		for (dInt32 i = m_activeJointCount + 1; i < jointArray.GetCount(); i++)
		{
			ndConstraint* const joint0 = jointArray[i - 1];
			ndConstraint* const joint1 = jointArray[i - 0];
			dAssert(joint0->m_resting);
			dAssert(joint1->m_resting);
			dAssert(joint0->m_rowCount >= joint1->m_rowCount);
			dAssert(joint0->GetBody0()->m_resting & joint0->GetBody1()->m_resting);
			dAssert(joint1->GetBody0()->m_resting & joint1->GetBody1()->m_resting);
		}
	#endif

	const dInt32 mask = -dInt32(D_SSE_WORK_GROUP);
	const dInt32 jointCount = jointArray.GetCount();
	const dInt32 soaJointCount = (jointCount + D_SSE_WORK_GROUP - 1) & mask;
	dAssert(jointArray.GetCapacity() > soaJointCount);
	ndConstraint** const jointArrayPtr = &jointArray[0];
	for (dInt32 i = jointCount; i < soaJointCount; i++)
	{
		jointArrayPtr[i] = nullptr;
	}

	if (m_activeJointCount - jointArray.GetCount())
	{
		const dInt32 base = m_activeJointCount & mask;
		const dInt32 count = jointArrayPtr[base + D_SSE_WORK_GROUP - 1] ? D_SSE_WORK_GROUP : jointArray.GetCount() - base;
		dAssert(count <= D_SSE_WORK_GROUP);
		ndConstraint** const array = &jointArrayPtr[base];
		for (dInt32 j = 1; j < count; j++)
		{
			dInt32 slot = j;
			ndConstraint* const joint = array[slot];
			for (; (slot > 0) && (array[slot - 1]->m_rowCount < joint->m_rowCount); slot--)
			{
				array[slot] = array[slot - 1];
			}
			array[slot] = joint;
		}
	}

	ndSetRowStarts::ndRowsCount rowsCount;
	const dInt32 soaJointCountBatches = soaJointCount / D_SSE_WORK_GROUP;
	m_soaJointRows.SetCount(soaJointCountBatches);
	scene->SubmitJobs<ndSetRowStarts>(&rowsCount);

	m_leftHandSide.SetCount(rowsCount.m_rowsCount);
	m_rightHandSide.SetCount(rowsCount.m_rowsCount);
	m_soaMassMatrix.SetCount(rowsCount.m_soaJointRowCount);

	#ifdef _DEBUG
		dAssert(m_activeJointCount <= jointArray.GetCount());
		const dInt32 maxRowCount = m_leftHandSide.GetCount();
		for (dInt32 i = 0; i < jointArray.GetCount(); i++)
		{
			ndConstraint* const joint = jointArray[i];
			dAssert(joint->m_rowStart < m_leftHandSide.GetCount());
			dAssert((joint->m_rowStart + joint->m_rowCount) <= maxRowCount);
		}

		for (dInt32 i = 0; i < jointCount; i += D_SSE_WORK_GROUP)
		{
			const dInt32 count = jointArrayPtr[i + D_SSE_WORK_GROUP - 1] ? D_SSE_WORK_GROUP : jointCount - i;
			for (dInt32 j = 1; j < count; j++)
			{
				ndConstraint* const joint0 = jointArrayPtr[i + j - 1];
				ndConstraint* const joint1 = jointArrayPtr[i + j - 0];
				dAssert(joint0->m_rowCount >= joint1->m_rowCount);
			}
		}
	#endif
}

void ndDynamicsUpdateSoa::SortIslands()
{
	D_TRACKTIME();
	ndScene* const scene = m_world->GetScene();
	const dArray<ndBodyKinematic*>& bodyArray = scene->GetActiveBodyArray();

	GetInternalForces().SetCount(bodyArray.GetCount());
	GetTempInternalForces().SetCount(bodyArray.GetCount());

	dInt32 count = 0;
	ndBodyIndexPair* const buffer0 = (ndBodyIndexPair*)&GetInternalForces()[0];
	for (dInt32 i = bodyArray.GetCount() - 2; i >= 0; i--)
	{
		ndBodyKinematic* const body = bodyArray[i];
		if (!(body->m_resting & body->m_islandSleep) || body->GetAsBodyPlayerCapsule())
		{
			buffer0[count].m_body = body;
			if (body->GetInvMass() > dFloat32(0.0f))
			{
				ndBodyKinematic* root = body->m_islandParent;
				while (root != root->m_islandParent)
				{
					root = root->m_islandParent;
				}

				buffer0[count].m_root = root;
				if (root->m_rank != -1)
				{
					root->m_rank = -1;
				}
			}
			else
			{
				buffer0[count].m_root = body;
				body->m_rank = -1;
			}
			count++;
		}
	}

	m_islands.SetCount(0);
	m_bodyIslandOrder.SetCount(count);
	m_unConstrainedBodyCount = 0;
	if (count)
	{
		// sort using counting sort o(n)
		dInt32 scans[2];
		scans[0] = 0;
		scans[1] = 0;
		for (dInt32 i = 0; i < count; i++)
		{
			dInt32 j = 1 - buffer0[i].m_root->m_bodyIsConstrained;
			scans[j] ++;
		}
		scans[1] = scans[0];
		scans[0] = 0;
		ndBodyIndexPair* const buffer2 = buffer0 + count;
		for (dInt32 i = 0; i < count; i++)
		{
			const dInt32 key = 1 - buffer0[i].m_root->m_bodyIsConstrained;
			const dInt32 j = scans[key];
			buffer2[j] = buffer0[i];
			scans[key] = j + 1;
		}

		const ndBodyIndexPair* const buffer1 = buffer0 + count;
		for (dInt32 i = 0; i < count; i++)
		{
			dAssert((i == count - 1) || (buffer1[i].m_root->m_bodyIsConstrained >= buffer1[i + 1].m_root->m_bodyIsConstrained));

			m_bodyIslandOrder[i] = buffer1[i].m_body;
			if (buffer1[i].m_root->m_rank == -1)
			{
				buffer1[i].m_root->m_rank = 0;
				ndIsland island(buffer1[i].m_root);
				m_islands.PushBack(island);
			}
			buffer1[i].m_root->m_rank += 1;
		}

		dInt32 start = 0;
		dInt32 unConstrainedCount = 0;
		for (dInt32 i = 0; i < m_islands.GetCount(); i++)
		{
			ndIsland& island = m_islands[i];
			island.m_start = start;
			island.m_count = island.m_root->m_rank;
			start += island.m_count;
			unConstrainedCount += island.m_root->m_bodyIsConstrained ? 0 : 1;
		}

		m_unConstrainedBodyCount = unConstrainedCount;
		RadixSort();
	}
}

void ndDynamicsUpdateSoa::BuildIsland()
{
	ndScene* const scene = m_world->GetScene();
	const dArray<ndBodyKinematic*>& bodyArray = scene->GetActiveBodyArray();
	dAssert(bodyArray.GetCount() >= 1);
	if (bodyArray.GetCount() - 1)
	{
		D_TRACKTIME();
		SortJoints();
		SortIslands();
	}
}

void ndDynamicsUpdateSoa::IntegrateUnconstrainedBodies()
{
	class ndIntegrateUnconstrainedBodies : public ndScene::ndBaseJob
	{
		public:
		virtual void Execute()
		{
			D_TRACKTIME();
			ndWorld* const world = m_owner->GetWorld();
			ndDynamicsUpdateSoa* const me = (ndDynamicsUpdateSoa*)world->m_solver;
			dArray<ndBodyKinematic*>& bodyArray = me->GetBodyIslandOrder();

			const dFloat32 timestep = m_timestep;
			const dInt32 threadIndex = GetThreadId();
			const dInt32 threadCount = m_owner->GetThreadCount();
			const dInt32 bodyCount = me->GetUnconstrainedBodyCount();
			const dInt32 stride = bodyCount / threadCount;
			const dInt32 start = threadIndex * stride;
			const dInt32 blockSize = (threadIndex != (threadCount - 1)) ? stride : bodyCount - start;
			const dInt32 base = bodyArray.GetCount() - bodyCount + start;

			for (dInt32 i = 0; i < blockSize; i++)
			{
				ndBodyKinematic* const body = bodyArray[base + i]->GetAsBodyKinematic();
				dAssert(body);
				body->UpdateInvInertiaMatrix();
				body->AddDampingAcceleration(m_timestep);
				body->IntegrateExternalForce(timestep);
			}
		}
	};

	if (m_unConstrainedBodyCount)
	{
		D_TRACKTIME();
		ndScene* const scene = m_world->GetScene();
		scene->SubmitJobs<ndIntegrateUnconstrainedBodies>();
	}
}

void ndDynamicsUpdateSoa::IntegrateBodies()
{
	D_TRACKTIME();
	class ndIntegrateBodies : public ndScene::ndBaseJob
	{
		public:
		virtual void Execute()
		{
			D_TRACKTIME();
			ndWorld* const world = m_owner->GetWorld();
			ndDynamicsUpdateSoa* const me = (ndDynamicsUpdateSoa*)world->m_solver;
			dArray<ndBodyKinematic*>& bodyArray = me->m_bodyIslandOrder;

			const dFloat32 timestep = m_timestep;
			const dVector invTime(me->m_invTimestep);

			const dInt32 threadIndex = GetThreadId();
			const dInt32 threadCount = m_owner->GetThreadCount();
			const dInt32 bodyCount = bodyArray.GetCount();
			const dInt32 stride = bodyCount / threadCount;
			const dInt32 start = threadIndex * stride;
			const dInt32 blockSize = (threadIndex != (threadCount - 1)) ? stride : bodyCount - start;

			for (dInt32 i = 0; i < blockSize; i++)
			{
				ndBodyDynamic* const dynBody = bodyArray[i + start]->GetAsBodyDynamic();

				// the initial velocity and angular velocity were stored in m_accel and dynBody->m_alpha for memory saving
				if (dynBody)
				{
					if (!dynBody->m_equilibrium)
					{
						dynBody->m_accel = invTime * (dynBody->m_veloc - dynBody->m_accel);
						dynBody->m_alpha = invTime * (dynBody->m_omega - dynBody->m_alpha);
						dynBody->IntegrateVelocity(timestep);
					}
				}
				else
				{
					ndBodyKinematic* const kinBody = bodyArray[i]->GetAsBodyKinematic();
					dAssert(kinBody);
					if (!kinBody->m_equilibrium)
					{
						kinBody->IntegrateVelocity(timestep);
					}
				}
			}
		}
	};

	ndScene* const scene = m_world->GetScene();
	scene->SubmitJobs<ndIntegrateBodies>();
}

void ndDynamicsUpdateSoa::InitWeights()
{
	D_TRACKTIME();
	class ndInitWeights : public ndScene::ndBaseJob
	{
		public:
		virtual void Execute()
		{
			D_TRACKTIME();
			const ndConstraintArray& jointArray = m_owner->GetActiveContactArray();

			dFloat32 maxExtraPasses = dFloat32(1.0f);
			const dInt32 threadIndex = GetThreadId();
			const dInt32 threadCount = m_owner->GetThreadCount();
			const dInt32 jointCount = jointArray.GetCount();

			const dInt32 stride = jointCount / threadCount;
			const dInt32 start = threadIndex * stride;
			const dInt32 blockSize = (threadIndex != (threadCount - 1)) ? stride : jointCount - start;

			for (dInt32 i = 0; i < blockSize; i++)
			{
				ndConstraint* const constraint = jointArray[i + start];
				ndBodyKinematic* const body0 = constraint->GetBody0();
				ndBodyKinematic* const body1 = constraint->GetBody1();

				if (body1->GetInvMass() > dFloat32(0.0f))
				{
					dScopeSpinLock lock(body1->m_lock);
					body1->m_weigh += dFloat32(1.0f);
					maxExtraPasses = dMax(body1->m_weigh, maxExtraPasses);
				}
				else if (body1->m_weigh != dFloat32(1.0f))
				{
					body1->m_weigh = dFloat32(1.0f);
				}
				dScopeSpinLock lock(body0->m_lock);
				body0->m_weigh += dFloat32(1.0f);
				dAssert(body0->GetInvMass() != dFloat32(0.0f));
				maxExtraPasses = dMax(body0->m_weigh, maxExtraPasses);
			}

			dFloat32* const extraPasses = (dFloat32*)m_context;
			extraPasses[threadIndex] = maxExtraPasses;
		}
	};

	ndScene* const scene = m_world->GetScene();
	m_invTimestep = dFloat32(1.0f) / m_timestep;
	m_invStepRK = dFloat32(0.25f);
	m_timestepRK = m_timestep * m_invStepRK;
	m_invTimestepRK = m_invTimestep * dFloat32(4.0f);

	const dArray<ndBodyKinematic*>& bodyArray = scene->GetActiveBodyArray();
	const dInt32 bodyCount = bodyArray.GetCount();
	GetInternalForces().SetCount(bodyCount);
	GetTempInternalForces().SetCount(bodyCount);

	dFloat32 extraPassesArray[D_MAX_THREADS_COUNT];
	memset(extraPassesArray, 0, sizeof(extraPassesArray));
	scene->SubmitJobs<ndInitWeights>(extraPassesArray);

	dFloat32 extraPasses = dFloat32(0.0f);
	const dInt32 threadCount = scene->GetThreadCount();
	for (dInt32 i = 0; i < threadCount; i++)
	{
		extraPasses = dMax(extraPasses, extraPassesArray[i]);
	}

	const dInt32 conectivity = 7;
	m_solverPasses = m_world->GetSolverIterations() + 2 * dInt32(extraPasses) / conectivity + 1;
}

void ndDynamicsUpdateSoa::InitBodyArray()
{
	D_TRACKTIME();
	class ndInitBodyArray : public ndScene::ndBaseJob
	{
		public:
		virtual void Execute()
		{
			D_TRACKTIME();
			ndWorld* const world = m_owner->GetWorld();
			ndDynamicsUpdateSoa* const me = (ndDynamicsUpdateSoa*)world->m_solver;
			dArray<ndBodyKinematic*>& bodyArray = me->GetBodyIslandOrder();

			const dInt32 threadIndex = GetThreadId();
			const dInt32 threadCount = m_owner->GetThreadCount();
			const dInt32 bodyCount = bodyArray.GetCount() - me->GetUnconstrainedBodyCount();
			const dInt32 stride = bodyCount / threadCount;
			const dInt32 start = threadIndex * stride;
			const dInt32 blockSize = (threadIndex != (threadCount - 1)) ? stride : bodyCount - start;

			for (dInt32 i = 0; i < blockSize; i++)
			{
				ndBodyDynamic* const body = bodyArray[i + start]->GetAsBodyDynamic();
				if (body)
				{
					dAssert(body->m_bodyIsConstrained);
					body->UpdateInvInertiaMatrix();
					body->AddDampingAcceleration(m_timestep);

					const dVector localOmega(body->m_matrix.UnrotateVector(body->m_omega));
					const dVector localAngularMomentum(body->m_mass * localOmega);
					const dVector angularMomentum(body->m_matrix.RotateVector(localAngularMomentum));
					body->m_gyroTorque = body->m_omega.CrossProduct(angularMomentum);
					body->m_gyroAlpha = body->m_invWorldInertiaMatrix.RotateVector(body->m_gyroTorque);

					body->m_accel = body->m_veloc;
					body->m_alpha = body->m_omega;
					body->m_gyroRotation = body->m_rotation;
				}
			}
		}
	};

	ndScene* const scene = m_world->GetScene();
	scene->SubmitJobs<ndInitBodyArray>();
}

void ndDynamicsUpdateSoa::GetJacobianDerivatives(ndConstraint* const joint)
{
	ndConstraintDescritor constraintParam;
	dAssert(joint->GetRowsCount() <= D_CONSTRAINT_MAX_ROWS);
	for (dInt32 i = joint->GetRowsCount() - 1; i >= 0; i--)
	{
		constraintParam.m_forceBounds[i].m_low = D_MIN_BOUND;
		constraintParam.m_forceBounds[i].m_upper = D_MAX_BOUND;
		constraintParam.m_forceBounds[i].m_jointForce = nullptr;
		constraintParam.m_forceBounds[i].m_normalIndex = D_INDEPENDENT_ROW;
	}

	constraintParam.m_rowsCount = 0;
	constraintParam.m_timestep = m_timestep;
	constraintParam.m_invTimestep = m_invTimestep;
	joint->JacobianDerivative(constraintParam);
	const dInt32 dof = constraintParam.m_rowsCount;
	dAssert(dof <= joint->m_rowCount);

	if (joint->GetAsContact())
	{
		ndContact* const contactJoint = joint->GetAsContact();
		contactJoint->m_isInSkeletonLoop = 0;
		ndSkeletonContainer* const skeleton0 = contactJoint->GetBody0()->GetSkeleton();
		ndSkeletonContainer* const skeleton1 = contactJoint->GetBody1()->GetSkeleton();
		if (skeleton0 && (skeleton0 == skeleton1))
		{
			if (contactJoint->IsSkeletonSelftCollision())
			{
				contactJoint->m_isInSkeletonLoop = 1;
				skeleton0->AddSelfCollisionJoint(contactJoint);
			}
		}
		else if (contactJoint->IsSkeletonIntraCollision())
		{
			if (skeleton0 && !skeleton1)
			{
				contactJoint->m_isInSkeletonLoop = 1;
				skeleton0->AddSelfCollisionJoint(contactJoint);
			}
			else if (skeleton1 && !skeleton0)
			{
				contactJoint->m_isInSkeletonLoop = 1;
				skeleton1->AddSelfCollisionJoint(contactJoint);
			}
		}
	}
	else
	{
		ndJointBilateralConstraint* const bilareral = joint->GetAsBilateral();
		dAssert(bilareral);
		if (!bilareral->m_isInSkeleton && (bilareral->GetSolverModel() == m_jointkinematicAttachment))
		{
			ndSkeletonContainer* const skeleton0 = bilareral->m_body0->GetSkeleton();
			ndSkeletonContainer* const skeleton1 = bilareral->m_body1->GetSkeleton();
			if (skeleton0 || skeleton1)
			{
				if (skeleton0 && !skeleton1)
				{
					bilareral->m_isInSkeletonLoop = 1;
					skeleton0->AddSelfCollisionJoint(bilareral);
				}
				else if (skeleton1 && !skeleton0)
				{
					bilareral->m_isInSkeletonLoop = 1;
					skeleton1->AddSelfCollisionJoint(bilareral);
				}
			}
		}
	}

	joint->m_rowCount = dof;
	const dInt32 baseIndex = joint->m_rowStart;
	for (dInt32 i = 0; i < dof; i++)
	{
		dAssert(constraintParam.m_forceBounds[i].m_jointForce);

		ndLeftHandSide* const row = &m_leftHandSide[baseIndex + i];
		ndRightHandSide* const rhs = &m_rightHandSide[baseIndex + i];

		row->m_Jt = constraintParam.m_jacobian[i];
		rhs->m_diagDamp = dFloat32(0.0f);
		rhs->m_diagonalRegularizer = dMax(constraintParam.m_diagonalRegularizer[i], dFloat32(1.0e-5f));

		rhs->m_coordenateAccel = constraintParam.m_jointAccel[i];
		rhs->m_restitution = constraintParam.m_restitution[i];
		rhs->m_penetration = constraintParam.m_penetration[i];
		rhs->m_penetrationStiffness = constraintParam.m_penetrationStiffness[i];
		rhs->m_lowerBoundFrictionCoefficent = constraintParam.m_forceBounds[i].m_low;
		rhs->m_upperBoundFrictionCoefficent = constraintParam.m_forceBounds[i].m_upper;
		rhs->m_jointFeebackForce = constraintParam.m_forceBounds[i].m_jointForce;

		dAssert(constraintParam.m_forceBounds[i].m_normalIndex >= -1);
		rhs->m_normalForceIndex = constraintParam.m_forceBounds[i].m_normalIndex;
	}
}

void ndDynamicsUpdateSoa::InitJacobianMatrix()
{
	class ndInitJacobianMatrix : public ndScene::ndBaseJob
	{
		public:
		ndInitJacobianMatrix()
			:m_zero(dVector::m_zero)
		{
		}

		void BuildJacobianMatrix(ndConstraint* const joint)
		{
			dAssert(joint->GetBody0());
			dAssert(joint->GetBody1());
			ndBodyKinematic* const body0 = joint->GetBody0();
			ndBodyKinematic* const body1 = joint->GetBody1();
			const ndBodyDynamic* const dynBody0 = body0->GetAsBodyDynamic();
			const ndBodyDynamic* const dynBody1 = body1->GetAsBodyDynamic();

			const dInt32 m0 = body0->m_index;
			const dInt32 m1 = body1->m_index;
			const dInt32 index = joint->m_rowStart;
			const dInt32 count = joint->m_rowCount;

			const bool isBilateral = joint->IsBilateral();

			const dMatrix& invInertia0 = body0->m_invWorldInertiaMatrix;
			const dMatrix& invInertia1 = body1->m_invWorldInertiaMatrix;
			const dVector invMass0(body0->m_invMass[3]);
			const dVector invMass1(body1->m_invMass[3]);

			dVector force0(m_zero);
			dVector torque0(m_zero);
			if (dynBody0)
			{
				force0 = dynBody0->m_externalForce;
				torque0 = dynBody0->m_externalTorque;
			}

			dVector force1(m_zero);
			dVector torque1(m_zero);
			if (dynBody1)
			{
				force1 = dynBody1->m_externalForce;
				torque1 = dynBody1->m_externalTorque;
			}

			joint->m_preconditioner0 = dFloat32(1.0f);
			joint->m_preconditioner1 = dFloat32(1.0f);
			if ((invMass0.GetScalar() > dFloat32(0.0f)) && (invMass1.GetScalar() > dFloat32(0.0f)) && !(body0->GetSkeleton() && body1->GetSkeleton()))
			{
				const dFloat32 mass0 = body0->GetMassMatrix().m_w;
				const dFloat32 mass1 = body1->GetMassMatrix().m_w;
				if (mass0 > (D_DIAGONAL_PRECONDITIONER * mass1))
				{
					joint->m_preconditioner0 = mass0 / (mass1 * D_DIAGONAL_PRECONDITIONER);
				}
				else if (mass1 > (D_DIAGONAL_PRECONDITIONER * mass0))
				{
					joint->m_preconditioner1 = mass1 / (mass0 * D_DIAGONAL_PRECONDITIONER);
				}
			}

			dVector forceAcc0(m_zero);
			dVector torqueAcc0(m_zero);
			dVector forceAcc1(m_zero);
			dVector torqueAcc1(m_zero);

			const dVector weigh0(body0->m_weigh * joint->m_preconditioner0);
			const dVector weigh1(body1->m_weigh * joint->m_preconditioner0);
			const dFloat32 preconditioner0 = joint->m_preconditioner0;
			const dFloat32 preconditioner1 = joint->m_preconditioner1;

			for (dInt32 i = 0; i < count; i++)
			{
				ndLeftHandSide* const row = &m_leftHandSide[index + i];
				ndRightHandSide* const rhs = &m_rightHandSide[index + i];

				row->m_JMinv.m_jacobianM0.m_linear = row->m_Jt.m_jacobianM0.m_linear * invMass0;
				row->m_JMinv.m_jacobianM0.m_angular = invInertia0.RotateVector(row->m_Jt.m_jacobianM0.m_angular);
				row->m_JMinv.m_jacobianM1.m_linear = row->m_Jt.m_jacobianM1.m_linear * invMass1;
				row->m_JMinv.m_jacobianM1.m_angular = invInertia1.RotateVector(row->m_Jt.m_jacobianM1.m_angular);

				const ndJacobian& JMinvM0 = row->m_JMinv.m_jacobianM0;
				const ndJacobian& JMinvM1 = row->m_JMinv.m_jacobianM1;
				const dVector tmpAccel(
					JMinvM0.m_linear * force0 + JMinvM0.m_angular * torque0 +
					JMinvM1.m_linear * force1 + JMinvM1.m_angular * torque1);

				dFloat32 extenalAcceleration = -tmpAccel.AddHorizontal().GetScalar();
				rhs->m_deltaAccel = extenalAcceleration;
				rhs->m_coordenateAccel += extenalAcceleration;
				dAssert(rhs->m_jointFeebackForce);
				const dFloat32 force = rhs->m_jointFeebackForce->GetInitiailGuess();

				rhs->m_force = isBilateral ? dClamp(force, rhs->m_lowerBoundFrictionCoefficent, rhs->m_upperBoundFrictionCoefficent) : force;
				rhs->m_maxImpact = dFloat32(0.0f);

				const ndJacobian& JtM0 = row->m_Jt.m_jacobianM0;
				const ndJacobian& JtM1 = row->m_Jt.m_jacobianM1;
				const dVector tmpDiag(
					weigh0 * (JMinvM0.m_linear * JtM0.m_linear + JMinvM0.m_angular * JtM0.m_angular) +
					weigh1 * (JMinvM1.m_linear * JtM1.m_linear + JMinvM1.m_angular * JtM1.m_angular));

				dFloat32 diag = tmpDiag.AddHorizontal().GetScalar();
				dAssert(diag > dFloat32(0.0f));
				rhs->m_diagDamp = diag * rhs->m_diagonalRegularizer;

				diag *= (dFloat32(1.0f) + rhs->m_diagonalRegularizer);
				rhs->m_invJinvMJt = dFloat32(1.0f) / diag;

				dVector f0(rhs->m_force * preconditioner0);
				dVector f1(rhs->m_force * preconditioner1);
				forceAcc0 = forceAcc0 + JtM0.m_linear * f0;
				torqueAcc0 = torqueAcc0 + JtM0.m_angular * f0;
				forceAcc1 = forceAcc1 + JtM1.m_linear * f1;
				torqueAcc1 = torqueAcc1 + JtM1.m_angular * f1;
			}

			if (body1->GetInvMass() > dFloat32(0.0f))
			{
				ndJacobian& outBody1 = m_internalForces[m1];
				dScopeSpinLock lock(body1->m_lock);
				outBody1.m_linear += forceAcc1;
				outBody1.m_angular += torqueAcc1;
			}

			ndJacobian& outBody0 = m_internalForces[m0];
			dScopeSpinLock lock(body0->m_lock);
			outBody0.m_linear += forceAcc0;
			outBody0.m_angular += torqueAcc0;
		}

		virtual void Execute()
		{
			D_TRACKTIME();
			ndWorld* const world = m_owner->GetWorld();
			ndDynamicsUpdateSoa* const me = (ndDynamicsUpdateSoa*)world->m_solver;
			m_leftHandSide = &me->GetLeftHandSide()[0];
			m_rightHandSide = &me->GetRightHandSide()[0];
			m_internalForces = &me->GetInternalForces()[0];
			
			ndConstraint** const jointArray = &m_owner->GetActiveContactArray()[0];
			const dInt32 jointCount = m_owner->GetActiveContactArray().GetCount();
			const dInt32 bodyCount = m_owner->GetActiveBodyArray().GetCount();
			const dInt32 threadIndex = GetThreadId();
			const dInt32 threadCount = m_owner->GetThreadCount();

			for (dInt32 i = threadIndex; i < jointCount; i += threadCount)
			{
				ndConstraint* const joint = jointArray[i];
				me->GetJacobianDerivatives(joint);
				BuildJacobianMatrix(joint);
			}
		}

		dVector m_zero;
		ndJacobian* m_internalForces;
		ndLeftHandSide* m_leftHandSide;
		ndRightHandSide* m_rightHandSide;
	};

	class ndTransposeMassMatrix : public ndScene::ndBaseJob
	{
		public:
		virtual void Execute()
		{
			D_TRACKTIME();
			ndWorld* const world = m_owner->GetWorld();
			ndDynamicsUpdateSoa* const me = (ndDynamicsUpdateSoa*)world->m_solver;
			ndConstraint** const jointArray = &m_owner->GetActiveContactArray()[0];
			const dInt32 jointCount = m_owner->GetActiveContactArray().GetCount();

			const dInt32 threadIndex = GetThreadId();
			const dInt32 threadCount = m_owner->GetThreadCount();
			const ndLeftHandSide* const leftHandSide = &me->GetLeftHandSide()[0];
			const ndRightHandSide* const rightHandSide = &me->GetRightHandSide()[0];
			dArray<ndSoa::ndSoaMatrixElement>& massMatrix = me->m_soaMassMatrix;

			const dInt32 mask = -dInt32(D_SSE_WORK_GROUP);
			const dInt32 soaJointCount = ((jointCount + D_SSE_WORK_GROUP - 1) & mask) / D_SSE_WORK_GROUP;
			const dInt32* const soaJointRows = &me->m_soaJointRows[0];

			const dVector zero(dVector::m_zero);
			const dVector ordinals(me->m_ordinals);
			for (dInt32 i = threadIndex; i < soaJointCount; i += threadCount)
			{
				const dInt32 index = i * D_SSE_WORK_GROUP;
				for (dInt32 j = 1; j < D_SSE_WORK_GROUP; j++)
				{
					ndConstraint* const joint = jointArray[index + j];
					if (joint)
					{
						dInt32 slot = j;
						for (; (slot > 0) && (jointArray[index + slot - 1]->m_rowCount < joint->m_rowCount); slot--)
						{
							jointArray[index + slot] = jointArray[index + slot - 1];
						}
						jointArray[index + slot] = joint;
					}
				}

				const dInt32 soaRowBase = soaJointRows[i];
				const ndConstraint* const lastJoint = jointArray[index + D_SSE_WORK_GROUP - 1];
				if (lastJoint && (lastJoint->m_rowCount == jointArray[index]->m_rowCount))
				{
					const ndConstraint* const joint0 = jointArray[index + 0];
					const ndConstraint* const joint1 = jointArray[index + 1];
					const ndConstraint* const joint2 = jointArray[index + 2];
					const ndConstraint* const joint3 = jointArray[index + 3];
					
					const ndConstraint* const joint = jointArray[index];
					const dInt32 rowCount = joint->m_rowCount;
					
					for (dInt32 j = 0; j < rowCount; j++)
					{
						dVector tmp;
						const ndLeftHandSide* const row0 = &leftHandSide[joint0->m_rowStart + j];
						const ndLeftHandSide* const row1 = &leftHandSide[joint1->m_rowStart + j];
						const ndLeftHandSide* const row2 = &leftHandSide[joint2->m_rowStart + j];
						const ndLeftHandSide* const row3 = &leftHandSide[joint3->m_rowStart + j];

						ndSoa::ndSoaMatrixElement& row = massMatrix[soaRowBase + j];
						dVector::Transpose4x4(
							row.m_Jt.m_jacobianM0.m_linear.m_x, 
							row.m_Jt.m_jacobianM0.m_linear.m_y,
							row.m_Jt.m_jacobianM0.m_linear.m_z,
							tmp,
							row0->m_Jt.m_jacobianM0.m_linear,
							row1->m_Jt.m_jacobianM0.m_linear,
							row2->m_Jt.m_jacobianM0.m_linear,
							row3->m_Jt.m_jacobianM0.m_linear);
						dVector::Transpose4x4(
							row.m_Jt.m_jacobianM0.m_angular.m_x, 
							row.m_Jt.m_jacobianM0.m_angular.m_y, 
							row.m_Jt.m_jacobianM0.m_angular.m_z, 
							tmp,
							row0->m_Jt.m_jacobianM0.m_angular,
							row1->m_Jt.m_jacobianM0.m_angular,
							row2->m_Jt.m_jacobianM0.m_angular,
							row3->m_Jt.m_jacobianM0.m_angular);

						dVector::Transpose4x4(
							row.m_Jt.m_jacobianM1.m_linear.m_x, 
							row.m_Jt.m_jacobianM1.m_linear.m_y, 
							row.m_Jt.m_jacobianM1.m_linear.m_z, 
							tmp,
							row0->m_Jt.m_jacobianM1.m_linear,
							row1->m_Jt.m_jacobianM1.m_linear,
							row2->m_Jt.m_jacobianM1.m_linear,
							row3->m_Jt.m_jacobianM1.m_linear);
						dVector::Transpose4x4(
							row.m_Jt.m_jacobianM1.m_angular.m_x, 
							row.m_Jt.m_jacobianM1.m_angular.m_y, 
							row.m_Jt.m_jacobianM1.m_angular.m_z, 
							tmp,
							row0->m_Jt.m_jacobianM1.m_angular,
							row1->m_Jt.m_jacobianM1.m_angular,
							row2->m_Jt.m_jacobianM1.m_angular,
							row3->m_Jt.m_jacobianM1.m_angular);
						
						dVector::Transpose4x4(
							row.m_JMinv.m_jacobianM0.m_linear.m_x, 
							row.m_JMinv.m_jacobianM0.m_linear.m_y, 
							row.m_JMinv.m_jacobianM0.m_linear.m_z, 
							tmp,
							row0->m_JMinv.m_jacobianM0.m_linear,
							row1->m_JMinv.m_jacobianM0.m_linear,
							row2->m_JMinv.m_jacobianM0.m_linear,
							row3->m_JMinv.m_jacobianM0.m_linear);
						dVector::Transpose4x4(
							row.m_JMinv.m_jacobianM0.m_angular.m_x, 
							row.m_JMinv.m_jacobianM0.m_angular.m_y, 
							row.m_JMinv.m_jacobianM0.m_angular.m_z, 
							tmp,
							row0->m_JMinv.m_jacobianM0.m_angular,
							row1->m_JMinv.m_jacobianM0.m_angular,
							row2->m_JMinv.m_jacobianM0.m_angular,
							row3->m_JMinv.m_jacobianM0.m_angular);

						dVector::Transpose4x4(
							row.m_JMinv.m_jacobianM1.m_linear.m_x,
							row.m_JMinv.m_jacobianM1.m_linear.m_y, 
							row.m_JMinv.m_jacobianM1.m_linear.m_z, 
							tmp,
							row0->m_JMinv.m_jacobianM1.m_linear,
							row1->m_JMinv.m_jacobianM1.m_linear,
							row2->m_JMinv.m_jacobianM1.m_linear,
							row3->m_JMinv.m_jacobianM1.m_linear);
						dVector::Transpose4x4(
							row.m_JMinv.m_jacobianM1.m_angular.m_x,
							row.m_JMinv.m_jacobianM1.m_angular.m_y,
							row.m_JMinv.m_jacobianM1.m_angular.m_z,
							tmp,
							row0->m_JMinv.m_jacobianM1.m_angular,
							row1->m_JMinv.m_jacobianM1.m_angular,
							row2->m_JMinv.m_jacobianM1.m_angular,
							row3->m_JMinv.m_jacobianM1.m_angular);
						
						#ifdef D_NEWTON_USE_DOUBLE
							dInt64* const normalIndex = (dInt64*)&row.m_normalForceIndex[0];
						#else
							dInt32* const normalIndex = (dInt32*)&row.m_normalForceIndex[0];
						#endif
						for (dInt32 k = 0; k < D_SSE_WORK_GROUP; k++)
						{
							const ndConstraint* const soaJoint = jointArray[index + k];
							const ndRightHandSide* const rhs = &rightHandSide[soaJoint->m_rowStart + j];
							row.m_force[k] = rhs->m_force;
							row.m_diagDamp[k] = rhs->m_diagDamp;
							row.m_invJinvMJt[k] = rhs->m_invJinvMJt;
							row.m_coordenateAccel[k] = rhs->m_coordenateAccel;
							normalIndex[k] = (rhs->m_normalForceIndex + 1) * D_SSE_WORK_GROUP + k;
							row.m_lowerBoundFrictionCoefficent[k] = rhs->m_lowerBoundFrictionCoefficent;
							row.m_upperBoundFrictionCoefficent[k] = rhs->m_upperBoundFrictionCoefficent;
						}
					}
				}
				else
				{
					const ndConstraint* const firstJoint = jointArray[index];
					for (dInt32 j = 0; j < firstJoint->m_rowCount; j++)
					{
						ndSoa::ndSoaMatrixElement& row = massMatrix[soaRowBase + j];
						row.m_Jt.m_jacobianM0.m_linear.m_x = zero;
						row.m_Jt.m_jacobianM0.m_linear.m_y = zero;
						row.m_Jt.m_jacobianM0.m_linear.m_z = zero;
						row.m_Jt.m_jacobianM0.m_angular.m_x = zero;
						row.m_Jt.m_jacobianM0.m_angular.m_y = zero;
						row.m_Jt.m_jacobianM0.m_angular.m_z = zero;
						row.m_Jt.m_jacobianM1.m_linear.m_x = zero;
						row.m_Jt.m_jacobianM1.m_linear.m_y = zero;
						row.m_Jt.m_jacobianM1.m_linear.m_z = zero;
						row.m_Jt.m_jacobianM1.m_angular.m_x = zero;
						row.m_Jt.m_jacobianM1.m_angular.m_y = zero;
						row.m_Jt.m_jacobianM1.m_angular.m_z = zero;
					
						row.m_JMinv.m_jacobianM0.m_linear.m_x = zero;
						row.m_JMinv.m_jacobianM0.m_linear.m_y = zero;
						row.m_JMinv.m_jacobianM0.m_linear.m_z = zero;
						row.m_JMinv.m_jacobianM0.m_angular.m_x = zero;
						row.m_JMinv.m_jacobianM0.m_angular.m_y = zero;
						row.m_JMinv.m_jacobianM0.m_angular.m_z = zero;
						row.m_JMinv.m_jacobianM1.m_linear.m_x = zero;
						row.m_JMinv.m_jacobianM1.m_linear.m_y = zero;
						row.m_JMinv.m_jacobianM1.m_linear.m_z = zero;
						row.m_JMinv.m_jacobianM1.m_angular.m_x = zero;
						row.m_JMinv.m_jacobianM1.m_angular.m_y = zero;
						row.m_JMinv.m_jacobianM1.m_angular.m_z = zero;
					
						row.m_force = zero;
						row.m_diagDamp = zero;
						row.m_invJinvMJt = zero;
						row.m_coordenateAccel = zero;
						row.m_normalForceIndex = ordinals;
						row.m_lowerBoundFrictionCoefficent = zero;
						row.m_upperBoundFrictionCoefficent = zero;
					}
					
					for (dInt32 j = 0; j < D_SSE_WORK_GROUP; j++)
					{
						const ndConstraint* const joint = jointArray[index + j];
						if (joint)
						{
							for (dInt32 k = 0; k < joint->m_rowCount; k++)
							{
								ndSoa::ndSoaMatrixElement& row = massMatrix[soaRowBase + k];
								const ndLeftHandSide* const lhs = &leftHandSide[joint->m_rowStart + k];
					
								row.m_Jt.m_jacobianM0.m_linear.m_x[j] = lhs->m_Jt.m_jacobianM0.m_linear.m_x;
								row.m_Jt.m_jacobianM0.m_linear.m_y[j] = lhs->m_Jt.m_jacobianM0.m_linear.m_y;
								row.m_Jt.m_jacobianM0.m_linear.m_z[j] = lhs->m_Jt.m_jacobianM0.m_linear.m_z;
								row.m_Jt.m_jacobianM0.m_angular.m_x[j] = lhs->m_Jt.m_jacobianM0.m_angular.m_x;
								row.m_Jt.m_jacobianM0.m_angular.m_y[j] = lhs->m_Jt.m_jacobianM0.m_angular.m_y;
								row.m_Jt.m_jacobianM0.m_angular.m_z[j] = lhs->m_Jt.m_jacobianM0.m_angular.m_z;
								row.m_Jt.m_jacobianM1.m_linear.m_x[j] = lhs->m_Jt.m_jacobianM1.m_linear.m_x;
								row.m_Jt.m_jacobianM1.m_linear.m_y[j] = lhs->m_Jt.m_jacobianM1.m_linear.m_y;
								row.m_Jt.m_jacobianM1.m_linear.m_z[j] = lhs->m_Jt.m_jacobianM1.m_linear.m_z;
								row.m_Jt.m_jacobianM1.m_angular.m_x[j] = lhs->m_Jt.m_jacobianM1.m_angular.m_x;
								row.m_Jt.m_jacobianM1.m_angular.m_y[j] = lhs->m_Jt.m_jacobianM1.m_angular.m_y;
								row.m_Jt.m_jacobianM1.m_angular.m_z[j] = lhs->m_Jt.m_jacobianM1.m_angular.m_z;
					
								row.m_JMinv.m_jacobianM0.m_linear.m_x[j] = lhs->m_JMinv.m_jacobianM0.m_linear.m_x;
								row.m_JMinv.m_jacobianM0.m_linear.m_y[j] = lhs->m_JMinv.m_jacobianM0.m_linear.m_y;
								row.m_JMinv.m_jacobianM0.m_linear.m_z[j] = lhs->m_JMinv.m_jacobianM0.m_linear.m_z;
								row.m_JMinv.m_jacobianM0.m_angular.m_x[j] = lhs->m_JMinv.m_jacobianM0.m_angular.m_x;
								row.m_JMinv.m_jacobianM0.m_angular.m_y[j] = lhs->m_JMinv.m_jacobianM0.m_angular.m_y;
								row.m_JMinv.m_jacobianM0.m_angular.m_z[j] = lhs->m_JMinv.m_jacobianM0.m_angular.m_z;
								row.m_JMinv.m_jacobianM1.m_linear.m_x[j] = lhs->m_JMinv.m_jacobianM1.m_linear.m_x;
								row.m_JMinv.m_jacobianM1.m_linear.m_y[j] = lhs->m_JMinv.m_jacobianM1.m_linear.m_y;
								row.m_JMinv.m_jacobianM1.m_linear.m_z[j] = lhs->m_JMinv.m_jacobianM1.m_linear.m_z;
								row.m_JMinv.m_jacobianM1.m_angular.m_x[j] = lhs->m_JMinv.m_jacobianM1.m_angular.m_x;
								row.m_JMinv.m_jacobianM1.m_angular.m_y[j] = lhs->m_JMinv.m_jacobianM1.m_angular.m_y;
								row.m_JMinv.m_jacobianM1.m_angular.m_z[j] = lhs->m_JMinv.m_jacobianM1.m_angular.m_z;
					
								const ndRightHandSide* const rhs = &rightHandSide[joint->m_rowStart + k];
								row.m_force[j] = rhs->m_force;
								row.m_diagDamp[j] = rhs->m_diagDamp;
								row.m_invJinvMJt[j] = rhs->m_invJinvMJt;
								row.m_coordenateAccel[j] = rhs->m_coordenateAccel;
			
								#ifdef D_NEWTON_USE_DOUBLE
									dInt64* const normalIndex = (dInt64*)&row.m_normalForceIndex[0];
								#else
									dInt32* const normalIndex = (dInt32*)&row.m_normalForceIndex[0];
								#endif
								normalIndex[j] = (rhs->m_normalForceIndex + 1) * D_SSE_WORK_GROUP + j;
								row.m_lowerBoundFrictionCoefficent[j] = rhs->m_lowerBoundFrictionCoefficent;
								row.m_upperBoundFrictionCoefficent[j] = rhs->m_upperBoundFrictionCoefficent;
							}
						}
					}
				}
			}
		}
	};

	ndScene* const scene = m_world->GetScene();
	if (scene->GetActiveContactArray().GetCount())
	{
		D_TRACKTIME();
		m_rightHandSide[0].m_force = dFloat32(1.0f);
		ClearJacobianBuffer(GetInternalForces().GetCount(), &GetInternalForces()[0]);
		scene->SubmitJobs<ndInitJacobianMatrix>();
		scene->SubmitJobs<ndTransposeMassMatrix>();
	}
}

void ndDynamicsUpdateSoa::UpdateForceFeedback()
{
	D_TRACKTIME();
	class ndUpdateForceFeedback : public ndScene::ndBaseJob
	{
		public:
		virtual void Execute()
		{
			D_TRACKTIME();
			ndWorld* const world = m_owner->GetWorld();
			ndDynamicsUpdateSoa* const me = (ndDynamicsUpdateSoa*)world->m_solver;
			const ndConstraintArray& jointArray = m_owner->GetActiveContactArray();
			dArray<ndRightHandSide>& rightHandSide = me->m_rightHandSide;

			const dInt32 threadIndex = GetThreadId();
			const dInt32 threadCount = m_owner->GetThreadCount();
			const dInt32 jointCount = jointArray.GetCount();

			const dFloat32 timestepRK = me->m_timestepRK;
			for (dInt32 i = threadIndex; i < jointCount; i += threadCount)
			{
				ndConstraint* const joint = jointArray[i];
				const dInt32 rows = joint->m_rowCount;
				const dInt32 first = joint->m_rowStart;

				for (dInt32 j = 0; j < rows; j++)
				{
					const ndRightHandSide* const rhs = &rightHandSide[j + first];
					dAssert(dCheckFloat(rhs->m_force));
					rhs->m_jointFeebackForce->Push(rhs->m_force);
					rhs->m_jointFeebackForce->m_force = rhs->m_force;
					rhs->m_jointFeebackForce->m_impact = rhs->m_maxImpact * timestepRK;
				}

				if (joint->GetAsBilateral())
				{
					const dArray<ndLeftHandSide>& leftHandSide = me->m_leftHandSide;
					dVector force0(dVector::m_zero);
					dVector force1(dVector::m_zero);
					dVector torque0(dVector::m_zero);
					dVector torque1(dVector::m_zero);
					for (dInt32 j = 0; j < rows; j++)
					{
						const ndRightHandSide* const rhs = &rightHandSide[j + first];
						const ndLeftHandSide* const lhs = &leftHandSide[j + first];
						const dVector f(rhs->m_force);
						force0 += lhs->m_Jt.m_jacobianM0.m_linear * f;
						torque0 += lhs->m_Jt.m_jacobianM0.m_angular * f;
						force1 += lhs->m_Jt.m_jacobianM1.m_linear * f;
						torque1 += lhs->m_Jt.m_jacobianM1.m_angular * f;
					}
					ndJointBilateralConstraint* const bilateral = joint->GetAsBilateral();
					bilateral->m_forceBody0 = force0;
					bilateral->m_torqueBody0 = torque0;
					bilateral->m_forceBody1 = force1;
					bilateral->m_torqueBody1 = torque1;
				}
			}
		}
	};

	ndScene* const scene = m_world->GetScene();
	scene->SubmitJobs<ndUpdateForceFeedback>();
}

void ndDynamicsUpdateSoa::InitSkeletons()
{
	D_TRACKTIME();

	class ndInitSkeletons : public ndScene::ndBaseJob
	{
		public:
		virtual void Execute()
		{
			D_TRACKTIME();
			const dInt32 threadIndex = GetThreadId();
			ndWorld* const world = m_owner->GetWorld();
			ndDynamicsUpdateSoa* const me = (ndDynamicsUpdateSoa*)world->m_solver;
			ndSkeletonList::dNode* node = world->GetSkeletonList().GetFirst();
			for (dInt32 i = 0; i < threadIndex; i++)
			{
				node = node ? node->GetNext() : nullptr;
			}

			dArray<ndRightHandSide>& rightHandSide = me->m_rightHandSide;
			const dArray<ndLeftHandSide>& leftHandSide = me->m_leftHandSide;

			const dInt32 threadCount = m_owner->GetThreadCount();
			while (node)
			{
				ndSkeletonContainer* const skeleton = &node->GetInfo();
				skeleton->InitMassMatrix(&leftHandSide[0], &rightHandSide[0]);

				for (dInt32 i = 0; i < threadCount; i++)
				{
					node = node ? node->GetNext() : nullptr;
				}
			}
		}
	};

	ndScene* const scene = m_world->GetScene();
	scene->SubmitJobs<ndInitSkeletons>();
}

void ndDynamicsUpdateSoa::UpdateSkeletons()
{
	D_TRACKTIME();
	class ndUpdateSkeletons : public ndScene::ndBaseJob
	{
		public:
		virtual void Execute()
		{
			D_TRACKTIME();
			const dInt32 threadIndex = GetThreadId();
			ndWorld* const world = m_owner->GetWorld();
			ndDynamicsUpdateSoa* const me = (ndDynamicsUpdateSoa*)world->m_solver;
			ndSkeletonList::dNode* node = world->GetSkeletonList().GetFirst();
			for (dInt32 i = 0; i < threadIndex; i++)
			{
				node = node ? node->GetNext() : nullptr;
			}

			ndJacobian* const internalForces = &me->GetInternalForces()[0];
			const dArray<ndBodyKinematic*>& activeBodies = m_owner->ndScene::GetActiveBodyArray();
			const ndBodyKinematic** const bodyArray = (const ndBodyKinematic**)&activeBodies[0];

			const dInt32 threadCount = m_owner->GetThreadCount();
			while (node)
			{
				ndSkeletonContainer* const skeleton = &node->GetInfo();
				skeleton->CalculateJointForce(bodyArray, internalForces);

				for (dInt32 i = 0; i < threadCount; i++)
				{
					node = node ? node->GetNext() : nullptr;
				}
			}
		}
	};

	ndScene* const scene = m_world->GetScene();
	scene->SubmitJobs<ndUpdateSkeletons>();
}

void ndDynamicsUpdateSoa::CalculateJointsAcceleration()
{
	D_TRACKTIME();
	class ndCalculateJointsAcceleration : public ndScene::ndBaseJob
	{
		public:
		virtual void Execute()
		{
			D_TRACKTIME();
			ndWorld* const world = m_owner->GetWorld();
			ndDynamicsUpdateSoa* const me = (ndDynamicsUpdateSoa*)world->m_solver;
			const ndConstraintArray& jointArray = m_owner->GetActiveContactArray();

			ndJointAccelerationDecriptor joindDesc;
			joindDesc.m_timestep = me->m_timestepRK;
			joindDesc.m_invTimestep = me->m_invTimestepRK;
			joindDesc.m_firstPassCoefFlag = me->m_firstPassCoef;
			dArray<ndLeftHandSide>& leftHandSide = me->m_leftHandSide;
			dArray<ndRightHandSide>& rightHandSide = me->m_rightHandSide;

			const dInt32 threadIndex = GetThreadId();
			const dInt32 threadCount = m_owner->GetThreadCount();
			const dInt32 jointCount = jointArray.GetCount();

			for (dInt32 i = threadIndex; i < jointCount; i += threadCount)
			{
				ndConstraint* const joint = jointArray[i];
				const dInt32 pairStart = joint->m_rowStart;
				joindDesc.m_rowsCount = joint->m_rowCount;
				joindDesc.m_leftHandSide = &leftHandSide[pairStart];
				joindDesc.m_rightHandSide = &rightHandSide[pairStart];
				joint->JointAccelerations(&joindDesc);
			}
		}
	};

	class ndUpdateAcceleration : public ndScene::ndBaseJob
	{
		public:
		virtual void Execute()
		{
			D_TRACKTIME();
			ndWorld* const world = m_owner->GetWorld();
			ndDynamicsUpdateSoa* const me = (ndDynamicsUpdateSoa*)world->m_solver;
			const ndConstraintArray& jointArray = m_owner->GetActiveContactArray();
			const dArray<ndRightHandSide>& rightHandSide = me->m_rightHandSide;

			const dInt32 threadIndex = GetThreadId();
			const dInt32 threadCount = m_owner->GetThreadCount();
			const dInt32 jointCount = jointArray.GetCount();
			const dInt32 mask = -dInt32(D_SSE_WORK_GROUP);
			const dInt32* const soaJointRows = &me->m_soaJointRows[0];
			const dInt32 soaJointCountBatches = ((jointCount + D_SSE_WORK_GROUP - 1) & mask) / D_SSE_WORK_GROUP;

			const ndConstraint* const * jointArrayPtr = &jointArray[0];
			ndSoaMatrixElement* const massMatrix = &me->m_soaMassMatrix[0];
			for (dInt32 i = threadIndex; i < soaJointCountBatches; i += threadCount)
			{
				const ndConstraint* const * jointGroup = &jointArrayPtr[i * D_SSE_WORK_GROUP];
				const ndConstraint* const firstJoint = jointGroup[0];
				const ndConstraint* const lastJoint = jointGroup[D_SSE_WORK_GROUP - 1];
				const dInt32 soaRowStartBase = soaJointRows[i];
				if (lastJoint && (firstJoint->m_rowCount == lastJoint->m_rowCount))
				{
					const dInt32 rowCount = firstJoint->m_rowCount;
					for (dInt32 j = 0; j < D_SSE_WORK_GROUP; j++)
					{
						const ndConstraint* const Joint = jointGroup[j];
						const dInt32 base = Joint->m_rowStart;
						for (dInt32 k = 0; k < rowCount; k++)
						{
							ndSoaMatrixElement* const row = &massMatrix[soaRowStartBase + k];
							row->m_coordenateAccel[j] = rightHandSide[base + k].m_coordenateAccel;
						}
					}
				}
				else
				{
					for (dInt32 j = 0; j < D_SSE_WORK_GROUP; j++)
					{
						const ndConstraint* const Joint = jointGroup[j];
						if (Joint)
						{
							const dInt32 rowCount = Joint->m_rowCount;
							const dInt32 base = Joint->m_rowStart;
							for (dInt32 k = 0; k < rowCount; k++)
							{
								ndSoaMatrixElement* const row = &massMatrix[soaRowStartBase + k];
								row->m_coordenateAccel[j] = rightHandSide[base + k].m_coordenateAccel;
							}
						}
					}
				}
			}
		}
	};

	ndScene* const scene = m_world->GetScene();
	scene->SubmitJobs<ndCalculateJointsAcceleration>();
	m_firstPassCoef = dFloat32(1.0f);

	scene->SubmitJobs<ndUpdateAcceleration>();
}

void ndDynamicsUpdateSoa::IntegrateBodiesVelocity()
{
	D_TRACKTIME();
	class ndIntegrateBodiesVelocity : public ndScene::ndBaseJob
	{
		public:
		virtual void Execute()
		{
			D_TRACKTIME();
			ndWorld* const world = m_owner->GetWorld();
			ndDynamicsUpdateSoa* const me = (ndDynamicsUpdateSoa*)world->m_solver;
			dArray<ndBodyKinematic*>& bodyArray = me->m_bodyIslandOrder;
			const dArray<ndJacobian>& internalForces = me->GetInternalForces();

			const dVector timestep4(me->m_timestepRK);
			const dVector speedFreeze2(world->m_freezeSpeed2 * dFloat32(0.1f));

			const dInt32 threadIndex = GetThreadId();
			const dInt32 threadCount = m_owner->GetThreadCount();
			const dInt32 bodyCount = bodyArray.GetCount() - me->m_unConstrainedBodyCount;

			for (dInt32 i = threadIndex; i < bodyCount; i += threadCount)
			{
				ndBodyKinematic* const body = bodyArray[i];
				ndBodyDynamic* const dynBody = body->GetAsBodyDynamic();
				if (dynBody)
				{
					dAssert(dynBody->m_bodyIsConstrained);
					const dInt32 index = dynBody->m_index;
					const ndJacobian& forceAndTorque = internalForces[index];
					const dVector force(dynBody->GetForce() + forceAndTorque.m_linear);
					const dVector torque(dynBody->GetTorque() + forceAndTorque.m_angular - body->GetGyroTorque());
					ndJacobian velocStep(dynBody->IntegrateForceAndToque(force, torque, timestep4));

					if (!body->m_resting)
					{
						body->m_veloc += velocStep.m_linear;
						body->m_omega += velocStep.m_angular;
						dynBody->IntegrateGyroSubstep(timestep4);
					}
					else
					{
						const dVector velocStep2(velocStep.m_linear.DotProduct(velocStep.m_linear));
						const dVector omegaStep2(velocStep.m_angular.DotProduct(velocStep.m_angular));
						const dVector test(((velocStep2 > speedFreeze2) | (omegaStep2 > speedFreeze2)) & dVector::m_negOne);
						const dInt32 equilibrium = test.GetSignMask() ? 0 : 1;
						body->m_resting &= equilibrium;
					}

					dAssert(body->m_veloc.m_w == dFloat32(0.0f));
					dAssert(body->m_omega.m_w == dFloat32(0.0f));
				}
			}
		}
	};

	ndScene* const scene = m_world->GetScene();
	scene->SubmitJobs<ndIntegrateBodiesVelocity>();
}

void ndDynamicsUpdateSoa::CalculateJointsForce()
{
	D_TRACKTIME();
	class ndCalculateJointsForce : public ndScene::ndBaseJob
	{
		public:
		ndCalculateJointsForce()
			:m_one(dFloat32(1.0f))
			,m_zero(dFloat32 (0.0f))
		{
		}

		dFloat32 JointForce(dInt32 block, ndSoaMatrixElement* const massMatrix)
		{
			dVector weight0;
			dVector weight1;
			ndSoaVector6 forceM0;
			ndSoaVector6 forceM1;
			dVector preconditioner0;
			dVector preconditioner1;
			dVector normalForce[D_CONSTRAINT_MAX_ROWS + 1];

			ndConstraint** const jointGroup = &m_jointArray[block];

			if (jointGroup[D_SSE_WORK_GROUP - 1])
			{
				for (dInt32 i = 0; i < D_SSE_WORK_GROUP; i++)
				{
					const ndConstraint* const joint = jointGroup[i];
					const ndBodyKinematic* const body0 = joint->GetBody0();
					const ndBodyKinematic* const body1 = joint->GetBody1();

					const dInt32 m0 = body0->m_index;
					const dInt32 m1 = body1->m_index;

					weight0[i] = body0->m_weigh;
					weight1[i] = body1->m_weigh;
					preconditioner0[i] = joint->m_preconditioner0;
					preconditioner1[i] = joint->m_preconditioner1;

					forceM0.m_linear.m_x[i] = m_internalForces[m0].m_linear.m_x;
					forceM0.m_linear.m_y[i] = m_internalForces[m0].m_linear.m_y;
					forceM0.m_linear.m_z[i] = m_internalForces[m0].m_linear.m_z;
					forceM0.m_angular.m_x[i] = m_internalForces[m0].m_angular.m_x;
					forceM0.m_angular.m_y[i] = m_internalForces[m0].m_angular.m_y;
					forceM0.m_angular.m_z[i] = m_internalForces[m0].m_angular.m_z;

					forceM1.m_linear.m_x[i] = m_internalForces[m1].m_linear.m_x;
					forceM1.m_linear.m_y[i] = m_internalForces[m1].m_linear.m_y;
					forceM1.m_linear.m_z[i] = m_internalForces[m1].m_linear.m_z;
					forceM1.m_angular.m_x[i] = m_internalForces[m1].m_angular.m_x;
					forceM1.m_angular.m_y[i] = m_internalForces[m1].m_angular.m_y;
					forceM1.m_angular.m_z[i] = m_internalForces[m1].m_angular.m_z;
				}
			}
			else
			{
				weight0 = m_zero;
				weight1 = m_zero;
				preconditioner0 = m_zero;
				preconditioner1 = m_zero;
				
				forceM0.m_linear.m_x = m_zero;
				forceM0.m_linear.m_y = m_zero;
				forceM0.m_linear.m_z = m_zero;
				forceM0.m_angular.m_x = m_zero;
				forceM0.m_angular.m_y = m_zero;
				forceM0.m_angular.m_z = m_zero;
				
				forceM1.m_linear.m_x = m_zero;
				forceM1.m_linear.m_y = m_zero;
				forceM1.m_linear.m_z = m_zero;
				forceM1.m_angular.m_x = m_zero;
				forceM1.m_angular.m_y = m_zero;
				forceM1.m_angular.m_z = m_zero;
				for (dInt32 i = 0; i < D_SSE_WORK_GROUP; i++)
				{
					const ndConstraint* const joint = jointGroup[i];
					if (joint)
					{
						const ndBodyKinematic* const body0 = joint->GetBody0();
						const ndBodyKinematic* const body1 = joint->GetBody1();
				
						const dInt32 m0 = body0->m_index;
						const dInt32 m1 = body1->m_index;

						preconditioner0[i] = joint->m_preconditioner0;
						preconditioner1[i] = joint->m_preconditioner1;
				
						forceM0.m_linear.m_x[i] = m_internalForces[m0].m_linear.m_x;
						forceM0.m_linear.m_y[i] = m_internalForces[m0].m_linear.m_y;
						forceM0.m_linear.m_z[i] = m_internalForces[m0].m_linear.m_z;
						forceM0.m_angular.m_x[i] = m_internalForces[m0].m_angular.m_x;
						forceM0.m_angular.m_y[i] = m_internalForces[m0].m_angular.m_y;
						forceM0.m_angular.m_z[i] = m_internalForces[m0].m_angular.m_z;
				
						forceM1.m_linear.m_x[i] = m_internalForces[m1].m_linear.m_x;
						forceM1.m_linear.m_y[i] = m_internalForces[m1].m_linear.m_y;
						forceM1.m_linear.m_z[i] = m_internalForces[m1].m_linear.m_z;
						forceM1.m_angular.m_x[i] = m_internalForces[m1].m_angular.m_x;
						forceM1.m_angular.m_y[i] = m_internalForces[m1].m_angular.m_y;
						forceM1.m_angular.m_z[i] = m_internalForces[m1].m_angular.m_z;
				
						weight0[i] = body0->m_weigh;
						weight1[i] = body1->m_weigh;
					}
				}
			}

			forceM0.m_linear.m_x = forceM0.m_linear.m_x * preconditioner0;
			forceM0.m_linear.m_y = forceM0.m_linear.m_y * preconditioner0;
			forceM0.m_linear.m_z = forceM0.m_linear.m_z * preconditioner0;
			forceM0.m_angular.m_x = forceM0.m_angular.m_x * preconditioner0;
			forceM0.m_angular.m_y = forceM0.m_angular.m_y * preconditioner0;
			forceM0.m_angular.m_z = forceM0.m_angular.m_z * preconditioner0;

			forceM1.m_linear.m_x = forceM1.m_linear.m_x * preconditioner1;
			forceM1.m_linear.m_y = forceM1.m_linear.m_y * preconditioner1;
			forceM1.m_linear.m_z = forceM1.m_linear.m_z * preconditioner1;
			forceM1.m_angular.m_x = forceM1.m_angular.m_x * preconditioner1;
			forceM1.m_angular.m_y = forceM1.m_angular.m_y * preconditioner1;
			forceM1.m_angular.m_z = forceM1.m_angular.m_z * preconditioner1;

			preconditioner0 = preconditioner0 * weight0;
			preconditioner1 = preconditioner1 * weight1;

			normalForce[0] = m_one;
			dVector accNorm(m_zero);
			const dInt32 rowsCount = jointGroup[0]->m_rowCount;
		
			for (dInt32 j = 0; j < rowsCount; j++)
			{
				const ndSoaMatrixElement* const row = &massMatrix[j];

				dVector a(row->m_JMinv.m_jacobianM0.m_linear.m_x * forceM0.m_linear.m_x);
				a = a.MulAdd(row->m_JMinv.m_jacobianM0.m_linear.m_y, forceM0.m_linear.m_y);
				a = a.MulAdd(row->m_JMinv.m_jacobianM0.m_linear.m_z, forceM0.m_linear.m_z);

				a = a.MulAdd(row->m_JMinv.m_jacobianM0.m_angular.m_x, forceM0.m_angular.m_x);
				a = a.MulAdd(row->m_JMinv.m_jacobianM0.m_angular.m_y, forceM0.m_angular.m_y);
				a = a.MulAdd(row->m_JMinv.m_jacobianM0.m_angular.m_z, forceM0.m_angular.m_z);

				a = a.MulAdd(row->m_JMinv.m_jacobianM1.m_linear.m_x, forceM1.m_linear.m_x);
				a = a.MulAdd(row->m_JMinv.m_jacobianM1.m_linear.m_y, forceM1.m_linear.m_y);
				a = a.MulAdd(row->m_JMinv.m_jacobianM1.m_linear.m_z, forceM1.m_linear.m_z);

				a = a.MulAdd(row->m_JMinv.m_jacobianM1.m_angular.m_x, forceM1.m_angular.m_x);
				a = a.MulAdd(row->m_JMinv.m_jacobianM1.m_angular.m_y, forceM1.m_angular.m_y);
				a = a.MulAdd(row->m_JMinv.m_jacobianM1.m_angular.m_z, forceM1.m_angular.m_z);

				a = row->m_coordenateAccel.MulSub(row->m_force, row->m_diagDamp) - a;
				dVector f(row->m_force.MulAdd(row->m_invJinvMJt, a));

				const dVector frictionNormal(&normalForce[0].m_x, row->m_normalForceIndex.m_i);
				const dVector lowerFrictionForce(frictionNormal * row->m_lowerBoundFrictionCoefficent);
				const dVector upperFrictionForce(frictionNormal * row->m_upperBoundFrictionCoefficent);

				a = a & (f < upperFrictionForce) & (f > lowerFrictionForce);
				f = f.GetMax(lowerFrictionForce).GetMin(upperFrictionForce);

				accNorm = accNorm.MulAdd(a, a);
				normalForce[j + 1] = f;

				const dVector deltaForce(f - row->m_force);
				const dVector deltaForce0(deltaForce * preconditioner0);
				const dVector deltaForce1(deltaForce * preconditioner1);

				forceM0.m_linear.m_x = forceM0.m_linear.m_x.MulAdd(row->m_Jt.m_jacobianM0.m_linear.m_x, deltaForce0);
				forceM0.m_linear.m_y = forceM0.m_linear.m_y.MulAdd(row->m_Jt.m_jacobianM0.m_linear.m_y, deltaForce0);
				forceM0.m_linear.m_z = forceM0.m_linear.m_z.MulAdd(row->m_Jt.m_jacobianM0.m_linear.m_z, deltaForce0);
				forceM0.m_angular.m_x = forceM0.m_angular.m_x.MulAdd(row->m_Jt.m_jacobianM0.m_angular.m_x, deltaForce0);
				forceM0.m_angular.m_y = forceM0.m_angular.m_y.MulAdd(row->m_Jt.m_jacobianM0.m_angular.m_y, deltaForce0);
				forceM0.m_angular.m_z = forceM0.m_angular.m_z.MulAdd(row->m_Jt.m_jacobianM0.m_angular.m_z, deltaForce0);

				forceM1.m_linear.m_x = forceM1.m_linear.m_x.MulAdd(row->m_Jt.m_jacobianM1.m_linear.m_x, deltaForce1);
				forceM1.m_linear.m_y = forceM1.m_linear.m_y.MulAdd(row->m_Jt.m_jacobianM1.m_linear.m_y, deltaForce1);
				forceM1.m_linear.m_z = forceM1.m_linear.m_z.MulAdd(row->m_Jt.m_jacobianM1.m_linear.m_z, deltaForce1);
				forceM1.m_angular.m_x = forceM1.m_angular.m_x.MulAdd(row->m_Jt.m_jacobianM1.m_angular.m_x, deltaForce1);
				forceM1.m_angular.m_y = forceM1.m_angular.m_y.MulAdd(row->m_Jt.m_jacobianM1.m_angular.m_y, deltaForce1);
				forceM1.m_angular.m_z = forceM1.m_angular.m_z.MulAdd(row->m_Jt.m_jacobianM1.m_angular.m_z, deltaForce1);
			}

			const dFloat32 tol = dFloat32(0.5f);
			const dFloat32 tol2 = tol * tol;

			dVector maxAccel(accNorm);
			for (dInt32 k = 0; (k < 4) && (maxAccel.AddHorizontal().GetScalar() > tol2); k++)
			{
				maxAccel = m_zero;
				for (dInt32 j = 0; j < rowsCount; j++)
				{
					const ndSoaMatrixElement* const row = &massMatrix[j];

					dVector a(row->m_JMinv.m_jacobianM0.m_linear.m_x * forceM0.m_linear.m_x);
					a = a.MulAdd(row->m_JMinv.m_jacobianM0.m_linear.m_y, forceM0.m_linear.m_y);
					a = a.MulAdd(row->m_JMinv.m_jacobianM0.m_linear.m_z, forceM0.m_linear.m_z);

					a = a.MulAdd(row->m_JMinv.m_jacobianM0.m_angular.m_x, forceM0.m_angular.m_x);
					a = a.MulAdd(row->m_JMinv.m_jacobianM0.m_angular.m_y, forceM0.m_angular.m_y);
					a = a.MulAdd(row->m_JMinv.m_jacobianM0.m_angular.m_z, forceM0.m_angular.m_z);

					a = a.MulAdd(row->m_JMinv.m_jacobianM1.m_linear.m_x, forceM1.m_linear.m_x);
					a = a.MulAdd(row->m_JMinv.m_jacobianM1.m_linear.m_y, forceM1.m_linear.m_y);
					a = a.MulAdd(row->m_JMinv.m_jacobianM1.m_linear.m_z, forceM1.m_linear.m_z);

					a = a.MulAdd(row->m_JMinv.m_jacobianM1.m_angular.m_x, forceM1.m_angular.m_x);
					a = a.MulAdd(row->m_JMinv.m_jacobianM1.m_angular.m_y, forceM1.m_angular.m_y);
					a = a.MulAdd(row->m_JMinv.m_jacobianM1.m_angular.m_z, forceM1.m_angular.m_z);

					const dVector force(normalForce[j + 1]);
					a = row->m_coordenateAccel.MulSub(force, row->m_diagDamp) - a;
					dVector f(force.MulAdd(row->m_invJinvMJt, a));

					const dVector frictionNormal(&normalForce[0].m_x, row->m_normalForceIndex.m_i);
					const dVector lowerFrictionForce(frictionNormal * row->m_lowerBoundFrictionCoefficent);
					const dVector upperFrictionForce(frictionNormal * row->m_upperBoundFrictionCoefficent);

					a = a & (f < upperFrictionForce) & (f > lowerFrictionForce);
					f = f.GetMax(lowerFrictionForce).GetMin(upperFrictionForce);

					maxAccel = maxAccel.MulAdd(a, a);
					normalForce[j + 1] = f;

					const dVector deltaForce(f - force);
					const dVector deltaForce0(deltaForce * preconditioner0);
					const dVector deltaForce1(deltaForce * preconditioner1);

					forceM0.m_linear.m_x = forceM0.m_linear.m_x.MulAdd(row->m_Jt.m_jacobianM0.m_linear.m_x, deltaForce0);
					forceM0.m_linear.m_y = forceM0.m_linear.m_y.MulAdd(row->m_Jt.m_jacobianM0.m_linear.m_y, deltaForce0);
					forceM0.m_linear.m_z = forceM0.m_linear.m_z.MulAdd(row->m_Jt.m_jacobianM0.m_linear.m_z, deltaForce0);
					forceM0.m_angular.m_x = forceM0.m_angular.m_x.MulAdd(row->m_Jt.m_jacobianM0.m_angular.m_x, deltaForce0);
					forceM0.m_angular.m_y = forceM0.m_angular.m_y.MulAdd(row->m_Jt.m_jacobianM0.m_angular.m_y, deltaForce0);
					forceM0.m_angular.m_z = forceM0.m_angular.m_z.MulAdd(row->m_Jt.m_jacobianM0.m_angular.m_z, deltaForce0);

					forceM1.m_linear.m_x = forceM1.m_linear.m_x.MulAdd(row->m_Jt.m_jacobianM1.m_linear.m_x, deltaForce1);
					forceM1.m_linear.m_y = forceM1.m_linear.m_y.MulAdd(row->m_Jt.m_jacobianM1.m_linear.m_y, deltaForce1);
					forceM1.m_linear.m_z = forceM1.m_linear.m_z.MulAdd(row->m_Jt.m_jacobianM1.m_linear.m_z, deltaForce1);
					forceM1.m_angular.m_x = forceM1.m_angular.m_x.MulAdd(row->m_Jt.m_jacobianM1.m_angular.m_x, deltaForce1);
					forceM1.m_angular.m_y = forceM1.m_angular.m_y.MulAdd(row->m_Jt.m_jacobianM1.m_angular.m_y, deltaForce1);
					forceM1.m_angular.m_z = forceM1.m_angular.m_z.MulAdd(row->m_Jt.m_jacobianM1.m_angular.m_z, deltaForce1);
				}
			}

			dVector mask(m_mask);
			if ((block + D_SSE_WORK_GROUP) > m_activeCount)
			{
				for (dInt32 i = 0; i < D_SSE_WORK_GROUP; i++)
				{
					const ndConstraint* const joint = jointGroup[i];
					if (joint)
					{
						const ndBodyKinematic* const body0 = joint->GetBody0();
						const ndBodyKinematic* const body1 = joint->GetBody1();
						dAssert(body0);
						dAssert(body1);
						const dInt32 isSleeping = body0->m_resting & body1->m_resting;
						if (isSleeping)
						{
							mask[i] = dFloat32(0.0f);
						}
					}
					else
					{
						mask[i] = dFloat32(0.0f);
					}
				}
			}

			forceM0.m_linear.m_x = m_zero;
			forceM0.m_linear.m_y = m_zero;
			forceM0.m_linear.m_z = m_zero;
			forceM0.m_angular.m_x = m_zero;
			forceM0.m_angular.m_y = m_zero;
			forceM0.m_angular.m_z = m_zero;

			forceM1.m_linear.m_x = m_zero;
			forceM1.m_linear.m_y = m_zero;
			forceM1.m_linear.m_z = m_zero;
			forceM1.m_angular.m_x = m_zero;
			forceM1.m_angular.m_y = m_zero;
			forceM1.m_angular.m_z = m_zero;
			for (dInt32 i = 0; i < rowsCount; i++)
			{
				ndSoaMatrixElement* const row = &massMatrix[i];
				const dVector force(row->m_force.Select(normalForce[i + 1], mask));
				row->m_force = force;

				forceM0.m_linear.m_x = forceM0.m_linear.m_x.MulAdd(row->m_Jt.m_jacobianM0.m_linear.m_x, force);
				forceM0.m_linear.m_y = forceM0.m_linear.m_y.MulAdd(row->m_Jt.m_jacobianM0.m_linear.m_y, force);
				forceM0.m_linear.m_z = forceM0.m_linear.m_z.MulAdd(row->m_Jt.m_jacobianM0.m_linear.m_z, force);
				forceM0.m_angular.m_x = forceM0.m_angular.m_x.MulAdd(row->m_Jt.m_jacobianM0.m_angular.m_x, force);
				forceM0.m_angular.m_y = forceM0.m_angular.m_y.MulAdd(row->m_Jt.m_jacobianM0.m_angular.m_y, force);
				forceM0.m_angular.m_z = forceM0.m_angular.m_z.MulAdd(row->m_Jt.m_jacobianM0.m_angular.m_z, force);

				forceM1.m_linear.m_x = forceM1.m_linear.m_x.MulAdd(row->m_Jt.m_jacobianM1.m_linear.m_x, force);
				forceM1.m_linear.m_y = forceM1.m_linear.m_y.MulAdd(row->m_Jt.m_jacobianM1.m_linear.m_y, force);
				forceM1.m_linear.m_z = forceM1.m_linear.m_z.MulAdd(row->m_Jt.m_jacobianM1.m_linear.m_z, force);
				forceM1.m_angular.m_x = forceM1.m_angular.m_x.MulAdd(row->m_Jt.m_jacobianM1.m_angular.m_x, force);
				forceM1.m_angular.m_y = forceM1.m_angular.m_y.MulAdd(row->m_Jt.m_jacobianM1.m_angular.m_y, force);
				forceM1.m_angular.m_z = forceM1.m_angular.m_z.MulAdd(row->m_Jt.m_jacobianM1.m_angular.m_z, force);
			}

			ndJacobian force0[4];
			ndJacobian force1[4];
			dVector::Transpose4x4(
				force0[0].m_linear,
				force0[1].m_linear,
				force0[2].m_linear,
				force0[3].m_linear,
				forceM0.m_linear.m_x,
				forceM0.m_linear.m_y,
				forceM0.m_linear.m_z, dVector::m_zero);
			dVector::Transpose4x4(
				force0[0].m_angular,
				force0[1].m_angular,
				force0[2].m_angular,
				force0[3].m_angular,
				forceM0.m_angular.m_x,
				forceM0.m_angular.m_y,
				forceM0.m_angular.m_z, dVector::m_zero);

			dVector::Transpose4x4(
				force1[0].m_linear,
				force1[1].m_linear,
				force1[2].m_linear,
				force1[3].m_linear,
				forceM1.m_linear.m_x,
				forceM1.m_linear.m_y,
				forceM1.m_linear.m_z, dVector::m_zero);
			dVector::Transpose4x4(
				force1[0].m_angular,
				force1[1].m_angular,
				force1[2].m_angular,
				force1[3].m_angular,
				forceM1.m_angular.m_x,
				forceM1.m_angular.m_y,
				forceM1.m_angular.m_z, dVector::m_zero);

			ndRightHandSide* const rightHandSide = &m_rightHandSide[0];
			for (dInt32 i = 0; i < D_SSE_WORK_GROUP; i++)
			{
				const ndConstraint* const joint = jointGroup[i];
				if (joint)
				{
					const ndBodyKinematic* const body0 = joint->GetBody0();
					const ndBodyKinematic* const body1 = joint->GetBody1();

					const dInt32 m0 = body0->m_index;
					const dInt32 m1 = body1->m_index;

					dInt32 const rowCount = joint->m_rowCount;
					dInt32 const rowStartBase = joint->m_rowStart;
					for (dInt32 j = 0; j < rowCount; j++)
					{
						const ndSoaMatrixElement* const row = &massMatrix[j];
						rightHandSide[j + rowStartBase].m_force = row->m_force[i];
						rightHandSide[j + rowStartBase].m_maxImpact = dMax(dAbs(row->m_force[i]), rightHandSide[j + rowStartBase].m_maxImpact);
					}

					if (body1->GetInvMass() > dFloat32(0.0f))
					{
						ndJacobian& outBody1 = m_outputForces[m1];
						dScopeSpinLock lock(body1->m_lock);
						outBody1.m_linear += force1[i].m_linear;
						outBody1.m_angular += force1[i].m_angular;
					}

					ndJacobian& outBody0 = m_outputForces[m0];
					dScopeSpinLock lock(body0->m_lock);
					outBody0.m_linear += force0[i].m_linear;
					outBody0.m_angular += force0[i].m_angular;
				}
			}

			accNorm = m_zero.Select(accNorm, mask);
			return accNorm.AddHorizontal().GetScalar();
		}

		virtual void Execute()
		{
			D_TRACKTIME();
			ndWorld* const world = m_owner->GetWorld();
			ndDynamicsUpdateSoa* const me = (ndDynamicsUpdateSoa*)world->m_solver;
			m_leftHandSide = &me->m_leftHandSide[0];
			m_rightHandSide = &me->m_rightHandSide[0];
			m_internalForces = &me->GetInternalForces()[0];
			m_outputForces = &me->GetTempInternalForces()[0];

			ndConstraintArray& jointArray = m_owner->GetActiveContactArray();
			dFloat32 accNorm = dFloat32(0.0f);
			const dInt32 jointCount = jointArray.GetCount();
			m_activeCount = me->m_activeJointCount;
			const dInt32 bodyCount = m_owner->GetActiveBodyArray().GetCount();

			const dInt32 threadIndex = GetThreadId();
			const dInt32 threadCount = dMax(m_owner->GetThreadCount(), 1);

			m_jointArray = &jointArray[0];

			const dInt32* const soaJointRows = &me->m_soaJointRows[0];
			ndSoaMatrixElement* const soaMassMatrix = &me->m_soaMassMatrix[0];

			m_mask = dVector::m_xyzwMask;
			const dInt32 mask = -dInt32(D_SSE_WORK_GROUP);
			const dInt32 soaJointCount = ((jointCount + D_SSE_WORK_GROUP - 1) & mask) / D_SSE_WORK_GROUP;
			for (dInt32 i = threadIndex; i < soaJointCount; i += threadCount)
			{
				accNorm += JointForce(i * D_SSE_WORK_GROUP, &soaMassMatrix[soaJointRows[i]]);
			}

			dFloat32* const accelNorm = (dFloat32*)m_context;
			accelNorm[threadIndex] = accNorm;
		}

		dVector m_one;
		dVector m_zero;
		dVector m_mask;
		ndJacobian* m_outputForces;
		ndJacobian* m_internalForces;
		ndRightHandSide* m_rightHandSide;
		const ndLeftHandSide* m_leftHandSide;
		ndConstraint** m_jointArray;

		dInt32 m_activeCount;
	};

	ndScene* const scene = m_world->GetScene();
	const dInt32 passes = m_solverPasses;
	const dInt32 threadsCount = dMax(scene->GetThreadCount(), 1);

	dFloat32 m_accelNorm[D_MAX_THREADS_COUNT];
	dFloat32 accNorm = D_SOLVER_MAX_ERROR * dFloat32(2.0f);

	for (dInt32 i = 0; (i < passes) && (accNorm > D_SOLVER_MAX_ERROR); i++)
	{
		ClearJacobianBuffer(GetTempInternalForces().GetCount(), &GetTempInternalForces()[0]);
		scene->SubmitJobs<ndCalculateJointsForce>(m_accelNorm);
		GetInternalForces().Swap(GetTempInternalForces());

		accNorm = dFloat32(0.0f);
		for (dInt32 j = 0; j < threadsCount; j++)
		{
			accNorm = dMax(accNorm, m_accelNorm[j]);
		}
	}
}

void ndDynamicsUpdateSoa::CalculateForces()
{
	D_TRACKTIME();
	if (m_world->GetScene()->GetActiveContactArray().GetCount())
	{
		m_firstPassCoef = dFloat32(0.0f);
		if (m_world->m_skeletonList.GetCount())
		{
			InitSkeletons();
		}
		
		for (dInt32 step = 0; step < 4; step++)
		{
			CalculateJointsAcceleration();
			CalculateJointsForce();
			if (m_world->m_skeletonList.GetCount())
			{
				UpdateSkeletons();
			}
			IntegrateBodiesVelocity();
		}
		
		UpdateForceFeedback();
	}
}

void ndDynamicsUpdateSoa::Update()
{
	D_TRACKTIME();
	m_timestep = m_world->GetScene()->GetTimestep();

	BuildIsland();
	if (m_islands.GetCount())
	{
		IntegrateUnconstrainedBodies();
		InitWeights();
		InitBodyArray();
		InitJacobianMatrix();
		CalculateForces();
		IntegrateBodies();
		DetermineSleepStates();
	}
}
