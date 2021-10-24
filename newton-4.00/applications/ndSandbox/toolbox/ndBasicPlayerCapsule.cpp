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
#include "ndTargaToOpenGl.h"
#include "ndDemoMesh.h"
#include "ndDemoCamera.h"
#include "ndPhysicsUtils.h"
#include "ndPhysicsWorld.h"
#include "ndDemoEntityManager.h"
#include "ndAnimationSequence.h"
#include "ndBasicPlayerCapsule.h"
#include "ndAnimationTwoWayBlend.h"
#include "ndAnimationSequencePlayer.h"

#define PLAYER_WALK_SPEED				8.0f
#define PLAYER_JUMP_SPEED				5.0f

D_CLASS_REFLECTION_IMPLEMENT_LOADER(ndBasicPlayerCapsule)

//#define PLAYER_FIRST_PERSON	

#ifdef PLAYER_FIRST_PERSON	
	#define PLAYER_THIRD_PERSON_VIEW_DIST	0.0f
#else
	#define PLAYER_THIRD_PERSON_VIEW_DIST	8.0f
#endif

class ndBasicPlayerCapsuleNotify : public ndDemoEntityNotify
{
	public:
	ndBasicPlayerCapsuleNotify(ndDemoEntityManager* const manager, ndDemoEntity* const entity)
		:ndDemoEntityNotify(manager, entity)
	{
	}

	void OnTransform(dInt32 thread, const dMatrix& matrix)
	{
		ndDemoEntityNotify::OnTransform(thread, matrix);

		ndWorld* const word = m_manager->GetWorld();
		ndBasicPlayerCapsule* const player = (ndBasicPlayerCapsule*)GetBody();

		dFloat32 timestep = word->GetScene()->GetTimestep();
		//timestep *= 0.25f;
		//timestep = 1.0f/(30.0f * 4.0f);
		timestep *= 0.05f;
		player->m_animBlendTree->Evaluate(player->m_output, timestep);

		for (int i = 0; i < player->m_output.GetCount(); i++)
		{
			const ndAnimKeyframe& keyFrame = player->m_output[i];
			ndDemoEntity* const entity = (ndDemoEntity*)keyFrame.m_userData;
			entity->SetMatrix(keyFrame.m_rotation, keyFrame.m_posit);
		}
	}
};

ndBasicPlayerCapsule::ndBasicPlayerCapsule(
	ndDemoEntityManager* const scene, const ndDemoEntity* const modelEntity,
	const dMatrix& localAxis, const dMatrix& location,
	dFloat32 mass, dFloat32 radius, dFloat32 height, dFloat32 stepHeight, bool isPlayer)
	:ndBodyPlayerCapsule(localAxis, mass, radius, height, stepHeight)
	,m_scene(scene)
	,m_isPlayer(isPlayer)
	,m_output()
	,m_animBlendTree(nullptr)
{
	dMatrix matrix(location);
	ndPhysicsWorld* const world = scene->GetWorld();
	dVector floor(FindFloor(*world, matrix.m_posit + dVector(0.0f, 100.0f, 0.0f, 0.0f), 200.0f));
	//matrix.m_posit.m_y = floor.m_y + 1.0f;
	matrix.m_posit.m_y = floor.m_y;
	
	ndDemoEntity* const entity = (ndDemoEntity*)modelEntity->CreateClone();
	entity->ResetMatrix(matrix);

	SetMatrix(matrix);
	world->AddBody(this);
	scene->AddEntity(entity);
	SetNotifyCallback(new ndBasicPlayerCapsuleNotify(scene, entity));

	if (isPlayer)
	{
		scene->SetUpdateCameraFunction(UpdateCameraCallback, this);
	}

	// create bind pose to animation sequences.
	ndAnimationSequence* const sequence = scene->GetAnimationSequence("whiteMan_idle.fbx");
	const dList<ndAnimationKeyFramesTrack>& tracks = sequence->m_tracks;
	for (dList<ndAnimationKeyFramesTrack>::dNode* node = tracks.GetFirst(); node; node = node->GetNext()) 
	{
		ndAnimationKeyFramesTrack& track = node->GetInfo();
		ndDemoEntity* const ent = entity->Find(track.GetName().GetStr());
		ndAnimKeyframe keyFrame;
		keyFrame.m_userData = ent;
		m_output.PushBack(keyFrame);
	}

	// create an animation blend tree
	ndAnimationSequence* const idleSequence = scene->GetAnimationSequence("whiteMan_idle.fbx");
	ndAnimationSequence* const walkSequence = scene->GetAnimationSequence("whiteman_walk.fbx");
	ndAnimationSequence* const runSequence = scene->GetAnimationSequence("whiteman_run.fbx");

	ndAnimationSequencePlayer* const idle = new ndAnimationSequencePlayer(idleSequence);
	ndAnimationSequencePlayer* const walk = new ndAnimationSequencePlayer(walkSequence);
	ndAnimationSequencePlayer* const run = new ndAnimationSequencePlayer(runSequence);
	
	////dFloat scale0 = walkSequence->GetPeriod() / runSequence->GetPeriod();
	//dFloat32 scale1 = runSequence->GetPeriod() / walkSequence->GetPeriod();
	ndAnimationTwoWayBlend* const walkRunBlend = new ndAnimationTwoWayBlend(walk, run);
	ndAnimationTwoWayBlend* const idleMoveBlend = new ndAnimationTwoWayBlend(idle, walkRunBlend);
	
	walkRunBlend->SetParam(0.0f);
	//idleMoveBlend->SetParam(0.0f);
	idleMoveBlend->SetParam(1.0f);
	//walkRunBlend->SetTimeDilation1(scale1);
	m_animBlendTree = idleMoveBlend;

	// evaluate twice that interpolation is reset
	m_animBlendTree->Evaluate(m_output, dFloat32(0.0f));
	m_animBlendTree->Evaluate(m_output, dFloat32(0.0f));
}

