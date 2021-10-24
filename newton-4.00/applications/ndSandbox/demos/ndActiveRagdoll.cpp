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

#include "ndAnimationPose.h"
#include "ndAnimationSequence.h"
#include "ndBasicPlayerCapsule.h"
#include "ndAnimationKeyframesTrack.h"
#include "ndAnimationSequencePlayer.h"


class dActiveJointDefinition
{
	public:
	enum dLimbType
	{
		forwardKinematic,
		ballAndSocket,
		effector,
	};

	struct dJointPdData
	{
		dJointPdData()
			:m_spring(1500.0f)
			,m_damper(40.0f)
			,m_regularizer(0.001f)
		{
		}

		dJointPdData(dFloat32 spring, dFloat32 damper, dFloat32 regularizer)
			:m_spring(spring)
			,m_damper(damper)
			,m_regularizer(regularizer)
		{
		}

		dFloat32 m_spring;
		dFloat32 m_damper;
		dFloat32 m_regularizer;
	};

	struct dJointLimit
	{
		dFloat32 m_minTwistAngle;
		dFloat32 m_maxTwistAngle;
		dFloat32 m_coneAngle;
	};

	struct dFrameMatrix
	{
		dFloat32 m_pitch;
		dFloat32 m_yaw;
		dFloat32 m_roll;
	};

	char m_boneName[32];
	dLimbType m_limbType;
	dFloat32 m_massWeight;
	dJointLimit m_jointLimits;
	dFrameMatrix m_frameBasics;
	dJointPdData m_coneSpringData;
	dJointPdData m_twistSpringData;
};

class ndActiveRagdollEntityNotify : public ndDemoEntityNotify
{
	public:
	ndActiveRagdollEntityNotify(ndDemoEntityManager* const manager, ndDemoEntity* const entity, ndBodyDynamic* const parentBody)
		:ndDemoEntityNotify(manager, entity, parentBody)
		,m_bindMatrix(dGetIdentityMatrix())
	{
		if (parentBody)
		{
			ndDemoEntity* const parentEntity = (ndDemoEntity*)(parentBody->GetNotifyCallback()->GetUserData());
			m_bindMatrix = entity->GetParent()->CalculateGlobalMatrix(parentEntity).Inverse();
		}
	}

	void OnTransform(dInt32 thread, const dMatrix& matrix)
	{
		if (!m_parentBody)
		{
			ndDemoEntityNotify::OnTransform(thread, matrix);
		}
		else
		{
			const dMatrix parentMatrix(m_parentBody->GetMatrix());
			const dMatrix localMatrix(matrix * parentMatrix.Inverse() * m_bindMatrix);
			const dQuaternion rot(localMatrix);
			m_entity->SetMatrix(rot, localMatrix.m_posit);
		}
	}

	void OnApplyExternalForce(dInt32 thread, dFloat32 timestep)
	{
		ndDemoEntityNotify::OnApplyExternalForce(thread, timestep);
		// remember to check and clamp huge angular velocities
	}

	dMatrix m_bindMatrix;
};

