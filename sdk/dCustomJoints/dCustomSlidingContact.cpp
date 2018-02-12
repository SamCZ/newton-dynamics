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



// dCustomSlidingContact.cpp: implementation of the dCustomSlidingContact class.
//
//////////////////////////////////////////////////////////////////////
#include "dCustomJointLibraryStdAfx.h"
#include "dCustomSlidingContact.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IMPLEMENT_CUSTOM_JOINT(dCustomSlidingContact);


dCustomSlidingContact::dCustomSlidingContact(const dMatrix& pinAndPivotFrame, NewtonBody* child, NewtonBody* parent)
	:dCustomSlider(pinAndPivotFrame, child, parent)
	,m_curJointAngle()
	,m_minAngle(-45.0f * dDegreeToRad)
	,m_maxAngle(45.0f * dDegreeToRad)
	,m_angularFriction(0.0f)
	,m_angularOmega(0.0f)
	,m_angularSpring(0.0f)
	,m_angularDamper(0.0f)
	,m_angularSpringDamperRelaxation(0.9f)
{
}

dCustomSlidingContact::dCustomSlidingContact(const dMatrix& pinAndPivotFrameChild, const dMatrix& pinAndPivotFrameParent, NewtonBody* const child, NewtonBody* const parent)
	:dCustomSlider(pinAndPivotFrameChild, pinAndPivotFrameParent, child, parent)
	,m_curJointAngle()
	,m_minAngle(-45.0f * dDegreeToRad)
	,m_maxAngle(45.0f * dDegreeToRad)
	,m_angularFriction(0.0f)
	,m_angularOmega(0.0f)
	,m_angularSpring(0.0f)
	,m_angularDamper(0.0f)
	,m_angularSpringDamperRelaxation(0.9f)
{
}

dCustomSlidingContact::~dCustomSlidingContact()
{
}

void dCustomSlidingContact::Deserialize(NewtonDeserializeCallback callback, void* const userData)
{
	callback(userData, &m_curJointAngle, sizeof(dAngularIntegration));
	callback(userData, &m_minAngle, sizeof(dFloat));
	callback(userData, &m_maxAngle, sizeof(dFloat));
	callback(userData, &m_angularFriction, sizeof(dFloat));
	callback(userData, &m_angularOmega, sizeof(dFloat));
	callback(userData, &m_angularSpring, sizeof(dFloat));
	callback(userData, &m_angularDamper, sizeof(dFloat));
	callback(userData, &m_angularSpringDamperRelaxation, sizeof(dFloat));
}

void dCustomSlidingContact::Serialize(NewtonSerializeCallback callback, void* const userData) const
{
	dCustomJoint::Serialize(callback, userData);

	callback(userData, &m_curJointAngle, sizeof(dAngularIntegration));
	callback(userData, &m_minAngle, sizeof(dFloat));
	callback(userData, &m_maxAngle, sizeof(dFloat));
	callback(userData, &m_angularFriction, sizeof(dFloat));
	callback(userData, &m_angularOmega, sizeof(dFloat));
	callback(userData, &m_angularSpring, sizeof(dFloat));
	callback(userData, &m_angularDamper, sizeof(dFloat));
	callback(userData, &m_angularSpringDamperRelaxation, sizeof(dFloat));
}

void dCustomSlidingContact::EnableAngularLimits(bool state)
{
	m_options.m_option2 = state;
}

void dCustomSlidingContact::SetAsAngularSpringDamper(bool state, dFloat springDamperRelaxation, dFloat spring, dFloat damper)
{
	m_angularSpring = spring;
	m_angularDamper = damper;
	m_options.m_option3 = state;
	m_angularSpringDamperRelaxation = dClamp(springDamperRelaxation, dFloat(0.0f), dFloat(0.999f));
	
}

void dCustomSlidingContact::SetAngularLimits(dFloat minDist, dFloat maxDist)
{
	m_minAngle = -dAbs(minDist);
	m_maxAngle = dAbs(maxDist);
}

