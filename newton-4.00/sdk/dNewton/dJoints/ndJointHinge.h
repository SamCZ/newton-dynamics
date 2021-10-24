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

#ifndef __D_JOINT_HINGE_H__
#define __D_JOINT_HINGE_H__

#include "ndNewtonStdafx.h"
#include "ndJointBilateralConstraint.h"

#define D_HINGE_PENETRATION_RECOVERY_SPEED	dFloat32 (0.1f) 
#define D_HINGE_PENETRATION_LIMIT			dFloat32 (10.0f * dDegreeToRad) 

class ndJointHinge: public ndJointBilateralConstraint
{
	public:
	D_CLASS_REFLECTION(ndJointHinge);
	D_NEWTON_API ndJointHinge(const dLoadSaveBase::dLoadDescriptor& desc);
	D_NEWTON_API ndJointHinge(const dMatrix& pinAndPivotFrame, ndBodyKinematic* const child, ndBodyKinematic* const parent);
	D_NEWTON_API ndJointHinge(const dMatrix& pinAndPivotInChild, const dMatrix& pinAndPivotInParent, ndBodyKinematic* const child, ndBodyKinematic* const parent);
	D_NEWTON_API virtual ~ndJointHinge();

	D_NEWTON_API dFloat32 GetAngle() const;
	D_NEWTON_API dFloat32 GetOmega() const;
	D_NEWTON_API dFloat32 GetFriction() const;

	D_NEWTON_API void SetFriction(dFloat32 frictionTorque);
	D_NEWTON_API void EnableLimits(bool state, dFloat32 minLimit, dFloat32 maxLimit);
	D_NEWTON_API void SetAsSpringDamper(bool state, dFloat32 regularizer, dFloat32 spring, dFloat32 damper);

	private:
	void SubmitConstraintLimits(ndConstraintDescritor& desc, const dMatrix& matrix0, const dMatrix& matrix1);
	void SubmitConstraintLimitSpringDamper(ndConstraintDescritor& desc, const dMatrix& matrix0, const dMatrix& matrix1);
	
	protected:
	D_NEWTON_API void JacobianDerivative(ndConstraintDescritor& desc);
	D_NEWTON_API void Save(const dLoadSaveBase::dSaveDescriptor& desc) const;

	dFloat32 m_angle;
	dFloat32 m_omega;
	dFloat32 m_springK;
	dFloat32 m_damperC;
	dFloat32 m_minLimit;
	dFloat32 m_maxLimit;
	dFloat32 m_friction;
	dFloat32 m_springDamperRegularizer;

	bool m_hasLimits;
	bool m_isSpringDamper;
};

#endif 

