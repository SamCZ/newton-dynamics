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
#include "ndDemoMesh.h"
#include "ndPhysicsWorld.h"
#include "ndVehicleCommon.h"

D_CLASS_REFLECTION_IMPLEMENT_LOADER(ndVehicleSelector)

ndVehicleDectriptor::ndEngineTorqueCurve::ndEngineTorqueCurve()
{
	// take from the data sheet of a 2005 dodge viper, 
	// some values are missing so I have to improvise them
	dFloat32 fuelInjectionRate = 10.0f;
	dFloat32 idleTorquePoundFoot = 100.0f;
	dFloat32 idleRmp = 800.0f;
	dFloat32 horsePower = 400.0f;
	dFloat32 rpm0 = 5000.0f;
	dFloat32 rpm1 = 6200.0f;
	dFloat32 horsePowerAtRedLine = 100.0f;
	dFloat32 redLineRpm = 8000.0f;
	Init(fuelInjectionRate, idleTorquePoundFoot, idleRmp,
		horsePower, rpm0, rpm1, horsePowerAtRedLine, redLineRpm);
}

void ndVehicleDectriptor::ndEngineTorqueCurve::Init(
	dFloat32 fuelInjectionRate,
	dFloat32 idleTorquePoundFoot, dFloat32 idleRmp,
	dFloat32 horsePower, dFloat32 rpm0, dFloat32 rpm1,
	dFloat32 horsePowerAtRedLine, dFloat32 redLineRpm)
{
	m_fuelInjectionRate = fuelInjectionRate;
	m_torqueCurve[0] = ndTorqueTap(0.0f, idleTorquePoundFoot);
	m_torqueCurve[1] = ndTorqueTap(idleRmp, idleTorquePoundFoot);
	
	dFloat32 power = horsePower * 746.0f;
	dFloat32 omegaInRadPerSec = rpm0 * 0.105f;
	dFloat32 torqueInPoundFood = (power / omegaInRadPerSec) / 1.36f;
	m_torqueCurve[2] = ndTorqueTap(rpm0, torqueInPoundFood);
	
	power = horsePower * 746.0f;
	omegaInRadPerSec = rpm1 * 0.105f;
	torqueInPoundFood = (power / omegaInRadPerSec) / 1.36f;
	m_torqueCurve[3] = ndTorqueTap(rpm1, torqueInPoundFood);
	
	power = horsePowerAtRedLine * 746.0f;
	omegaInRadPerSec = redLineRpm * 0.105f;
	torqueInPoundFood = (power / omegaInRadPerSec) / 1.36f;
	m_torqueCurve[4] = ndTorqueTap(redLineRpm, torqueInPoundFood);
}

dFloat32 ndVehicleDectriptor::ndEngineTorqueCurve::GetFuelRate() const
{
	return m_fuelInjectionRate;
}

dFloat32 ndVehicleDectriptor::ndEngineTorqueCurve::GetIdleRadPerSec() const
{
	return m_torqueCurve[1].m_radPerSeconds;
}

dFloat32 ndVehicleDectriptor::ndEngineTorqueCurve::GetLowGearShiftRadPerSec() const
{
	return m_torqueCurve[2].m_radPerSeconds;
}

dFloat32 ndVehicleDectriptor::ndEngineTorqueCurve::GetHighGearShiftRadPerSec() const
{
	return m_torqueCurve[3].m_radPerSeconds;
}

dFloat32 ndVehicleDectriptor::ndEngineTorqueCurve::GetRedLineRadPerSec() const
{
	const int maxIndex = sizeof(m_torqueCurve) / sizeof(m_torqueCurve[0]);
	return m_torqueCurve[maxIndex - 1].m_radPerSeconds;
}

dFloat32 ndVehicleDectriptor::ndEngineTorqueCurve::GetTorque(dFloat32 omegaInRadPerSeconds) const
{
	const int maxIndex = sizeof(m_torqueCurve) / sizeof(m_torqueCurve[0]);
	omegaInRadPerSeconds = dClamp(omegaInRadPerSeconds, dFloat32(0.0f), m_torqueCurve[maxIndex - 1].m_radPerSeconds);

	for (dInt32 i = 1; i < maxIndex; i++)
	{
		if (omegaInRadPerSeconds <= m_torqueCurve[i].m_radPerSeconds)
		{
			dFloat32 omega0 = m_torqueCurve[i - 0].m_radPerSeconds;
			dFloat32 omega1 = m_torqueCurve[i - 1].m_radPerSeconds;

			dFloat32 torque0 = m_torqueCurve[i - 0].m_torqueInNewtonMeters;
			dFloat32 torque1 = m_torqueCurve[i - 1].m_torqueInNewtonMeters;

			dFloat32 torque = torque0 + (omegaInRadPerSeconds - omega0) * (torque1 - torque0) / (omega1 - omega0);
			return torque;
		}
	}

	return m_torqueCurve[maxIndex - 1].m_torqueInNewtonMeters;
}