void dCustomSlidingContact::SubmitConstraintSpringDamper(const dMatrix& matrix0, const dMatrix& matrix1, dFloat timestep)
{
	NewtonUserJointAddAngularRow(m_joint, -m_curJointAngle.GetAngle(), &matrix1.m_up[0]);
	NewtonUserJointSetRowSpringDamperAcceleration(m_joint, m_angularSpringDamperRelaxation, m_angularSpring, m_angularDamper);
}

void dCustomSlidingContact::SubmitConstraintLimits(const dMatrix& matrix0, const dMatrix& matrix1, dFloat timestep)
{
	dFloat angle = m_curJointAngle.GetAngle() + m_angularOmega * timestep;
	if (angle < m_minAngle) {
		NewtonUserJointAddAngularRow(m_joint, 0.0f, &matrix1.m_up[0]);
		NewtonUserJointSetRowStiffness(m_joint, m_stiffness);
		NewtonUserJointSetRowMinimumFriction(m_joint, -m_angularFriction);

		const dFloat invtimestep = 1.0f / timestep;
		const dFloat speed = 0.5f * (m_minAngle - m_curJointAngle.GetAngle()) * invtimestep;
		const dFloat stopAccel = NewtonUserJointCalculateRowZeroAccelaration(m_joint) + speed * invtimestep;
		NewtonUserJointSetRowAcceleration(m_joint, stopAccel);
	} else if (angle > m_maxAngle) {
		NewtonUserJointAddAngularRow(m_joint, 0.0f, &matrix1.m_up[0]);
		NewtonUserJointSetRowStiffness(m_joint, 1.0f);
		NewtonUserJointSetRowMaximumFriction(m_joint, m_angularFriction);

		const dFloat invtimestep = 1.0f / timestep;
		const dFloat speed = 0.5f * (m_maxAngle - m_curJointAngle.GetAngle()) * invtimestep;
		const dFloat stopAccel = NewtonUserJointCalculateRowZeroAccelaration(m_joint) + speed * invtimestep;
		NewtonUserJointSetRowAcceleration(m_joint, stopAccel);

	} else if (m_angularFriction != 0.0f) {
		NewtonUserJointAddAngularRow(m_joint, 0, &matrix1.m_up[0]);
		NewtonUserJointSetRowStiffness(m_joint, m_stiffness);
		NewtonUserJointSetRowAcceleration(m_joint, -m_angularOmega / timestep);
		NewtonUserJointSetRowMinimumFriction(m_joint, -m_angularFriction);
		NewtonUserJointSetRowMaximumFriction(m_joint, m_angularFriction);
	}
}

void dCustomSlidingContact::SubmitConstraintLimitSpringDamper(const dMatrix& matrix0, const dMatrix& matrix1, dFloat timestep)
{
	dFloat angle = m_curJointAngle.GetAngle() + m_angularOmega * timestep;
	if (angle < m_minAngle) {
		NewtonUserJointAddAngularRow(m_joint, 0.0f, &matrix1.m_up[0]);
		NewtonUserJointSetRowStiffness(m_joint, m_stiffness);
		NewtonUserJointSetRowMinimumFriction(m_joint, -m_angularFriction);

		const dFloat invtimestep = 1.0f / timestep;
		const dFloat speed = 0.5f * (m_minAngle - m_curJointAngle.GetAngle()) * invtimestep;
		const dFloat springAccel = NewtonCalculateSpringDamperAcceleration(timestep, m_angularSpring, m_curJointAngle.GetAngle(), m_angularDamper, m_angularOmega);
		const dFloat stopAccel = NewtonUserJointCalculateRowZeroAccelaration(m_joint) + speed * invtimestep + springAccel;
		NewtonUserJointSetRowAcceleration(m_joint, stopAccel);

	} else if (angle > m_maxAngle) {
		NewtonUserJointAddAngularRow(m_joint, 0.0f, &matrix1.m_up[0]);
		NewtonUserJointSetRowStiffness(m_joint, m_stiffness);
		NewtonUserJointSetRowMaximumFriction(m_joint, m_angularFriction);

		const dFloat invtimestep = 1.0f / timestep;
		const dFloat speed = 0.5f * (m_maxAngle - m_curJointAngle.GetAngle()) * invtimestep;
		const dFloat springAccel = NewtonCalculateSpringDamperAcceleration(timestep, m_angularSpring, m_curJointAngle.GetAngle(), m_angularDamper, m_angularOmega);
		const dFloat stopAccel = NewtonUserJointCalculateRowZeroAccelaration(m_joint) + speed * invtimestep + springAccel;
		NewtonUserJointSetRowAcceleration(m_joint, stopAccel);

	} else {
		dCustomSlidingContact::SubmitConstraintSpringDamper(matrix0, matrix1, timestep);
	}
}

