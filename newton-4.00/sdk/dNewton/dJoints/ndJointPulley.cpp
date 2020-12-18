/* Copyright (c) <2003-2019> <Newton Game Dynamics>
* 
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
* 
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely
*/

#include "dCoreStdafx.h"
#include "ndNewtonStdafx.h"
#include "ndJointPulley.h"

ndJointPulley::ndJointPulley(dFloat32 gearRatio,
	const dVector& body0Pin, ndBodyKinematic* const body0,
	const dVector& body1Pin, ndBodyKinematic* const body1)
	:ndJointBilateralConstraint(1, body0, body0, dGetIdentityMatrix())
{
	m_gearRatio = gearRatio;

	// calculate the two local matrix of the pivot point
	dMatrix dommyMatrix;

	// calculate the local matrix for body body0
	dMatrix pinAndPivot0(body0Pin);
	CalculateLocalMatrix(pinAndPivot0, m_localMatrix0, dommyMatrix);
	m_localMatrix0.m_posit = dVector::m_wOne;

	// calculate the local matrix for body body1  
	dMatrix pinAndPivot1(body1Pin);
	CalculateLocalMatrix(pinAndPivot1, dommyMatrix, m_localMatrix1);
	m_localMatrix1.m_posit = dVector::m_wOne;

	// set as kinematic loop
	SetSolverModel(1);
}

ndJointPulley::~ndJointPulley()
{
}

void ndJointPulley::JacobianDerivative(ndConstraintDescritor& desc)
{
	dMatrix matrix0;
	dMatrix matrix1;

	// calculate the position of the pivot point and the Jacobian direction vectors, in global space. 
	CalculateGlobalMatrix(matrix0, matrix1);

	AddAngularRowJacobian(desc, matrix0.m_front, dFloat32(0.0f));

	ndJacobian& jacobian0 = desc.m_jacobian[desc.m_rowsCount - 1].m_jacobianM0;
	ndJacobian& jacobian1 = desc.m_jacobian[desc.m_rowsCount - 1].m_jacobianM1;

	jacobian0.m_linear = matrix0.m_front;
	jacobian1.m_linear = matrix1.m_front.Scale(m_gearRatio);

	const dVector& veloc0 = m_body0->GetVelocity();
	const dVector& veloc1 = m_body1->GetVelocity();

	const dVector relVeloc(veloc0 * jacobian0.m_linear + veloc1 * jacobian1.m_linear);
	const dFloat32 w = relVeloc.AddHorizontal().GetScalar() * dFloat32(0.5f);
	SetMotorAcceleration(desc, -w * desc.m_invTimestep);
}