static dActiveJointDefinition jointsDefinition[] =
{
	{ "mixamorig:Hips", dActiveJointDefinition::forwardKinematic, 1.0f, {}, {}, {} },
	
	//{ "mixamorig:Spine", dActiveJointDefinition::forwardKinematic, 1.0f, { -15.0f, 15.0f,  30.0f }, { 0.0f, 0.0f, 180.0f }, {} },
	//{ "mixamorig:Spine1", dActiveJointDefinition::forwardKinematic, 1.0f, { -15.0f, 15.0f, 30.0f }, { 0.0f, 0.0f, 180.0f }, {}  },
	//{ "mixamorig:Spine2", dActiveJointDefinition::forwardKinematic, 1.0f, { -15.0f, 15.0f, 30.0f }, { 0.0f, 0.0f, 180.0f }, {}  },
	//{ "mixamorig:Neck", dActiveJointDefinition::forwardKinematic, 1.0f, { -15.0f, 15.0f, 30.0f }, { 0.0f, 0.0f, 180.0f }, {}  },
	
	//{ "mixamorig:RightArm", dActiveJointDefinition::forwardKinematic, 1.0f, { -45.0f, 45.0f, 80.0f }, { 0.0f, 0.0f, 180.0f }, {}  },
	//{ "mixamorig:RightForeArm", dActiveJointDefinition::forwardKinematic, 1.0f, { -140.0f, 10.0f, 0.0f }, { 0.0f, 00.0f, 90.0f }, {}  },
	//{ "mixamorig:RightHand", dActiveJointDefinition::forwardKinematic, 2.0f, { 0.0f, 0.0f, 60.0f }, { 0.0f, 0.0f, 180.0f }, {}  },
	
	//{ "mixamorig:LeftArm", dActiveJointDefinition::forwardKinematic, 1.0f, { -45.0f, 45.0f, 80.0f }, { 0.0f, 0.0f, 180.0f }, {}  },
	//{ "mixamorig:LeftForeArm", dActiveJointDefinition::forwardKinematic, 1.0f, { -140.0f, 10.0f, 0.0f }, { 0.0f, 0.0f, -90.0f }, {}  },
	//{ "mixamorig:LeftHand", dActiveJointDefinition::forwardKinematic, 2.0f, { 0.0f, 0.0f, 60.0f }, { 0.0f, 0.0f, 180.0f }, {} },
	
	{ "mixamorig:RightUpLeg", dActiveJointDefinition::ballAndSocket, 1.0f, { -45.0f, 45.0f, 120.0f }, { 0.0f, 180.0f, 0.0f }, {} },
	{ "mixamorig:RightLeg", dActiveJointDefinition::ballAndSocket, 1.0f, { -140.0f, 0.0f, 0.0f }, { 0.0f, 90.0f, 90.0f }, {} },
	{ "mixamorig:RightFoot", dActiveJointDefinition::ballAndSocket, 1.0f, { 0.0f, 0.0f, 1.0e4f }, { 0.0f, 0.0f, 180.0f },{} },
	
	//{ "mixamorig:LeftUpLeg", dActiveJointDefinition::ballAndSocket, 1.0f, { -45.0f, 45.0f, 120.0f }, { 0.0f, 180.0f, 0.0f }, {} },
	//{ "mixamorig:LeftLeg", dActiveJointDefinition::ballAndSocket, 1.0f, { -140.0f, 0.0f, 0.0f }, { 0.0f, 90.0f, 90.0f }, {} },
	//{ "mixamorig:LeftFoot", dActiveJointDefinition::ballAndSocket, 1.0f, { 0.0f, 0.0f, 1.0e4f }, { 0.0f, 0.0f, 180.0f }, {} },
};

