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
#include "ndArchimedesBuoyancyVolume.h"

D_CLASS_REFLECTION_IMPLEMENT_LOADER(ndArchimedesBuoyancyVolume);

ndArchimedesBuoyancyVolume::ndArchimedesBuoyancyVolume()
	:ndBodyTriggerVolume()
	,m_plane(dVector::m_zero)
	,m_density(1.0f)
	,m_hasPlane(0)
{
}

ndArchimedesBuoyancyVolume::ndArchimedesBuoyancyVolume(const dLoadSaveBase::dLoadDescriptor& desc)
	:ndBodyTriggerVolume(dLoadSaveBase::dLoadDescriptor(desc))
{
	const nd::TiXmlNode* const xmlNode = desc.m_rootNode;
	m_plane = xmlGetVector3(xmlNode, "planeNormal");
	m_plane.m_w = xmlGetFloat(xmlNode, "planeDist");
	m_density = xmlGetFloat(xmlNode, "density");
	m_hasPlane = xmlGetInt(xmlNode, "hasPlane") ? true : false;
}

void ndArchimedesBuoyancyVolume::CalculatePlane(ndBodyKinematic* const body)
{
	class ndCastTriggerPlane : public ndRayCastClosestHitCallback
	{
		public:
		ndCastTriggerPlane()
			:ndRayCastClosestHitCallback()
		{
		}

		dUnsigned32 OnRayPrecastAction(const ndBody* const body, const ndShapeInstance* const)
		{
			return ((ndBody*)body)->GetAsBodyTriggerVolume() ? 1 : 0;
		}
	};

	dMatrix matrix(body->GetMatrix());

	dVector p0(matrix.m_posit);
	dVector p1(matrix.m_posit);

	p0.m_y += 30.0f;
	p1.m_y -= 30.0f;

	ndCastTriggerPlane rayCaster;
	m_hasPlane = body->GetScene()->RayCast(rayCaster, p0, p1);
	if (m_hasPlane)
	{ 
		dFloat32 dist = -rayCaster.m_contact.m_normal.DotProduct(rayCaster.m_contact.m_point).GetScalar();
		m_plane = dPlane(rayCaster.m_contact.m_normal, dist);
	}
}

void ndArchimedesBuoyancyVolume::OnTriggerEnter(ndBodyKinematic* const body, dFloat32)
{
	CalculatePlane(body);
}
	
void ndArchimedesBuoyancyVolume::OnTrigger(ndBodyKinematic* const kinBody, dFloat32)
{
	ndBodyDynamic* const body = kinBody->GetAsBodyDynamic();
	if (!m_hasPlane)
	{
		CalculatePlane(body);
	}

	if (body && m_hasPlane && (body->GetInvMass() != 0.0f))
	{
		dVector mass (body->GetMassMatrix());
		dVector centerOfPreasure(dVector::m_zero);
		dMatrix matrix (body->GetMatrix());
		ndShapeInstance& collision = body->GetCollisionShape();

		dFloat32 volume = collision.CalculateBuoyancyCenterOfPresure (centerOfPreasure, matrix, m_plane);
		if (volume > 0.0f)
		{
			// if some part of the shape si under water, calculate the buoyancy force base on 
			// Archimedes's buoyancy principle, which is the buoyancy force is equal to the 
			// weight of the fluid displaced by the volume under water. 
			//dVector cog(dVector::m_zero);
			const dFloat32 viscousDrag = 0.99f;

			ndShapeMaterial material(collision.GetMaterial());
			body->GetCollisionShape().SetMaterial(material);
			dFloat32 density = material.m_userParam[0].m_floatData;
			dFloat32 desplacedVolume = density * collision.GetVolume();
				
			dFloat32 displacedMass = mass.m_w * volume / desplacedVolume;
			dVector cog(body->GetCentreOfMass());
			centerOfPreasure -= matrix.TransformVector(cog);
				
			// now with the mass and center of mass of the volume under water, calculate buoyancy force and torque
			dVector force(dFloat32(0.0f), dFloat32(-DEMO_GRAVITY * displacedMass), dFloat32(0.0f), dFloat32(0.0f));
			dVector torque(centerOfPreasure.CrossProduct(force));
				
			body->SetForce(body->GetForce() + force);
			body->SetTorque(body->GetTorque() + torque);
				
			// apply a fake viscous drag to damp the under water motion 
			dVector omega(body->GetOmega());
			dVector veloc(body->GetVelocity());
			omega = omega.Scale(viscousDrag);
			veloc = veloc.Scale(viscousDrag);
			body->SetOmega(omega);
			body->SetVelocity(veloc);
				
			//// test delete bodies inside trigger
			//collisionMaterial.m_userParam[1].m_float += timestep;
			//NewtonCollisionSetMaterial(collision, &collisionMaterial);
			//if (collisionMaterial.m_userParam[1].m_float >= 30.0f) {
			//	// delete body after 2 minutes inside the pool
			//	NewtonDestroyBody(visitor);
			//}
		}
	}
}

void ndArchimedesBuoyancyVolume::OnTriggerExit(ndBodyKinematic* const, dFloat32)
{
	//dTrace(("exit trigger body: %d\n", body->GetId()));
}

void ndArchimedesBuoyancyVolume::Save(const dLoadSaveBase::dSaveDescriptor& desc) const
{
	nd::TiXmlElement* const childNode = new nd::TiXmlElement(ClassName());
	desc.m_rootNode->LinkEndChild(childNode);
	childNode->SetAttribute("hashId", desc.m_nodeNodeHash);
	ndBodyTriggerVolume::Save(dLoadSaveBase::dSaveDescriptor(desc, childNode));

	xmlSaveParam(childNode, "planeNormal", m_plane);
	xmlSaveParam(childNode, "planeDist", m_plane.m_w);
	xmlSaveParam(childNode, "density", m_density);
	xmlSaveParam(childNode, "hasPlane", m_hasPlane ? 1 : 0);
}

