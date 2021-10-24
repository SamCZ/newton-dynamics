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

#ifndef __D_MULTIBODY_VEHICLE_H__
#define __D_MULTIBODY_VEHICLE_H__

#include "ndNewtonStdafx.h"
#include "ndModel.h"

class ndWorld;
class ndWheelDescriptor;
class ndMultiBodyVehicleMotor;
class ndMultiBodyVehicleGearBox;
class ndMultiBodyVehicleTireJoint;
class ndMultiBodyVehicleTorsionBar;
class ndMultiBodyVehicleDifferential;
class ndMultiBodyVehicleDifferentialAxle;

#define dRadPerSecToRpm dFloat32(9.55f)

class ndMultiBodyVehicle: public ndModel
{
	public:
	class ndDownForce
	{
		public:
		class ndSpeedForcePair
		{
			public:
			dFloat32 m_speed;
			dFloat32 m_forceFactor;
			dFloat32 m_aerodynamicDownforceConstant;
			friend class ndDownForce;
		};

		ndDownForce();
		dFloat32 GetDownforceFactor(dFloat32 speed) const;

		void Load(const nd::TiXmlNode* const xmlNode);
		void Save(nd::TiXmlNode* const xmlNode) const;

		private:
		dFloat32 CalculateFactor(const ndSpeedForcePair* const entry) const;
	
		dFloat32 m_gravity;
		dFloat32 m_suspensionStiffnessModifier;
		ndSpeedForcePair m_downForceTable[5];
		friend class ndMultiBodyVehicle;
		friend class ndMultiBodyVehicleTireJoint;
	};

	D_CLASS_REFLECTION(ndMultiBodyVehicle);
	D_NEWTON_API ndMultiBodyVehicle(const dLoadSaveBase::dLoadDescriptor& desc);
	D_NEWTON_API ndMultiBodyVehicle(const dVector& frontDir, const dVector& upDir);
	D_NEWTON_API virtual ~ndMultiBodyVehicle ();

	D_NEWTON_API virtual void AddToWorld(ndWorld* const world);
	D_NEWTON_API virtual void RemoveFromToWorld(ndWorld* const world);

	ndMultiBodyVehicle* GetAsMultiBodyVehicle();
	virtual dFloat32 GetFrictionCoeficient(const ndMultiBodyVehicleTireJoint* const, const ndContactMaterial&) const;

	D_NEWTON_API dFloat32 GetSpeed() const;
	D_NEWTON_API ndShapeInstance CreateTireShape(dFloat32 radius, dFloat32 width) const;

	D_NEWTON_API void AddChassis(ndBodyDynamic* const chassis);
	D_NEWTON_API ndMultiBodyVehicleMotor* AddMotor(dFloat32 mass, dFloat32 radius);
	D_NEWTON_API ndMultiBodyVehicleTireJoint* AddTire(const ndWheelDescriptor& desc, ndBodyDynamic* const tire);
	D_NEWTON_API ndMultiBodyVehicleTireJoint* AddAxleTire(const ndWheelDescriptor& desc, ndBodyDynamic* const tire, ndBodyDynamic* const axleBody);
	D_NEWTON_API ndMultiBodyVehicleGearBox* AddGearBox(ndMultiBodyVehicleMotor* const motor, ndMultiBodyVehicleDifferential* const differential);
	D_NEWTON_API ndMultiBodyVehicleDifferential* AddDifferential(dFloat32 mass, dFloat32 radius, ndMultiBodyVehicleTireJoint* const leftTire, ndMultiBodyVehicleTireJoint* const rightTire, dFloat32 slipOmegaLock);
	D_NEWTON_API ndMultiBodyVehicleDifferential* AddDifferential(dFloat32 mass, dFloat32 radius, ndMultiBodyVehicleDifferential* const leftDifferential, ndMultiBodyVehicleDifferential* const rightDifferential, dFloat32 slipOmegaLock);
	D_NEWTON_API ndMultiBodyVehicleTorsionBar* AddTorsionBar(ndBodyDynamic* const sentinel);

	D_NEWTON_API void AddExtraBody(ndBodyDynamic* const body);
	D_NEWTON_API void AddExtraJoint(ndJointBilateralConstraint* const joint);

	D_NEWTON_API void SetVehicleSolverModel(bool hardJoint);

	private:
	void ApplySteering();
	void ApplyTireModel();
	void ApplyAerodynamics();
	void ApplyAligmentAndBalancing();
	ndBodyDynamic* CreateInternalBodyPart(dFloat32 mass, dFloat32 radius) const;
	void BrushTireModel(ndMultiBodyVehicleTireJoint* const tire, ndContactMaterial& contactPoint) const;

	protected:
	virtual void ApplyInputs(ndWorld* const world, dFloat32 timestep);
	D_NEWTON_API virtual void Debug(ndConstraintDebugCallback& context) const;
	D_NEWTON_API virtual void Update(ndWorld* const world, dFloat32 timestep);
	D_NEWTON_API virtual void PostUpdate(ndWorld* const world, dFloat32 timestep);
	D_NEWTON_API virtual void Save(const dLoadSaveBase::dSaveDescriptor& desc) const;

	dMatrix m_localFrame;
	ndBodyDynamic* m_chassis;
	ndMultiBodyVehicleMotor* m_motor;
	ndShapeChamferCylinder* m_tireShape;
	ndMultiBodyVehicleGearBox* m_gearBox;
	ndMultiBodyVehicleTorsionBar* m_torsionBar;
	dList<ndMultiBodyVehicleTireJoint*> m_tireList;
	dList<ndBodyDynamic*> m_extraBodiesAttachmentList;
	dList<ndMultiBodyVehicleDifferentialAxle*> m_axleList;
	dList<ndMultiBodyVehicleDifferential*> m_differentialList;
	dList<ndJointBilateralConstraint*> m_extraJointsAttachmentList;
	ndDownForce m_downForce;
	
	friend class ndMultiBodyVehicleMotor;
	friend class ndMultiBodyVehicleGearBox;
	friend class ndMultiBodyVehicleTireJoint;
	friend class ndMultiBodyVehicleTorsionBar;
};

inline void ndMultiBodyVehicle::ApplyInputs(ndWorld* const, dFloat32)
{
}

inline dFloat32 ndMultiBodyVehicle::GetFrictionCoeficient(const ndMultiBodyVehicleTireJoint* const, const ndContactMaterial&) const
{
	return dFloat32(2.0f);
}

inline ndMultiBodyVehicle* ndMultiBodyVehicle::GetAsMultiBodyVehicle() 
{ 
	return this; 
}

#endif