ndVehicleDectriptor::ndVehicleDectriptor(const char* const fileName)
	:m_comDisplacement(dVector::m_zero)
{
	strncpy(m_name, fileName, sizeof(m_name));

	dFloat32 fuelInjectionRate = 10.0f;
	dFloat32 idleTorquePoundFoot = 100.0f;
	dFloat32 idleRmp = 900.0f;
	dFloat32 horsePower = 400.0f;
	dFloat32 rpm0 = 5000.0f;
	dFloat32 rpm1 = 6200.0f;
	dFloat32 horsePowerAtRedLine = 100.0f;
	dFloat32 redLineRpm = 8000.0f;
	m_engine.Init(fuelInjectionRate, idleTorquePoundFoot, idleRmp, horsePower, rpm0, rpm1, horsePowerAtRedLine, redLineRpm);

	m_chassisMass = 1000.0f;
	m_chassisAngularDrag = 0.25f;
	m_transmission.m_gearsCount = 4;
	m_transmission.m_neutral = 0.0f;
	m_transmission.m_crownGearRatio = 10.0f;
	m_transmission.m_reverseRatio = -3.0f;
	//m_transmission.m_forwardRatios[0] = 2.25f;
	//m_transmission.m_forwardRatios[1] = 1.60f;
	//m_transmission.m_forwardRatios[2] = 1.10f;
	//m_transmission.m_forwardRatios[3] = 0.80f;

	m_transmission.m_forwardRatios[0] = 3.0f;
	m_transmission.m_forwardRatios[1] = 1.50f;
	m_transmission.m_forwardRatios[2] = 1.10f;
	m_transmission.m_forwardRatios[3] = 0.80f;

	m_transmission.m_torqueConverter = 2000.0f;
	m_transmission.m_idleClutchTorque = 200.0f;
	m_transmission.m_lockedClutchTorque = 1.0e6f;
	m_transmission.m_manual = true;

	m_frontTire.m_mass = 20.0f;
	m_frontTire.m_springK = 1000.0f;
	m_frontTire.m_damperC = 20.0f;
	m_frontTire.m_regularizer = 0.1f;
	m_frontTire.m_upperStop = -0.05f;
	m_frontTire.m_lowerStop = 0.2f;
	m_frontTire.m_verticalOffset = 0.0f;
	m_frontTire.m_brakeTorque = 1500.0f;
	m_frontTire.m_handBrakeTorque = 1500.0f;
	m_frontTire.m_steeringAngle = 35.0f * dDegreeToRad;
	m_frontTire.m_laterialStiffness  = 100.0f / 1000.0f;
	m_frontTire.m_longitudinalStiffness  = 600.0f / 1000.0f;

	m_rearTire.m_mass = 20.0f;
	m_rearTire.m_springK = 1000.0f;
	m_rearTire.m_damperC = 20.0f;
	m_rearTire.m_regularizer = 0.1f;
	m_rearTire.m_upperStop = -0.05f;
	m_rearTire.m_lowerStop = 0.2f;
	m_rearTire.m_steeringAngle = 0.0f;
	m_rearTire.m_verticalOffset = 0.0f;
	m_rearTire.m_brakeTorque = 1500.0f;
	m_rearTire.m_handBrakeTorque = 1000.0f;
	m_rearTire.m_laterialStiffness  = 100.0f / 1000.0f;
	m_rearTire.m_longitudinalStiffness  = 600.0f / 1000.0f;

	m_motorMass = 20.0f;
	m_motorRadius = 0.25f;

	m_differentialMass = 20.0f;
	m_differentialRadius = 0.25f;
	m_slipDifferentialRmpLock = 30.0f;
	m_frictionCoefficientScale = 1.5f;

	m_torsionBarSpringK = 100.0f;
	m_torsionBarDamperC = 10.0f;
	m_torsionBarRegularizer = 0.15f;
	m_torsionBarType = m_noWheelAxle;

	m_differentialType = m_rearWheelDrive;

	m_useHardSolverMode = true;
}