void dCustomSlidingContact::SubmitAngularRow(const dMatrix& matrix0, const dMatrix& matrix1, dFloat timestep)
{
#if 0
	dFloat sinAngle;
	dFloat cosAngle;

	// construct an orthogonal coordinate system with these two vectors
	dMatrix matrix1_1;
	matrix1_1.m_up = matrix1.m_up;
	matrix1_1.m_right = matrix0.m_front.CrossProduct(matrix1.m_up);
	matrix1_1.m_right = matrix1_1.m_right.Scale(1.0f / dSqrt(matrix1_1.m_right.DotProduct3(matrix1_1.m_right)));
	matrix1_1.m_front = matrix1_1.m_up.CrossProduct(matrix1_1.m_right);
	NewtonUserJointAddAngularRow(m_joint, CalculateAngle(matrix0.m_up, matrix1_1.m_up, matrix1_1.m_front), &matrix1_1.m_front[0]);
	NewtonUserJointAddAngularRow(m_joint, CalculateAngle(matrix0.m_up, matrix1_1.m_up, matrix1_1.m_right), &matrix1_1.m_right[0]);

	// the joint angle can be determined by getting the angle between any two non parallel vectors
	CalculateAngle(matrix1_1.m_front, matrix1.m_front, matrix1.m_up, sinAngle, cosAngle);
	m_curJointAngle.Update(cosAngle, sinAngle);


static int xxxxxx;
dMatrix localMatrix(matrix0 * matrix1.Inverse());
dVector euler0;
dVector euler1;
localMatrix.GetEulerAngles(euler0, euler1, m_pitchRollYaw);
dFloat xxx0 = CalculateAngle(matrix0.m_up, matrix1_1.m_up, matrix1_1.m_front);
dFloat xxx1 = CalculateAngle(matrix0.m_up, matrix1_1.m_up, matrix1_1.m_right);
dTrace(("%d (%f %f) (%f %f)\n", xxxxxx++, xxx0, -euler0[0], xxx1, -euler0[2]));
dFloat a = xxx0+euler0[0];
dFloat b = xxx1+euler0[2];
if ((dAbs (a) > 1.0e-2f) || (dAbs (b) > 1.0e-2f)) {
	xxxxxx*=1;
}

#else 

	dMatrix localMatrix(matrix0 * matrix1.Inverse());
	dVector euler0;
	dVector euler1;
	localMatrix.GetEulerAngles(euler0, euler1, m_pitchRollYaw);

	NewtonUserJointAddAngularRow(m_joint, -euler0[0], &matrix0[0][0]);
	NewtonUserJointSetRowStiffness(m_joint, m_stiffness);

	dVector rollPin (dSin(euler0[1]), dFloat(0.0f), dCos(euler0[1]), dFloat (0.0f));
	rollPin = matrix1.RotateVector (rollPin);
	dMatrix referenceFrame (dYawMatrix(euler0[1]) * matrix1);

	NewtonUserJointAddAngularRow(m_joint, -euler0[2], &rollPin[0]);
	NewtonUserJointSetRowStiffness(m_joint, m_stiffness);

	// the joint angle can be determined by getting the angle between any two non parallel vectors
	m_curJointAngle.Update(euler0.m_y);

/*
static int xxxxxx;
dMatrix matrix1_1;
matrix1_1.m_up = matrix1.m_up;
matrix1_1.m_right = matrix0.m_front.CrossProduct(matrix1.m_up);
matrix1_1.m_right = matrix1_1.m_right.Scale(1.0f / dSqrt(matrix1_1.m_right.DotProduct3(matrix1_1.m_right)));
matrix1_1.m_front = matrix1_1.m_up.CrossProduct(matrix1_1.m_right);
dFloat xxx0 = CalculateAngle(matrix0.m_up, matrix1_1.m_up, matrix1_1.m_front);
dFloat xxx1 = CalculateAngle(matrix0.m_up, matrix1_1.m_up, matrix1_1.m_right);
localMatrix.GetEulerAngles(euler0, euler1, m_pitchRollYaw);
dTrace(("%d (%f %f) (%f %f)\n", xxxxxx++, xxx0, -euler0[0], xxx1, -euler0[2]));

dFloat a = xxx0+euler0[0];
dFloat b = xxx1+euler0[2];
if ((dAbs (a) > 1.0e-2f) || (dAbs (b) > 1.0e-2f)) {
xxxxxx*=1;
}
*/
#endif

	// save the current joint Omega
	dVector omega0(0.0f);
	dVector omega1(0.0f);
	NewtonBodyGetOmega(m_body0, &omega0[0]);
	if (m_body1) {
		NewtonBodyGetOmega(m_body1, &omega1[0]);
	}
	m_angularOmega = (omega0 - omega1).DotProduct3(matrix1[1]);


	if (m_options.m_option2) {
		if (m_options.m_option3) {
			dCustomSlidingContact::SubmitConstraintLimitSpringDamper(matrix0, matrix1, timestep);
		} else {
			dCustomSlidingContact::SubmitConstraintLimits(matrix0, matrix1, timestep);
		}
	} else if (m_options.m_option3) {
		dCustomSlidingContact::SubmitConstraintSpringDamper(matrix0, matrix1, timestep);
	} else if (m_angularFriction != 0.0f) {
		NewtonUserJointAddAngularRow(m_joint, 0, &matrix1.m_up[0]);
		NewtonUserJointSetRowStiffness(m_joint, m_stiffness);
		NewtonUserJointSetRowAcceleration(m_joint, -m_angularOmega / timestep);
		NewtonUserJointSetRowMinimumFriction(m_joint, -m_angularFriction);
		NewtonUserJointSetRowMaximumFriction(m_joint, m_angularFriction);
	}
}


