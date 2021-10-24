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
#include "ndJointWheel.h"
#include "ndMultiBodyVehicle.h"
#include "ndMultiBodyVehicleMotor.h"
#include "ndMultiBodyVehicleGearBox.h"

D_CLASS_REFLECTION_IMPLEMENT_LOADER(ndMultiBodyVehicleGearBox)

ndMultiBodyVehicleGearBox::ndMultiBodyVehicleGearBox(ndBodyKinematic* const motor, ndBodyKinematic* const differential, ndMultiBodyVehicle* const chassis)
	:ndJointGear(dFloat32 (1.0f), motor->GetMatrix().m_front, differential,	motor->GetMatrix().m_front, motor)
	,m_chassis(chassis)
	,m_clutchTorque(dFloat32 (1.0e5f))
	,m_driveTrainResistanceTorque(dFloat32(1000.0f))
{
	SetRatio(dFloat32(0.0f));
	SetSolverModel(m_jointkinematicCloseLoop);
}

ndMultiBodyVehicleGearBox::ndMultiBodyVehicleGearBox(const dLoadSaveBase::dLoadDescriptor& desc)
	:ndJointGear(dLoadSaveBase::dLoadDescriptor(desc))
	,m_chassis(nullptr)
	,m_clutchTorque(dFloat32(1.0e5f))
	,m_driveTrainResistanceTorque(dFloat32(1000.0f))
{
	const nd::TiXmlNode* const xmlNode = desc.m_rootNode;

	m_clutchTorque = xmlGetFloat(xmlNode, "clutchTorque");
	m_driveTrainResistanceTorque = xmlGetFloat(xmlNode, "driveTrainResistanceTorque");
}

void ndMultiBodyVehicleGearBox::SetClutchTorque(dFloat32 torqueInNewtonMeters)
{
	m_clutchTorque = dAbs(torqueInNewtonMeters);
}

void ndMultiBodyVehicleGearBox::SetInternalLosesTorque(dFloat32 torqueInNewtonMeters)
{
	m_driveTrainResistanceTorque = dAbs(torqueInNewtonMeters);
}

void ndMultiBodyVehicleGearBox::JacobianDerivative(ndConstraintDescritor& desc)
{
	if (dAbs(m_gearRatio) > dFloat32(1.0e-2f))
	{
		dMatrix matrix0;
		dMatrix matrix1;
		
		// calculate the position of the pivot point and the Jacobian direction vectors, in global space. 
		CalculateGlobalMatrix(matrix0, matrix1);
		
		AddAngularRowJacobian(desc, matrix0.m_front, dFloat32(0.0f));
		
		ndJacobian& jacobian0 = desc.m_jacobian[desc.m_rowsCount - 1].m_jacobianM0;
		ndJacobian& jacobian1 = desc.m_jacobian[desc.m_rowsCount - 1].m_jacobianM1;

		dFloat32 gearRatio = dFloat32(1.0f) / m_gearRatio;
		
		jacobian0.m_angular = matrix0.m_front;
		jacobian1.m_angular = matrix1.m_front.Scale(gearRatio);
		
		const dVector& omega0 = m_body0->GetOmega();
		const dVector& omega1 = m_body1->GetOmega();
		
		dAssert(m_chassis->m_motor);
		ndMultiBodyVehicleMotor* const rotor = m_chassis->m_motor;
		dFloat32 idleOmega = rotor->m_idleOmega * gearRatio * dFloat32(0.95f);

		dFloat32 w0 = omega0.DotProduct(jacobian0.m_angular).GetScalar();
		dFloat32 w1 = omega1.DotProduct(jacobian1.m_angular).GetScalar() + idleOmega;
		w1 = (gearRatio > dFloat32(0.0f)) ? dMin(w1, dFloat32(0.0f)) : dMax(w1, dFloat32(0.0f));
		
		const dFloat32 w = (w0 + w1) * dFloat32(0.5f);
		SetMotorAcceleration(desc, -w * desc.m_invTimestep);

		if (m_gearRatio > dFloat32 (0.0f))
		{
			SetHighFriction(desc, m_clutchTorque);
			SetLowerFriction(desc, -m_driveTrainResistanceTorque);
		}
		else
		{
			SetHighFriction(desc, m_driveTrainResistanceTorque);
			SetLowerFriction(desc, -m_clutchTorque);
		}
	}
}

void ndMultiBodyVehicleGearBox::Save(const dLoadSaveBase::dSaveDescriptor& desc) const
{
	nd::TiXmlElement* const childNode = new nd::TiXmlElement(ClassName());
	desc.m_rootNode->LinkEndChild(childNode);
	childNode->SetAttribute("hashId", desc.m_nodeNodeHash);
	ndJointGear::Save(dLoadSaveBase::dSaveDescriptor(desc, childNode));
	
	xmlSaveParam(childNode, "clutchTorque", m_clutchTorque);
	xmlSaveParam(childNode, "driveTrainResistanceTorque", m_driveTrainResistanceTorque);
}