ndVehicleSelector::ndVehicleSelector()
	:ndModel()
	,m_changeVehicle()
{
}

ndVehicleSelector::ndVehicleSelector(const dLoadSaveBase::dLoadDescriptor& desc)
	:ndModel(dLoadSaveBase::dLoadDescriptor(desc))
	,m_changeVehicle()
{
}

void ndVehicleSelector::Save(const dLoadSaveBase::dSaveDescriptor& desc) const
{
	nd::TiXmlElement* const childNode = new nd::TiXmlElement(ClassName());
	desc.m_rootNode->LinkEndChild(childNode);
	childNode->SetAttribute("hashId", desc.m_nodeNodeHash);
	ndModel::Save(dLoadSaveBase::dSaveDescriptor(desc, childNode));
}

void ndVehicleSelector::PostUpdate(ndWorld* const world, dFloat32)
{
	dFixSizeArray<char, 32> buttons;
	ndDemoEntityManager* const scene = ((ndPhysicsWorld*)world)->GetManager();
	scene->GetJoystickButtons(buttons);
	if (m_changeVehicle.Update(scene->GetKeyState('C') || buttons[5]))
	{
		const ndModelList& modelList = world->GetModelList();

		dInt32 vehiclesCount = 0;
		ndBasicVehicle* vehicleArray[1024];
		for (ndModelList::dNode* node = modelList.GetFirst(); node; node = node->GetNext())
		{
			ndModel* const model = node->GetInfo();
			//if (!strcmp(model->ClassName(), "ndBasicVehicle"))
			if (model->GetAsMultiBodyVehicle())
			{
				vehicleArray[vehiclesCount] = (ndBasicVehicle*)model->GetAsMultiBodyVehicle();
				vehiclesCount++;
			}
		}

		if (vehiclesCount > 1)
		{
			for (dInt32 i = 0; i < vehiclesCount; i++)
			{
				if (vehicleArray[i]->IsPlayer())
				{
					ndBasicVehicle* const nexVehicle = vehicleArray[(i + 1) % vehiclesCount];
					vehicleArray[i]->SetAsPlayer(scene, false);
					nexVehicle->SetAsPlayer(scene, true);
					break;
				}
			}
		}
	}
}

ndBasicVehicle::ndBasicVehicle(const ndVehicleDectriptor& desc)
	:ndMultiBodyVehicle(dVector(1.0f, 0.0f, 0.0f, 0.0f), dVector(0.0f, 1.0f, 0.0f, 0.0f))
	,m_configuration(desc)
	,m_steerAngle(0.0f)
	,m_parking()
	,m_ignition()
	,m_neutralGear()
	,m_reverseGear()
	,m_forwardGearUp()
	,m_forwardGearDown()
	,m_manualTransmission()
	,m_currentGear(0)
	,m_autoGearShiftTimer(0)
	,m_isPlayer(false)
	,m_isParked(true)
	,m_isManualTransmission(desc.m_transmission.m_manual)
{
}

ndBasicVehicle::~ndBasicVehicle()
{
}

void ndBasicVehicle::SetAsPlayer(ndDemoEntityManager* const, bool mode)
{
	m_isPlayer = mode;
}

bool ndBasicVehicle::IsPlayer() const
{
	return m_isPlayer;
}

dFloat32 ndBasicVehicle::GetFrictionCoeficient(const ndMultiBodyVehicleTireJoint* const, const ndContactMaterial&) const
{
	return m_configuration.m_frictionCoefficientScale;
}

void ndBasicVehicle::CalculateTireDimensions(const char* const tireName, dFloat32& width, dFloat32& radius, ndDemoEntity* const vehEntity) const
{
	// find the the tire visual mesh 
	ndDemoEntity* const tirePart = vehEntity->Find(tireName);
	dAssert(tirePart);

	// make a convex hull collision shape to assist in calculation of the tire shape size
	ndDemoMesh* const tireMesh = (ndDemoMesh*)tirePart->GetMesh();

	const dMatrix matrix(tirePart->GetMeshMatrix());

	dArray<dVector> temp;
	tireMesh->GetVertexArray(temp);

	dVector minVal(1.0e10f);
	dVector maxVal(-1.0e10f);
	for (dInt32 i = 0; i < temp.GetCount(); i++)
	{
		dVector p(matrix.TransformVector(temp[i]));
		minVal = minVal.GetMin(p);
		maxVal = maxVal.GetMax(p);
	}

	dVector size(maxVal - minVal);
	width = size.m_x;
	radius = size.m_y * 0.5f;
}