class ndActiveRagdollModel : public ndCharacter
{
	public:
	ndActiveRagdollModel(ndDemoEntityManager* const scene, fbxDemoEntity* const ragdollMesh, const dMatrix& location)
		:ndCharacter()
		,m_animBlendTree(nullptr)
	{
		// make a clone of the mesh and add it to the scene
		ndDemoEntity* const entity = ragdollMesh->CreateClone();
		scene->AddEntity(entity);
		ndWorld* const world = scene->GetWorld();
		
		// find the floor location 
		dMatrix matrix(location);
		dVector floor(FindFloor(*world, matrix.m_posit + dVector(0.0f, 100.0f, 0.0f, 0.0f), 200.0f));
		matrix.m_posit.m_y = floor.m_y;
		matrix.m_posit.m_y += 0.5f;

		// add the root body
		ndDemoEntity* const rootEntity = (ndDemoEntity*)entity->Find(jointsDefinition[0].m_boneName);
		rootEntity->ResetMatrix(rootEntity->GetCurrentMatrix() * matrix);
		ndCharacterRootNode* const rootNode = CreateRoot(CreateBodyPart(scene, rootEntity, nullptr));
		ndDemoEntity* const characterFrame = (ndDemoEntity*)entity->Find("referenceFrame");
		dMatrix coronalFrame(dPitchMatrix(180.0f*dDegreeToRad) * dRollMatrix(90.0f*dDegreeToRad) * characterFrame->CalculateGlobalMatrix());
		rootNode->SetCoronalFrame(coronalFrame);
		rootNode->SetName(rootEntity->GetName().GetStr());

		dInt32 stack = 0;
		const dInt32 definitionCount = dInt32 (sizeof(jointsDefinition) / sizeof(jointsDefinition[0]));
		
		ndDemoEntity* childEntities[32];
		ndCharacterNode* parentBones[32];
		for (ndDemoEntity* child = rootEntity->GetChild(); child; child = child->GetSibling()) 
		{
			childEntities[stack] = child;
			parentBones[stack] = rootNode;
			stack++;
		}
		
		dInt32 bodyCount = 1;
		dFloat32 massWeight[1024];
		ndBodyDynamic* bodyArray[1024];
		massWeight[0] = 1.0f;
		bodyArray[0] = rootNode->GetBody();

		//ndBipedControllerConfig bipedConfig;
		// walk model hierarchic adding all children designed as rigid body bones. 

		ndCharacterNode* righFoot = nullptr;
		ndCharacterNode* leftFoot = nullptr;
		while (stack) 
		{
			stack--;
			ndCharacterNode* parentBone = parentBones[stack];
			ndDemoEntity* const childEntity = childEntities[stack];
			const char* const name = childEntity->GetName().GetStr();
			//dTrace(("name: %s\n", name));
			for (dInt32 i = 0; i < definitionCount; i++) 
			{
				const dActiveJointDefinition& definition = jointsDefinition[i];
				if (!strcmp(definition.m_boneName, name))
				{
					if (definition.m_limbType != dActiveJointDefinition::effector)
					{
						ndBodyDynamic* const childBody = CreateBodyPart(scene, childEntity, parentBone->GetBody());
						bodyArray[bodyCount] = childBody;
						massWeight[bodyCount] = definition.m_massWeight;
						bodyCount++;

						// connect this body part to its parentBody with a ragdoll joint
						parentBone = ConnectBodyParts(childBody, parentBone, definition);
						parentBone->SetName(name);

						if (strstr(name, "RightFoot"))
						{
							righFoot = parentBone;
							//bipedConfig.m_rightFootNode = parentBone;
						}
						else if (strstr(name, "LeftFoot"))
						{
							leftFoot = parentBone;
							//bipedConfig.m_leftFootNode = parentBone;
						}
					}
					else
					{
						dAssert(0);
						//dMatrix effectorMatrix(childEntity->GetCurrentMatrix() * parentBone->GetBody()->GetMatrix());
						//ndCharacterEffectorNode* const effectorNode = CreateInverseDynamicEffector(effectorMatrix, parentBone);
						//effectorNode->SetName(name);
						//if (strcmp(effectorNode->GetJoint()->SubClassName(), "ndJointTwoBodyIK") == 0)
						//{
						//	//ndJointTwoBodyIK* const effectorJoint = (ndJointTwoBodyIK*)effectorNode->GetJoint();
						//	//effectorJoint->SetLinearSpringDamperRegularizer(definition.m_jointData.m_spring, definition.m_jointData.m_damper, definition.m_jointData.m_regularizer);
						//}
						//else
						//{
						//	dAssert(0);
						//}
						//
						//if (strstr(name, "right"))
						//{
						//	bipedConfig.m_rightFootEffector = effectorNode;
						//}
						//else if (strstr(name, "left"))
						//{
						//	bipedConfig.m_leftFootEffector = effectorNode;
						//}
					}
				
					break;
				}
			}
		
			for (ndDemoEntity* child = childEntity->GetChild(); child; child = child->GetSibling())
			{
				childEntities[stack] = child;
				parentBones[stack] = parentBone;
				stack++;
			}
		}
		
		SetModelMass(100.0f, bodyCount, bodyArray, massWeight);

		if (1)
		{
			ndBodyKinematic* testBody = m_rootNode->Find("mixamorig:Hips")->GetBody();
			//ndBodyKinematic* testBody = m_rootNode->Find("mixamorig:Spine1")->GetBody();
			ndJointFix6dof* const joint = new ndJointFix6dof(testBody->GetMatrix(), testBody, world->GetSentinelBody());
			world->AddJoint(joint);
			AddAttachment(joint);
		}

		// initialize a biped controller and set to the model
		//m_bipedController.Init(this, bipedConfig);
		//SetController(&m_bipedController);

		if (righFoot)
		{
			CreateKinematicChain(coronalFrame, righFoot);
		}

		if (leftFoot)
		{
			CreateKinematicChain(coronalFrame, leftFoot);
		}

		SetAnimation(scene, entity);
	}

	~ndActiveRagdollModel()
	{
		if (m_animBlendTree)
		{
			delete m_animBlendTree;
		}
	}

	void SetAnimation(ndDemoEntityManager* const scene, const ndDemoEntity* const entity)
	{
		ndAnimationSequence* const sequence = scene->GetAnimationSequence("whiteMan_idle.fbx");
		const dList<ndAnimationKeyFramesTrack>& tracks = sequence->m_tracks;
		for (dList<ndAnimationKeyFramesTrack>::dNode* node = tracks.GetFirst(); node; node = node->GetNext())
		{
			ndAnimationKeyFramesTrack& track = node->GetInfo();
			const char* const name = track.GetName().GetStr();
			ndCharacterNode* const skelNode = m_rootNode->Find(name);
			const ndDemoEntity* const ent = entity->Find(name);
			dAssert(ent);
			ndAnimKeyframe keyFrame(ent->GetCurrentTransform());
			keyFrame.m_userData = skelNode;
			m_output.PushBack(keyFrame);
		}
		SetPose();

		//ndAnimationSequence* const walkSequence = scene->GetAnimationSequence("whiteMan_idle.fbx");
		ndAnimationSequence* const walkSequence = scene->GetAnimationSequence("whiteman_walk.fbx");
		ndAnimationSequencePlayer* const walk = new ndAnimationSequencePlayer(walkSequence);
		m_animBlendTree = walk;

	}