void dCustomSlidingContact::Debug(dDebugDisplay* const debugDisplay) const
{
	dCustomSlider::Debug(debugDisplay);

	if (m_options.m_option2) {
		dMatrix matrix0;
		dMatrix matrix1;
		CalculateGlobalMatrix(matrix0, matrix1);

		const int subdiv = 12;
		dVector arch[subdiv + 1];
		const float radius = debugDisplay->m_debugScale;

		if ((m_maxAngle > 1.0e-3f) || (m_minAngle < -1.0e-3f)) {
			// show pitch angle limits
			dVector point(dFloat(radius), dFloat(0.0f), dFloat(0.0f), dFloat(0.0f));

			dFloat minAngle = m_minAngle;
			dFloat maxAngle = m_maxAngle;
			if ((maxAngle - minAngle) >= dPi * 2.0f) {
				minAngle = 0.0f;
				maxAngle = dPi * 2.0f;
			}

			dFloat angleStep = (maxAngle - minAngle) / subdiv;
			dFloat angle0 = minAngle;

			matrix1.m_posit = matrix0.m_posit;
			debugDisplay->SetColor(dVector(0.0f, 0.5f, 0.0f, 0.0f));
			for (int i = 0; i <= subdiv; i++) {
				arch[i] = matrix1.TransformVector(dYawMatrix(angle0).RotateVector(point));
				debugDisplay->DrawLine(matrix1.m_posit, arch[i]);
				angle0 += angleStep;
			}

			for (int i = 0; i < subdiv; i++) {
				debugDisplay->DrawLine(arch[i], arch[i + 1]);
			}
		}
	}
}
