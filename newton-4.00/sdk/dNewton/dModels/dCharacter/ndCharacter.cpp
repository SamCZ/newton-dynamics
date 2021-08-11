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
#include "ndCharacter.h"
#include "ndBodyDynamic.h"
#include "ndCharacterLimbNode.h"
#include "ndCharacterRootNode.h"
#include "ndCharacterEffectorNode.h"
#include "ndCharacterPoseController.h"
#include "ndCharacterForwardDynamicNode.h"
#include "ndCharacterInverseDynamicNode.h"
#include "ndCharacterBipedPoseController.h"

ndCharacter::ndCharacter()
	:ndModel()
	,m_rootNode(nullptr)
	,m_controller(nullptr)
{
}

ndCharacter::ndCharacter(const nd::TiXmlNode* const xmlNode)
	:ndModel(xmlNode)
{
	dAssert(0);
}

ndCharacter::~ndCharacter()
{
	if (m_rootNode)
	{
		delete m_rootNode;
	}
}

ndCharacterRootNode* ndCharacter::CreateRoot(ndBodyDynamic* const body)
{
	m_rootNode = new ndCharacterRootNode(this, body);
	return m_rootNode;
}

ndCharacterForwardDynamicNode* ndCharacter::CreateForwardDynamicLimb(const dMatrix& matrixInGlobalScape, ndBodyDynamic* const body, ndCharacterLimbNode* const parent)
{
	ndCharacterForwardDynamicNode* const limb = new ndCharacterForwardDynamicNode(matrixInGlobalScape, body, parent);
	return limb;
}

ndCharacterInverseDynamicNode* ndCharacter::CreateInverseDynamicLimb(const dMatrix& matrixInGlobalScape, ndBodyDynamic* const body, ndCharacterLimbNode* const parent)
{
	ndCharacterInverseDynamicNode* const limb = new ndCharacterInverseDynamicNode(matrixInGlobalScape, body, parent);
	return limb;
}

ndCharacterEffectorNode* ndCharacter::CreateInverseDynamicEffector(const dMatrix& matrixInGlobalScape, ndCharacterLimbNode* const child, ndCharacterLimbNode* const referenceNode)
{
	ndCharacterEffectorNode* const effector = new ndCharacterEffectorNode(matrixInGlobalScape, child, referenceNode);
	return effector;
}

ndCharacter::ndCentreOfMassState ndCharacter::CalculateCentreOfMassState() const
{
	dInt32 stack = 1;
	ndCharacterLimbNode* nodePool[32];
	
	nodePool[0] = m_rootNode;

	//dMatrix inertia(dGetZeroMatrix());
	dVector com(dVector::m_zero);
	dVector veloc(dVector::m_zero);
	//dVector omega(dVector::m_zero);
	dFloat32 mass = dFloat32(0.0f);

	while (stack)
	{
		stack--;
		ndCharacterLimbNode* const node = nodePool[stack];
		ndBodyDynamic* const body = node->GetBody();
		if (body)
		{
			dFloat32 partMass = body->GetMassMatrix().m_w;
			mass += partMass;
			dMatrix bodyMatrix(body->GetMatrix());
			com += bodyMatrix.TransformVector(body->GetCentreOfMass()).Scale (partMass);
			veloc += body->GetVelocity().Scale(partMass);

			//dMatrix inertiaPart(body->CalculateInertiaMatrix());
			//inertia.m_front += inertiaPart.m_front;
			//inertia.m_up += inertiaPart.m_up;
			//inertia.m_right += inertiaPart.m_right;
			//omega += inertiaPart.RotateVector(body->GetOmega());
		}

		for (ndCharacterLimbNode* child = node->GetChild(); child; child = child->GetSibling())
		{
			nodePool[stack] = child;
			stack++;
		}
	}
	//inertia.m_posit.m_w = dFloat32(1.0f);
	dVector invMass (dFloat32(1.0f) / mass);
	com = com * invMass;
	veloc = veloc * invMass;
	//omega = inertia.Inverse4x4().RotateVector(omega);
	com.m_w = dFloat32(1.0f);
	//omega.m_w = dFloat32(0.0f);
	veloc.m_w = dFloat32(0.0f);

	ndCentreOfMassState state;
	state.m_mass = mass;
	state.m_centerOfMass = com;
	state.m_centerOfMassVeloc = veloc;
	//state.m_centerOfMassOmega = omega;
	return state;
}

void ndCharacter::Debug(ndConstraintDebugCallback& context) const
{
	if (m_controller)
	{
		dFloat32 scale = context.GetScale();
		context.SetScale(scale * 0.25f);
		m_controller->Debug(context);
		context.SetScale(scale);
	}
}

void ndCharacter::UpdateGlobalPose(ndWorld* const world, dFloat32 timestep)
{
	dInt32 stack = 1;
	ndCharacterLimbNode* nodePool[32];
	nodePool[0] = m_rootNode;

	while (stack)
	{
		stack--;
		ndCharacterLimbNode* const node = nodePool[stack];
		node->UpdateGlobalPose(world, timestep);

		for (ndCharacterLimbNode* child = node->GetChild(); child; child = child->GetSibling())
		{
			nodePool[stack] = child;
			stack++;
		}
	}
}

void ndCharacter::CalculateLocalPose(ndWorld* const world, dFloat32 timestep)
{
	dInt32 stack = 1;
	ndCharacterLimbNode* nodePool[32];
	nodePool[0] = m_rootNode;

	while (stack)
	{
		stack--;
		ndCharacterLimbNode* const node = nodePool[stack];
		node->CalculateLocalPose(world, timestep);

		for (ndCharacterLimbNode* child = node->GetChild(); child; child = child->GetSibling())
		{
			nodePool[stack] = child;
			stack++;
		}
	}
}

void ndCharacter::PostUpdate(ndWorld* const, dFloat32)
{
}

void ndCharacter::Update(ndWorld* const world, dFloat32 timestep)
{
	if (m_controller)
	{
		m_controller->Evaluate(world, timestep);
	}
}
