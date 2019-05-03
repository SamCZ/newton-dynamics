/* Copyright (c) <2003-2016> <Newton Game Dynamics>
* 
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
* 
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely
*/


// NewtonCustomJoint.cpp: implementation of the NewtonCustomJoint class.
//
//////////////////////////////////////////////////////////////////////
#include "dCustomJointLibraryStdAfx.h"
#include "dCustomJoint.h"
#include "dCustomPlayerControllerManager.h"


#define D_DESCRETE_MOTION_STEPS		4
#define D_MAX_COLLIONSION_STEPS		8
#define D_MAX_COLLISION_PENTRATION	dFloat (2.0e-3f)

dCustomPlayerControllerManager::dCustomPlayerControllerManager(NewtonWorld* const world)
	:dCustomParallelListener(world, PLAYER_PLUGIN_NAME)
	,m_playerList()
{
}

dCustomPlayerControllerManager::~dCustomPlayerControllerManager()
{
	m_playerList.RemoveAll();
	dAssert(m_playerList.GetCount() == 0);
}

void dCustomPlayerControllerManager::PreUpdate(dFloat timestep, int threadID)
{
	D_TRACKTIME();
	NewtonWorld* const world = GetWorld();
	const int threadCount = NewtonGetThreadsCount(world);

	dList<dCustomPlayerController>::dListNode* node = m_playerList.GetFirst();
	for (int i = 0; i < threadID; i++) {
		node = node ? node->GetNext() : NULL;
	}
	if (node) {
		dCustomPlayerController* const controller = &node->GetInfo();
		controller->PreUpdate(timestep);
		do {
			for (int i = 0; i < threadCount; i++) {
				node = node ? node->GetNext() : NULL;
			}
		} while (node);
	}
}

void dCustomPlayerControllerManager::PostUpdate(dFloat timestep, int threadID)
{
	D_TRACKTIME();
	NewtonWorld* const world = GetWorld();
	const int threadCount = NewtonGetThreadsCount(world);

	dList<dCustomPlayerController>::dListNode* node = m_playerList.GetFirst();
	for (int i = 0; i < threadID; i++) {
		node = node ? node->GetNext() : NULL;
	}
	if (node) {
		dCustomPlayerController* const controller = &node->GetInfo();
		controller->PostUpdate(timestep);
		do {
			for (int i = 0; i < threadCount; i++) {
				node = node ? node->GetNext() : NULL;
			}
		} while (node);
	}
}

int dCustomPlayerControllerManager::ProcessContacts(const dCustomPlayerController* const controller, NewtonWorldConvexCastReturnInfo* const contacts, int count) const
{
	dAssert(0);
	return 0;
}

dCustomPlayerController* dCustomPlayerControllerManager::CreatePlayerController(const dMatrix& location, const dMatrix& localAxis, dFloat mass, dFloat radius, dFloat height)
{
	NewtonWorld* const world = GetWorld();

	dMatrix shapeMatrix(localAxis);
	shapeMatrix.m_posit = shapeMatrix.m_front.Scale (height * 0.5f);
	shapeMatrix.m_posit.m_w = 1.0f;
	height = dMax (height - 2.0f * radius, dFloat (0.1f));
	NewtonCollision* const bodyCapsule = NewtonCreateCapsule(world, radius, radius, height, 0, &shapeMatrix[0][0]);

	// create the kinematic body
	NewtonBody* const body = NewtonCreateKinematicBody(world, bodyCapsule, &location[0][0]);

	// players must have weight, otherwise they are infinitely strong when they collide
	NewtonCollision* const shape = NewtonBodyGetCollision(body);
	NewtonBodySetMassProperties(body, mass, shape);

	// make the body collidable with other dynamics bodies, by default
	NewtonBodySetCollidable(body, 1);
	NewtonDestroyCollision(bodyCapsule);

	dCustomPlayerController& controller = m_playerList.Append()->GetInfo();

	controller.m_mass = mass;
	controller.m_invMass = 1.0f / mass;
	controller.m_manager = this;
	controller.m_kinematicBody = body;
	return &controller;
}


dVector dCustomPlayerController::GetVelocity() const
{ 
	dVector veloc(0.0);
	NewtonBodyGetVelocity(m_kinematicBody, &veloc[0]);
	return veloc; 
}

void dCustomPlayerController::SetVelocity(const dVector& veloc) 
{ 
	NewtonBodySetVelocity(m_kinematicBody, &veloc[0]);
}


