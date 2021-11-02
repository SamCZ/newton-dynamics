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

#ifndef __D_BODY_DYNAMIC_BODY_H__
#define __D_BODY_DYNAMIC_BODY_H__

#include "ndNewtonStdafx.h"

#define D_MAX_SPEED_ATT	dFloat32(0.02f)
//#define D_FREEZE_ACCEL	dFloat32(0.1f)
#define D_FREEZE_ACCEL		dFloat32(1.0f)
#define D_FREEZE_SPEED		dFloat32(0.032f)

#define D_FREEZE_ACCEL2		(D_FREEZE_ACCEL * D_FREEZE_ACCEL)
#define D_FREEZE_SPEED2		(D_FREEZE_SPEED * D_FREEZE_SPEED)

#define D_FREEZE_MAG		D_FREEZE_ACCEL
#define D_FREEZE_MAG2		(D_FREEZE_MAG * D_FREEZE_MAG)

#define D_ERR_TOLERANCE		dFloat32(1.0e-2f)
#define D_ERR_TOLERANCE2	(D_ERR_TOLERANCE * D_ERR_TOLERANCE)

D_MSV_NEWTON_ALIGN_32
class ndBodyDynamic: public ndBodyKinematic
{
	public:
	D_CLASS_REFLECTION(ndBodyDynamic);
	D_NEWTON_API ndBodyDynamic();
	D_NEWTON_API ndBodyDynamic(const dLoadSaveBase::dLoadDescriptor& desc);
	D_NEWTON_API virtual ~ndBodyDynamic ();

	D_NEWTON_API virtual ndBodyDynamic* GetAsBodyDynamic() { return this; }
	D_NEWTON_API virtual void ApplyExternalForces(dInt32 threadIndex, dFloat32 timestep);
	D_NEWTON_API virtual void AddDampingAcceleration(dFloat32 timestep);
	D_NEWTON_API virtual void IntegrateVelocity(dFloat32 timestep);

	D_NEWTON_API virtual void Save(const dLoadSaveBase::dSaveDescriptor& desc) const;

	D_NEWTON_API void SetForce(const dVector& force);
	D_NEWTON_API void SetTorque(const dVector& torque);

	D_NEWTON_API dFloat32 GetLinearDamping() const;
	D_NEWTON_API void SetLinearDamping(dFloat32 linearDamp);

	D_NEWTON_API dVector GetAngularDamping() const;
	D_NEWTON_API void SetAngularDamping(const dVector& angularDamp);

	virtual dVector GetForce() const;
	virtual dVector GetTorque() const;

	dVector GetAccel() const;
	virtual void SetAccel(const dVector& accel);

	dVector GetAlpha() const;
	virtual void SetAlpha(const dVector& alpha);

	void IntegrateGyroSubstep(const dVector& timestep);
	ndJacobian IntegrateForceAndToque(const dVector& force, const dVector& torque, const dVector& timestep) const;

	void SetExternalForces();

	protected:
	dVector m_accel;
	dVector m_alpha;
	dVector m_externalForce;
	dVector m_externalTorque;
	dVector m_impulseForce;
	dVector m_impulseTorque;
	dVector m_savedExternalForce;
	dVector m_savedExternalTorque;
	dVector m_dampCoef;
	dVector m_cachedDampCoef;
	dFloat32 m_cachedTimeStep;

	friend class ndDynamicsUpdate;
	friend class ndDynamicsUpdateSoa;
	friend class ndDynamicsUpdateAvx2;
	friend class ndDynamicsUpdateOpencl;
} D_GCC_NEWTON_ALIGN_32 ;

class ndBodySentinel : public ndBodyDynamic
{
	ndBodySentinel* GetAsBodySentinel() { return this; }
};

inline dVector ndBodyDynamic::GetForce() const
{
	return m_externalForce;
}

inline dVector ndBodyDynamic::GetTorque() const
{
	return m_externalTorque;
}

inline dVector ndBodyDynamic::GetAccel() const
{
	return m_accel;
}

inline void ndBodyDynamic::SetAccel(const dVector& accel)
{
	m_accel = accel;
}

inline dVector ndBodyDynamic::GetAlpha() const
{
	return m_alpha;
}

inline void ndBodyDynamic::SetAlpha(const dVector& alpha)
{
	m_alpha = alpha;
}

inline void ndBodyDynamic::SetExternalForces()
{
	m_savedExternalForce = m_externalForce;
	m_savedExternalTorque = m_externalTorque;
}

#endif 


