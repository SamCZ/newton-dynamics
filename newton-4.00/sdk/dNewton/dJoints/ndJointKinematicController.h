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

// ndJointKinematicController.h: interface for the ndJointKinematicController class.
//
//////////////////////////////////////////////////////////////////////

#ifndef __D_JOINT_KINEMATIC_CONTROLLER_H__
#define __D_JOINT_KINEMATIC_CONTROLLER_H__


#include "ndNewtonStdafx.h"
#include "ndJointBilateralConstraint.h"

class ndJointKinematicController: public ndJointBilateralConstraint
{
	public:
	enum ndControlModes
	{	
		m_linear,
		m_full6dof,
		m_linearAndTwist,
		m_linearAndCone,
		m_linearPlusAngularFriction, // this is pick mode from screen
	};

	D_CLASS_REFLECTION(ndJointKinematicController);
	D_NEWTON_API ndJointKinematicController(const dLoadSaveBase::dLoadDescriptor& desc);
	D_NEWTON_API ndJointKinematicController(ndBodyKinematic* const referenceBody, ndBodyKinematic* const body, const dVector& attachmentPointInGlobalSpace);
	D_NEWTON_API ndJointKinematicController(ndBodyKinematic* const referenceBody, ndBodyKinematic* const body, const dMatrix& attachmentMatrixInGlobalSpace);
	D_NEWTON_API virtual ~ndJointKinematicController();

	virtual bool IsBilateral() const;
	void SetControlMode(ndControlModes mode);
	void SetMaxSpeed(dFloat32 speedInMetersPerSeconds);
	void SetMaxOmega(dFloat32 speedInRadiansPerSeconds);
	void SetMaxLinearFriction(dFloat32 force);
	void SetMaxAngularFriction(dFloat32 torque);
	void SetAngularViscuosFrictionCoefficient(dFloat32 coefficient);

	dMatrix GetTargetMatrix() const;
	void SetTargetPosit(const dVector& posit);
	void SetTargetMatrix(const dMatrix& matrix);
	void SetTargetRotation(const dQuaternion& rotation);

	protected:
	void Init(const dMatrix& matrix);

	D_NEWTON_API void CheckSleep() const;
	D_NEWTON_API void JacobianDerivative(ndConstraintDescritor& desc);
	D_NEWTON_API void Save(const dLoadSaveBase::dSaveDescriptor& desc) const;

	dFloat32 m_maxSpeed;
	dFloat32 m_maxOmega;
	dFloat32 m_maxLinearFriction;
	dFloat32 m_maxAngularFriction;

	dFloat32 m_angularFrictionCoefficient;
	ndControlModes m_controlMode;
	bool m_autoSleepState;
};

inline void ndJointKinematicController::SetControlMode(ndControlModes mode)
{
	m_controlMode = mode;
}

inline void ndJointKinematicController::SetMaxSpeed(dFloat32 speedInMetersPerSeconds)
{
	m_maxSpeed = dAbs(speedInMetersPerSeconds);
}

inline void ndJointKinematicController::SetMaxLinearFriction(dFloat32 frictionForce)
{
	m_maxLinearFriction = dAbs(frictionForce);
}

inline void ndJointKinematicController::SetMaxAngularFriction(dFloat32 frictionTorque)
{
	m_maxAngularFriction = dAbs(frictionTorque);
}

inline void ndJointKinematicController::SetMaxOmega(dFloat32 speedInRadiansPerSeconds)
{
	m_maxOmega = dAbs(speedInRadiansPerSeconds);
}

inline void ndJointKinematicController::SetAngularViscuosFrictionCoefficient(dFloat32 coefficient)
{
	dVector mass (GetBody0()->GetMassMatrix());
	m_angularFrictionCoefficient = dAbs(coefficient) * dMax(mass.m_x, dMax(mass.m_y, mass.m_z));
}

inline void ndJointKinematicController::SetTargetPosit(const dVector& posit)
{
	m_localMatrix1.m_posit = posit;
	dAssert(m_localMatrix1.m_posit.m_w == dFloat32(1.0f));
	CheckSleep();
}

inline void ndJointKinematicController::SetTargetMatrix(const dMatrix& matrix)
{
	m_localMatrix1 = matrix;
	CheckSleep();
}

inline dMatrix ndJointKinematicController::GetTargetMatrix() const
{
	dAssert(0);
	return m_localMatrix0;
}

inline void ndJointKinematicController::SetTargetRotation(const dQuaternion& rotation)
{
	m_localMatrix1 = dMatrix(rotation, m_localMatrix1.m_posit);
	dAssert(m_localMatrix1.m_posit.m_w == dFloat32(1.0f));
	CheckSleep();
}

inline bool ndJointKinematicController::IsBilateral() const
{
	return true;
}

#endif 