	void SetModelMass(dFloat32 mass, int bodyCount, ndBodyDynamic** const bodyArray, const dFloat32* const massWeight) const
	{
		dFloat32 volume = 0.0f;
		for (dInt32 i = 0; i < bodyCount; i++) 
		{
			volume += bodyArray[i]->GetCollisionShape().GetVolume() * massWeight[i];
		}
		dFloat32 density = mass / volume;

		for (dInt32 i = 0; i < bodyCount; i++) 
		{
			ndBodyDynamic* const body = bodyArray[i];
			dFloat32 scale = density * body->GetCollisionShape().GetVolume() * massWeight[i];
			dVector inertia(body->GetMassMatrix().Scale (scale));
			body->SetMassMatrix(inertia);
		}
	}
	
	ndBodyDynamic* CreateBodyPart(ndDemoEntityManager* const scene, ndDemoEntity* const entityPart, ndBodyDynamic* const parentBone)
	{
		ndShapeInstance* const shape = entityPart->CreateCollisionFromchildren();
		dAssert(shape);

		// create the rigid body that will make this body
		dMatrix matrix(entityPart->CalculateGlobalMatrix());

		ndBodyDynamic* const body = new ndBodyDynamic();
		body->SetMatrix(matrix);
		body->SetCollisionShape(*shape);
		body->SetMassMatrix(1.0f, *shape);
		body->SetNotifyCallback(new ndActiveRagdollEntityNotify(scene, entityPart, parentBone));

		delete shape;
		return body;
	}

	ndCharacterNode* ConnectBodyParts(ndBodyDynamic* const childBody, ndCharacterNode* const parentNode, const dActiveJointDefinition& definition)
	{
		dMatrix matrix(childBody->GetMatrix());
		dActiveJointDefinition::dFrameMatrix frameAngle(definition.m_frameBasics);
		dMatrix pinAndPivotInGlobalSpace(dPitchMatrix(frameAngle.m_pitch * dDegreeToRad) * dYawMatrix(frameAngle.m_yaw * dDegreeToRad) * dRollMatrix(frameAngle.m_roll * dDegreeToRad) * matrix);

		if (definition.m_limbType == dActiveJointDefinition::forwardKinematic)
		{
			ndCharacterForwardDynamicNode* const jointNode = CreateForwardDynamicLimb(pinAndPivotInGlobalSpace, childBody, parentNode);

			dActiveJointDefinition::dJointLimit jointLimits(definition.m_jointLimits);
			ndJointPdActuator* const joint = (ndJointPdActuator*)jointNode->GetJoint();

			joint->SetConeLimit(jointLimits.m_coneAngle * dDegreeToRad);
			joint->SetTwistLimits(jointLimits.m_minTwistAngle * dDegreeToRad, jointLimits.m_maxTwistAngle * dDegreeToRad);
			joint->SetConeAngleSpringDamperRegularizer(definition.m_coneSpringData.m_spring, definition.m_coneSpringData.m_damper, definition.m_coneSpringData.m_regularizer);
			joint->SetTwistAngleSpringDamperRegularizer(definition.m_twistSpringData.m_spring, definition.m_twistSpringData.m_damper, definition.m_twistSpringData.m_regularizer);

			return jointNode;
		}
		else
		{
			ndCharacterInverseDynamicNode* const jointNode = CreateInverseDynamicLimb(pinAndPivotInGlobalSpace, childBody, parentNode);

			dActiveJointDefinition::dJointLimit jointLimits(definition.m_jointLimits);
			ndJointBallAndSocket* const joint = (ndJointBallAndSocket*)jointNode->GetJoint();

			//dTrace (("do not forget to delete this debug\n"))
			//joint->SetSolverModel(m_jointkinematicCloseLoop);

			joint->SetConeLimit(jointLimits.m_coneAngle * dDegreeToRad);
			joint->SetTwistLimits(jointLimits.m_minTwistAngle * dDegreeToRad, jointLimits.m_maxTwistAngle * dDegreeToRad);
			joint->SetConeFriction(dFloat32(0.0f), dFloat32(0.0f));
			joint->SetTwistFriction(dFloat32(0.0f), dFloat32(0.0f));

			return jointNode;
		}
	}