ndBodyDynamic* ndBasicVehicle::CreateTireBody(ndDemoEntityManager* const scene, ndBodyDynamic* const parentBody, const ndVehicleDectriptor::ndTireDefinition& definition, const char* const tireName) const
{
	dFloat32 width;
	dFloat32 radius;
	ndDemoEntity* const parentEntity = (ndDemoEntity*)parentBody->GetNotifyCallback()->GetUserData();
	CalculateTireDimensions(tireName, width, radius, parentEntity);

	ndShapeInstance tireCollision(CreateTireShape(radius, width));

	ndDemoEntity* const tireEntity = parentEntity->Find(tireName);
	dMatrix matrix(tireEntity->CalculateGlobalMatrix(nullptr));

	const dMatrix chassisMatrix(m_localFrame * m_chassis->GetMatrix());
	matrix.m_posit += chassisMatrix.m_up.Scale(definition.m_verticalOffset);

	ndBodyDynamic* const tireBody = new ndBodyDynamic();
	tireBody->SetNotifyCallback(new ndDemoEntityNotify(scene, tireEntity, parentBody));
	tireBody->SetMatrix(matrix);
	tireBody->SetCollisionShape(tireCollision);
	tireBody->SetMassMatrix(definition.m_mass, tireCollision);

	return tireBody;
}