ndBasicPlayerCapsule::ndBasicPlayerCapsule(const dLoadSaveBase::dLoadDescriptor& desc)
	:ndBodyPlayerCapsule(dLoadSaveBase::dLoadDescriptor(desc))
	,m_scene(nullptr)
	,m_isPlayer(false)
	,m_output()
	,m_animBlendTree(nullptr)
{
	//dAssert(0);
	//for now do not load the player configuration, we can do that is the postprocess pass. 
	//m_isPlayer = xmlGetInt(xmlNode, "isPlayer") ? true : false;
	//m_scene = world->GetManager();
	//if (m_isPlayer)
	//{
	//	m_scene->SetUpdateCameraFunction(UpdateCameraCallback, this);
	//}
	//
	//ndDemoEntity* const entity = new ndDemoEntity(m_localFrame, nullptr);
	//const ndShapeInstance& shape = GetCollisionShape();
	//ndDemoMesh* const mesh = new ndDemoMesh("shape", m_scene->GetShaderCache(), &shape, "smilli.tga", "marble.tga", "marble.tga");
	//entity->SetMesh(mesh, dGetIdentityMatrix());
	//mesh->Release();
	//
	//m_scene->AddEntity(entity);
	//SetNotifyCallback(new ndDemoEntityNotify(m_scene, entity));
}

ndBasicPlayerCapsule::~ndBasicPlayerCapsule()
{
	if (m_animBlendTree)
	{
		delete m_animBlendTree;
	}
}

void ndBasicPlayerCapsule::Save(const dLoadSaveBase::dSaveDescriptor& desc) const
{
	nd::TiXmlElement* const childNode = new nd::TiXmlElement(ClassName());
	desc.m_rootNode->LinkEndChild(childNode);
	childNode->SetAttribute("hashId", desc.m_nodeNodeHash);
	ndBodyPlayerCapsule::Save(dLoadSaveBase::dSaveDescriptor(desc, childNode));

	//xmlSaveParam(paramNode, "isPlayer", m_isPlayer ? 1 : 0);
}

