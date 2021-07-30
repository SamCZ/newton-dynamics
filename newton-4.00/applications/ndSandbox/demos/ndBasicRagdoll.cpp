/* Copyright (c) <2003-2021> <Newton Game Dynamics>
* 
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
* 
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely
*/

#include "ndSandboxStdafx.h"
#include "ndSkyBox.h"
#include "ndTargaToOpenGl.h"
#include "ndDemoMesh.h"
#include "ndDemoCamera.h"
#include "ndLoadFbxMesh.h"
#include "ndPhysicsUtils.h"
#include "ndPhysicsWorld.h"
#include "ndMakeStaticMap.h"
#include "ndDemoEntityManager.h"
#include "ndDemoInstanceEntity.h"

class ndRagDollModel : public ndModel
{
	public:
	ndRagDollModel(ndDemoEntityManager* const scene, fbxDemoEntity* const ragdollMesh, const dMatrix& matrix)
	{
		ndDemoEntity* const entity = ragdollMesh->CreateClone();
		entity->ResetMatrix(matrix);
		scene->AddEntity(entity);

	}

	void Update(ndWorld* const, dFloat32) 
	{
	}

	//void PostUpdate(ndWorld* const world, dFloat32)
	void PostUpdate(ndWorld* const, dFloat32)
	{
	}

	//void PostTransformUpdate(ndWorld* const world, dFloat32 timestep)
	void PostTransformUpdate(ndWorld* const, dFloat32)
	{

	}

	ndDemoEntityManager::ndKeyTrigger m_changeVehicle;
};

void ndBasicRagdoll (ndDemoEntityManager* const scene)
{
	// build a floor
	BuildFloorBox(scene, dGetIdentityMatrix());

	dVector origin1(0.0f, 0.0f, 0.0f, 0.0f);
	fbxDemoEntity* const ragdollMesh = scene->LoadFbxMesh("whiteMan.fbx");

	dMatrix matrix(dGetIdentityMatrix());
	matrix.m_posit.m_y = 0.5f;
	ndRagDollModel* const ragdoll = new ndRagDollModel(scene, ragdollMesh, matrix);
	scene->GetWorld()->AddModel(ragdoll);

	//AddCapsulesStacks(scene, origin1, 10.0f, 0.5f, 0.5f, 1.0f, 10, 10, 7);

	delete ragdollMesh;
	dQuaternion rot;
	dVector origin(-10.0f, 1.0f, 0.0f, 0.0f);
	scene->SetCameraMatrix(rot, origin);
}