void dCustomPlayerController::PreUpdate(dFloat timestep)
{
	m_impulse = dVector(0.0f);
	m_manager->ApplyPlayerMove(this, timestep);

	dVector veloc(GetVelocity() + m_impulse.Scale(m_invMass));
	NewtonBodySetVelocity(m_kinematicBody, &veloc[0]);
}


void dCustomPlayerController::PostUpdate(dFloat timestep)
{
	dFloat timeLeft = timestep;
	const dFloat timeEpsilon = timestep * (1.0f / 16.0f);

	for (int i = 0; (i < D_DESCRETE_MOTION_STEPS) && (timeLeft > timeEpsilon); i++) {
		if (timeLeft > timeEpsilon) {
			ResolveCollision();
		}

		dFloat predicetdTime = PredictTimestep(timestep);
		NewtonBodyIntegrateVelocity(m_kinematicBody, predicetdTime);
		timeLeft -= predicetdTime;
	}

//	dAssert(timeLeft < timeEpsilon);
//	NewtonBodyGetVelocity(m_kinematicBody, &m_veloc[0]);
}


unsigned dCustomPlayerController::PrefilterCallback(const NewtonBody* const body, const NewtonCollision* const collision, void* const userData)
{
	dCustomPlayerController* const controller = (dCustomPlayerController*)userData;
	if (controller->GetBody() == body) {
		return false;
	}
	return 1;
}

void dCustomPlayerController::ResolveCollision()
{
	dMatrix matrix;
	NewtonWorldConvexCastReturnInfo info[16];

	NewtonWorld* const world = m_manager->GetWorld();
		
	NewtonBodyGetMatrix(m_kinematicBody, &matrix[0][0]);
	NewtonCollision* const shape = NewtonBodyGetCollision(m_kinematicBody);

	int contactCount = NewtonWorldCollide(world, &matrix[0][0], shape, this, PrefilterCallback, info, 4, 0);
	if (!contactCount) {
		return;
	}

	int rowCount;
	dVector zero(0.0f);

	dMatrix invInertia;
	dComplementaritySolver::dJacobian jt[16];
	dComplementaritySolver::dJacobian jInvMass[16];
	dFloat rhs[16];

	NewtonBodyGetInvInertiaMatrix(m_kinematicBody, &invInertia[0][0]);

	for (int i = 0; i < 3; i++) {
		rhs[i] = 0.0f;
		jt[i].m_linear = zero;
		jt[i].m_angular = zero;
		jt[i].m_angular[i] = dFloat(1.0f);

		jInvMass[i].m_linear = zero;
		jInvMass[i].m_angular = invInertia.UnrotateVector(jt[i].m_angular);
	}
	

	dVector veloc(0.0f);
	NewtonBodySetVelocity(m_kinematicBody, &veloc[0]);
	dTrace(("implement collsion rsolution !!!\n"));
}

dFloat dCustomPlayerController::PredictTimestep(dFloat timestep)
{
	dMatrix matrix;
	dMatrix predicMatrix;
	NewtonWorld* const world = m_manager->GetWorld();
	
	NewtonWorldConvexCastReturnInfo info[16];
	NewtonBodyGetMatrix(m_kinematicBody, &matrix[0][0]);
	NewtonCollision* const shape = NewtonBodyGetCollision(m_kinematicBody);
	
	NewtonBodyIntegrateVelocity(m_kinematicBody, timestep);
	NewtonBodyGetMatrix(m_kinematicBody, &predicMatrix[0][0]);
	int contactCount = NewtonWorldCollide(world, &predicMatrix[0][0], shape, this, PrefilterCallback, info, 4, 0);
	NewtonBodySetMatrix(m_kinematicBody, &matrix[0][0]);

	if (contactCount) {
		dFloat t0 = 0.0f;
		dFloat t1 = timestep;
		dFloat dt = (t1 + t0) * 0.5f;
		timestep = dt;
		for (int i = 0; i < D_MAX_COLLIONSION_STEPS; i++) {
			NewtonBodyIntegrateVelocity(m_kinematicBody, timestep);
			NewtonBodyGetMatrix(m_kinematicBody, &predicMatrix[0][0]);
			contactCount = NewtonWorldCollide(world, &predicMatrix[0][0], shape, this, PrefilterCallback, info, 4, 0);
			NewtonBodySetMatrix(m_kinematicBody, &matrix[0][0]);

			dt *= 0.5f;
			if (contactCount) {
				dFloat penetration = 0.0f;
				for (int j = 0; j < contactCount; j++) {
					penetration = dMax(penetration, info[j].m_penetration);
				}
				if (penetration < D_MAX_COLLISION_PENTRATION) {
					break;
				}
				timestep -= dt;
			} else {
				timestep += dt;
			}
		}
	}

	return timestep;
}