	void Update(ndWorld* const world, dFloat32 timestep) 
	{
		//m_animBlendTree->Evaluate(m_output, timestep);
		m_animBlendTree->Evaluate(m_output, timestep * 0.05f);
		//m_animBlendTree->Evaluate(m_output, 0.0f);
		for (dInt32 i = 0; i < m_output.GetCount(); i++)
		{
			const ndAnimKeyframe& keyFrame = m_output[i];
			ndCharacterNode* const skelNode = (ndCharacterNode*)keyFrame.m_userData;
			if (skelNode)
			{
				skelNode->SetLocalPose(dMatrix(keyFrame.m_rotation, keyFrame.m_posit));
			}
		}
		SetPose();

		ndCharacter::Update(world, timestep);
	}

	void PostUpdate(ndWorld* const world, dFloat32 timestep)
	{
		ndCharacter::PostUpdate(world, timestep);
	}

	void PostTransformUpdate(ndWorld* const world, dFloat32 timestep)
	{
		ndCharacter::PostTransformUpdate(world, timestep);
	}

	ndAnimationPose m_output;
	ndAnimationBlendTreeNode* m_animBlendTree;
	//ndCharacterBipedPoseController m_bipedController;
};

static void TestPlayerCapsuleInteaction(ndDemoEntityManager* const scene, const dMatrix& location)
{
	dMatrix localAxis(dGetIdentityMatrix());
	localAxis[0] = dVector(0.0, 1.0f, 0.0f, 0.0f);
	localAxis[1] = dVector(1.0, 0.0f, 0.0f, 0.0f);
	localAxis[2] = localAxis[0].CrossProduct(localAxis[1]);

	dFloat32 height = 1.9f;
	dFloat32 radio = 0.5f;
	dFloat32 mass = 100.0f;
	ndDemoEntity* const entity = scene->LoadFbxMesh("whiteMan.fbx");
	ndBasicPlayerCapsule* const player = new ndBasicPlayerCapsule(scene, entity, localAxis, location, mass, radio, height, height / 4.0f);
	player->GetNotifyCallback()->SetGravity(dVector::m_zero);
	dMatrix matrix(player->GetMatrix());
	matrix.m_posit.m_y += 0.5f;
	player->SetMatrix(matrix);
	delete entity;
}

void ndActiveRagdoll (ndDemoEntityManager* const scene)
{
	// build a floor
	BuildFloorBox(scene, dGetIdentityMatrix());

	dVector origin1(0.0f, 0.0f, 0.0f, 0.0f);
	fbxDemoEntity* const ragdollMesh = scene->LoadFbxMesh("whiteMan.fbx");

	dMatrix matrix(dGetIdentityMatrix());
	matrix.m_posit.m_y = 0.5f;
	dMatrix playerMatrix(matrix);
	ndActiveRagdollModel* const ragdoll = new ndActiveRagdollModel(scene, ragdollMesh, matrix);
	scene->SetSelectedModel(ragdoll);
	scene->GetWorld()->AddModel(ragdoll);

	matrix.m_posit.m_x += 1.4f;
	TestPlayerCapsuleInteaction(scene, matrix);

	matrix.m_posit.m_x += 2.0f;
	matrix.m_posit.m_y += 2.0f;
	//ndBodyKinematic* const reckingBall = AddSphere(scene, matrix.m_posit, 25.0f, 0.25f);
	//reckingBall->SetVelocity(dVector(-5.0f, 0.0f, 0.0f, 0.0f));

	matrix.m_posit.m_x += 2.0f;
	matrix.m_posit.m_z -= 2.0f;
	//scene->GetWorld()->AddModel(new ndActiveRagdollModel(scene, ragdollMesh, matrix));

	matrix.m_posit.m_z = 2.0f;
	//scene->GetWorld()->AddModel(new ndActiveRagdollModel(scene, ragdollMesh, matrix));
	delete ragdollMesh;

	origin1.m_x += 20.0f;
	//AddCapsulesStacks(scene, origin1, 10.0f, 0.25f, 0.25f, 0.5f, 10, 10, 7);

	dFloat32 angle = dFloat32(90.0f * dDegreeToRad);
	playerMatrix = dYawMatrix(angle) * playerMatrix;
	dVector origin(playerMatrix.m_posit + playerMatrix.m_front.Scale (-5.0f));
	origin.m_y += 1.0f;
	origin.m_z -= 2.0f;
	scene->SetCameraMatrix(playerMatrix, origin);

	//ndLoadSave loadScene;
	//loadScene.SaveModel("xxxxxx", ragdoll);
}