void ndBasicVehicle::ApplyInputs(ndWorld* const world, dFloat32)
{
	if (m_isPlayer && m_motor)
	{
		dFixSizeArray<dFloat32, 8> axis;
		dFixSizeArray<char, 32> buttons;
		ndDemoEntityManager* const scene = ((ndPhysicsWorld*)world)->GetManager();

		scene->GetJoystickAxis(axis);
		scene->GetJoystickButtons(buttons);
		
		dFloat32 brake = scene->GetKeyState('S') ? dFloat32 (1.0f) : dFloat32 (0.0f);
		if (brake == 0.0f)
		{
			dFloat32 val = (axis[4] + 1.0f) * 0.5f;
			brake = val * val * val;
			//dTrace(("brake %f\n", brake));
		}

		dFloat32 throttle = dFloat32(scene->GetKeyState('W')) ? 1.0f : 0.0f;
		if (throttle == 0.0f)
		{
			throttle = (axis[5] + 1.0f) * 0.5f;
			throttle = throttle * throttle * throttle;
		}

		dFloat32 steerAngle = dFloat32(scene->GetKeyState('A')) - dFloat32(scene->GetKeyState('D'));
		if (dAbs(steerAngle) == 0.0f)
		{
			steerAngle = - axis[0] * axis[0] * axis[0];
		}
		m_steerAngle = m_steerAngle + (steerAngle - m_steerAngle) * 0.15f;

		dFloat32 handBrake = (scene->GetKeyState(' ') || buttons[4]) ? 1.0f : 0.0f;

		if (m_parking.Update(scene->GetKeyState('P') || buttons[6]))
		{
			m_isParked = !m_isParked;
		}

		if (m_ignition.Update(scene->GetKeyState('I') || buttons[7]))
		{
			m_motor->SetStart(!m_motor->GetStart());
		}

		if (m_manualTransmission.Update(scene->GetKeyState('?') || scene->GetKeyState('/') || buttons[8]))
		{
			m_isManualTransmission = !m_isManualTransmission;
		}

		// transmission front gear up
		if (m_forwardGearUp.Update(scene->GetKeyState('>') || scene->GetKeyState('.') || buttons[11]))
		{
			m_isParked = false;
			if (m_currentGear > m_configuration.m_transmission.m_gearsCount)
			{
				m_currentGear = 0;
			}
			else
			{
				m_currentGear++;
				if (m_currentGear >= m_configuration.m_transmission.m_gearsCount)
				{
					m_currentGear = m_configuration.m_transmission.m_gearsCount - 1;
				}
			}
			dFloat32 gearGain = m_configuration.m_transmission.m_crownGearRatio * m_configuration.m_transmission.m_forwardRatios[m_currentGear];
			m_gearBox->SetRatio(gearGain);
			m_autoGearShiftTimer = AUTOMATION_TRANSMISSION_FRAME_DELAY;
		}

		// transmission front gear down
		if (m_forwardGearDown.Update(scene->GetKeyState('<') || scene->GetKeyState(',') || buttons[13]))
		{
			m_isParked = false;
			if (m_currentGear > m_configuration.m_transmission.m_gearsCount)
			{
				m_currentGear = 0;
			}
			else
			{
				m_currentGear--;
				if (m_currentGear <= 0)
				{
					m_currentGear = 0;
				}
			}
			dFloat32 gearGain = m_configuration.m_transmission.m_crownGearRatio * m_configuration.m_transmission.m_forwardRatios[m_currentGear];
			m_gearBox->SetRatio(gearGain);
			m_autoGearShiftTimer = AUTOMATION_TRANSMISSION_FRAME_DELAY;
		}

		const dFloat32 omega = m_motor->GetRpm() / dRadPerSecToRpm;
		if (!m_isManualTransmission && (m_autoGearShiftTimer < 0))
		{
			if (m_currentGear < m_configuration.m_transmission.m_gearsCount)
			{
				if (omega < m_configuration.m_engine.GetLowGearShiftRadPerSec())
				{
					if (m_currentGear > 0)
					{
						m_currentGear--;
						dFloat32 gearGain = m_configuration.m_transmission.m_crownGearRatio * m_configuration.m_transmission.m_forwardRatios[m_currentGear];
						m_gearBox->SetRatio(gearGain);
						m_autoGearShiftTimer = AUTOMATION_TRANSMISSION_FRAME_DELAY;
					}
				}
				else if (omega > m_configuration.m_engine.GetHighGearShiftRadPerSec())
				{
					if (m_currentGear < (m_configuration.m_transmission.m_gearsCount - 1))
					{
						m_currentGear++;
						dFloat32 gearGain = m_configuration.m_transmission.m_crownGearRatio * m_configuration.m_transmission.m_forwardRatios[m_currentGear];
						m_gearBox->SetRatio(gearGain);
						m_autoGearShiftTimer = AUTOMATION_TRANSMISSION_FRAME_DELAY;
					}
				}
			}
		}
		m_autoGearShiftTimer--;

		// neural gear
		if (m_neutralGear.Update(scene->GetKeyState('N') || buttons[10]))
		{
			m_currentGear = sizeof(m_configuration.m_transmission.m_forwardRatios) / sizeof(m_configuration.m_transmission.m_forwardRatios[0]) + 1;
			m_gearBox->SetRatio(0.0f);
		}

		// reverse gear
		if (m_reverseGear.Update(scene->GetKeyState('R') || buttons[12]))
		{
			m_isParked = false;
			m_currentGear = sizeof(m_configuration.m_transmission.m_forwardRatios) / sizeof(m_configuration.m_transmission.m_forwardRatios[0]);

			dFloat32 gearGain = m_configuration.m_transmission.m_crownGearRatio * m_configuration.m_transmission.m_forwardRatios[m_currentGear];
			m_gearBox->SetRatio(gearGain);
		}

		if (m_isParked)
		{
			brake = 1.0f;
		}

		for (dList<ndMultiBodyVehicleTireJoint*>::dNode* node = m_tireList.GetFirst(); node; node = node->GetNext())
		{
			ndMultiBodyVehicleTireJoint* const tire = node->GetInfo();
			tire->SetSteering(m_steerAngle);
			tire->SetBrake(brake);
			tire->SetHandBrake(handBrake);
		}

		// set the transmission Torque converter when the power reverses.
		m_gearBox->SetInternalLosesTorque(m_configuration.m_transmission.m_torqueConverter);
		if (omega <= (m_configuration.m_engine.GetIdleRadPerSec() * 1.01f))
		{
			m_gearBox->SetClutchTorque(m_configuration.m_transmission.m_idleClutchTorque);
		}
		else
		{
			m_gearBox->SetClutchTorque(m_configuration.m_transmission.m_lockedClutchTorque);
		}

		m_motor->SetThrottle(throttle);
		m_motor->SetFuelRate(m_configuration.m_engine.GetFuelRate());
		m_motor->SetTorque(m_configuration.m_engine.GetTorque(m_motor->GetRpm() / dRadPerSecToRpm));
	}
}