void ndBasicPlayerCapsule::ApplyInputs(dFloat32 timestep)
{
	//calculate the gravity contribution to the velocity, 
	const dVector gravity(GetNotifyCallback()->GetGravity());
	const dVector totalImpulse(m_impulse + gravity.Scale(m_mass * timestep));
	m_impulse = totalImpulse;

	//dTrace(("  frame: %d    player camera: %f\n", m_scene->GetWorld()->GetFrameIndex(), m_playerInput.m_heading * dRadToDegree));
	if (m_playerInput.m_jump)
	{
		dVector jumpImpule(0.0f, PLAYER_JUMP_SPEED * m_mass, 0.0f, 0.0f);
		m_impulse += jumpImpule;
		m_playerInput.m_jump = false;
	}

	SetForwardSpeed(m_playerInput.m_forwardSpeed);
	SetLateralSpeed(m_playerInput.m_strafeSpeed);
	SetHeadingAngle(m_playerInput.m_heading);
}

//dFloat32 ndBasicPlayerCapsule::ContactFrictionCallback(const dVector& position, const dVector& normal, dInt32 contactId, const ndBodyKinematic* const otherbody) const
dFloat32 ndBasicPlayerCapsule::ContactFrictionCallback(const dVector&, const dVector& normal, dInt32, const ndBodyKinematic* const) const
{
	//return dFloat32(2.0f);
	if (dAbs(normal.m_y) < 0.8f)
	{
		return 0.4f;
	}
	return dFloat32(2.0f);
}

void ndBasicPlayerCapsule::SetCamera()
{
	if (m_isPlayer)
	{
		ndDemoCamera* const camera = m_scene->GetCamera();
		dMatrix camMatrix(camera->GetNextMatrix());

		ndDemoEntityNotify* const notify = (ndDemoEntityNotify*)GetNotifyCallback();
		ndDemoEntity* const player = (ndDemoEntity*)notify->GetUserData();
		dMatrix playerMatrix(player->GetNextMatrix());

		const dFloat32 height = m_height;
		const dVector frontDir(camMatrix[0]);
		const dVector upDir(0.0f, 1.0f, 0.0f, 0.0f);
		dVector camOrigin = playerMatrix.TransformVector(upDir.Scale(height));
		camOrigin -= frontDir.Scale(PLAYER_THIRD_PERSON_VIEW_DIST);

		camera->SetNextMatrix(camMatrix, camOrigin);

		dFloat32 angle0 = camera->GetYawAngle();
		dFloat32 angle1 = GetHeadingAngle();
		dFloat32 error = AnglesAdd(angle1, -angle0);

		if ((dAbs (error) > 1.0e-3f) ||
			m_scene->GetKeyState(' ') ||
			m_scene->GetKeyState('A') ||
			m_scene->GetKeyState('D') ||
			m_scene->GetKeyState('W') ||
			m_scene->GetKeyState('S'))
		{
			SetSleepState(false);
		}

		m_playerInput.m_heading = camera->GetYawAngle();
		m_playerInput.m_forwardSpeed = (dInt32(m_scene->GetKeyState('W')) - dInt32(m_scene->GetKeyState('S'))) * PLAYER_WALK_SPEED;
		m_playerInput.m_strafeSpeed = (dInt32(m_scene->GetKeyState('D')) - dInt32(m_scene->GetKeyState('A'))) * PLAYER_WALK_SPEED;
		m_playerInput.m_jump = m_scene->GetKeyState(' ') && IsOnFloor();

		if (m_playerInput.m_forwardSpeed && m_playerInput.m_strafeSpeed)
		{
			dFloat32 invMag = PLAYER_WALK_SPEED / dSqrt(m_playerInput.m_forwardSpeed * m_playerInput.m_forwardSpeed + m_playerInput.m_strafeSpeed * m_playerInput.m_strafeSpeed);
			m_playerInput.m_forwardSpeed *= invMag;
			m_playerInput.m_strafeSpeed *= invMag;
		}
	}
}

//void ndBasicPlayerCapsule::UpdateCameraCallback(ndDemoEntityManager* const manager, void* const context, dFloat32 timestep)
void ndBasicPlayerCapsule::UpdateCameraCallback(ndDemoEntityManager* const, void* const context, dFloat32)
{
	ndBasicPlayerCapsule* const me = (ndBasicPlayerCapsule*)context;
	me->SetCamera();
}
