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

#include "ndCoreStdafx.h"
#include "ndCollisionStdafx.h"
#include "ndScene.h"
#include "ndContact.h"
#include "ndShapeInstance.h"
#include "ndRayCastNotify.h"
#include "ndBodyKinematic.h"
#include "ndShapeCompound.h"

ndVector ndShapeInstance::m_padding(D_MAX_SHAPE_AABB_PADDING, D_MAX_SHAPE_AABB_PADDING, D_MAX_SHAPE_AABB_PADDING, ndFloat32(0.0f));

ndShapeInstance::ndShapeInstance(ndShape* const shape)
	:ndClassAlloc()
	,m_globalMatrix(dGetIdentityMatrix())
	,m_localMatrix(dGetIdentityMatrix())
	,m_alignmentMatrix(dGetIdentityMatrix())
	,m_scale(ndVector::m_one & ndVector::m_triplexMask)
	,m_invScale(ndVector::m_one & ndVector::m_triplexMask)
	,m_maxScale(ndVector::m_one & ndVector::m_triplexMask)
	,m_shape(shape ? shape->AddRef() : shape)
	,m_ownerBody(nullptr)
	,m_subCollisionHandle(nullptr)
	,m_parent(nullptr)
	,m_skinMargin(ndFloat32(0.0f))
	,m_scaleType(m_unit)
	,m_collisionMode(true)
{
	if (shape)
	{
		ndShapeCompound* const compound = shape->GetAsShapeCompound();
		if (compound)
		{
			compound->SetOwner(this);
		}
	}
}

ndShapeInstance::ndShapeInstance(const ndShapeInstance& instance)
	:ndClassAlloc()
	,m_globalMatrix(instance.m_globalMatrix)
	,m_localMatrix(instance.m_localMatrix)
	,m_alignmentMatrix(instance.m_alignmentMatrix)
	,m_scale(instance.m_scale)
	,m_invScale(instance.m_invScale)
	,m_maxScale(instance.m_maxScale)
	,m_shapeMaterial(instance.m_shapeMaterial)
	,m_shape(instance.m_shape->AddRef())
	,m_ownerBody(instance.m_ownerBody)
	,m_subCollisionHandle(instance.m_subCollisionHandle)
	,m_parent(instance.m_parent)
	,m_skinMargin(instance.m_skinMargin)
	,m_scaleType(instance.m_scaleType)
	,m_collisionMode(instance.m_collisionMode)
{
	ndShapeCompound* const compound = ((ndShape*)m_shape)->GetAsShapeCompound();
	if (compound)
	{
		m_shape->Release();
		m_shape = new ndShapeCompound(*compound, this);
		m_shape->AddRef();
	}
}

ndShapeInstance::ndShapeInstance(const ndShapeInstance& instance, ndShape* const shape)
	:ndClassAlloc()
	,m_globalMatrix(instance.m_globalMatrix)
	,m_localMatrix(instance.m_localMatrix)
	,m_alignmentMatrix(instance.m_alignmentMatrix)
	,m_scale(instance.m_scale)
	,m_invScale(instance.m_invScale)
	,m_maxScale(instance.m_maxScale)
	,m_shapeMaterial(instance.m_shapeMaterial)
	,m_shape(shape->AddRef())
	,m_ownerBody(instance.m_ownerBody)
	,m_subCollisionHandle(instance.m_subCollisionHandle)
	,m_parent(instance.m_parent)
	,m_skinMargin(instance.m_skinMargin)
	,m_scaleType(instance.m_scaleType)
	,m_collisionMode(instance.m_collisionMode)
{
}

ndShapeInstance::ndShapeInstance(const nd::TiXmlNode* const xmlNode, const ndShapeLoaderCache& shapesCache)
	:ndClassAlloc()
	,m_globalMatrix(dGetIdentityMatrix())
	,m_localMatrix(dGetIdentityMatrix())
	,m_alignmentMatrix(dGetIdentityMatrix())
	,m_scale(ndVector::m_one & ndVector::m_triplexMask)
	,m_invScale(ndVector::m_one & ndVector::m_triplexMask)
	,m_maxScale(ndVector::m_one & ndVector::m_triplexMask)
	,m_shape(nullptr)
	,m_ownerBody(nullptr)
	,m_subCollisionHandle(nullptr)
	,m_parent(nullptr)
	,m_skinMargin(ndFloat32(0.0f))
	,m_scaleType(m_unit)
	,m_collisionMode(true)
{
	ndInt32 index = xmlGetInt(xmlNode, "shapeHashId");
	const ndShapeInstance& cacheInstance = shapesCache.Find(index)->GetInfo();
	m_shape = cacheInstance.GetShape()->AddRef();
	
	m_localMatrix = xmlGetMatrix(xmlNode, "localMatrix");
	m_alignmentMatrix = xmlGetMatrix(xmlNode, "alignmentMatrix");
	m_skinMargin = xmlGetFloat(xmlNode, "skinMargin");
	m_collisionMode = xmlGetInt(xmlNode, "collisionMode") ? true : false;
	m_shapeMaterial.m_userId = xmlGetInt64(xmlNode, "materialID");
	m_shapeMaterial.m_data.m_alignPad = xmlGetInt64(xmlNode, "userData");
	
	for (ndInt32 i = 0; i < ndInt32 (sizeof(m_shapeMaterial.m_userParam) / sizeof(m_shapeMaterial.m_userParam[0])); i++)
	{
		char name[64];
		sprintf(name, "intData%d", i);
		m_shapeMaterial.m_userParam[i].m_intData = xmlGetInt64(xmlNode, name);
	}
	
	ndVector scale (xmlGetVector3(xmlNode, "scale"));
	SetScale(scale);
}

ndShapeInstance::~ndShapeInstance()
{
	if (m_shape)
	{
		m_shape->Release();
	}
}

void ndShapeInstance::Save(const ndLoadSaveBase::ndSaveDescriptor& desc) const
{
	nd::TiXmlElement* const paramNode = new nd::TiXmlElement("ndShapeInstance");
	desc.m_rootNode->LinkEndChild(paramNode);

	xmlSaveParam(paramNode, "shapeHashId", desc.m_shapeNodeHash);
	xmlSaveParam(paramNode, "localMatrix", m_localMatrix);
	xmlSaveParam(paramNode, "alignmentMatrix", m_alignmentMatrix);
	xmlSaveParam(paramNode, "scale", m_scale);

	xmlSaveParam(paramNode, "skinMargin", m_skinMargin);
	xmlSaveParam(paramNode, "collisionMode", m_collisionMode ? 1 : 0);
	xmlSaveParam(paramNode, "materialID", m_shapeMaterial.m_userId);
	xmlSaveParam(paramNode, "userData", ndInt64(m_shapeMaterial.m_data.m_alignPad));
	for (ndInt32 i = 0; i < ndInt32(sizeof(m_shapeMaterial.m_userParam) / sizeof(m_shapeMaterial.m_userParam[0])); i++)
	{
		char name[64];
		sprintf(name, "intData%d", i);
		xmlSaveParam(paramNode, name, ndInt64(m_shapeMaterial.m_userParam[i].m_intData));
	}
}

ndShapeInstance& ndShapeInstance::operator=(const ndShapeInstance& instance)
{
	m_globalMatrix = instance.m_globalMatrix;
	m_localMatrix = instance.m_localMatrix;
	m_alignmentMatrix = instance.m_alignmentMatrix;
	m_scale = instance.m_scale;
	m_invScale = instance.m_invScale;
	m_maxScale = instance.m_maxScale;
	m_scaleType = instance.m_scaleType;
	m_shapeMaterial = instance.m_shapeMaterial;
	m_skinMargin = instance.m_skinMargin;
	m_collisionMode = instance.m_collisionMode;
	if (m_shape != nullptr)
	{
		m_shape->Release();
	}
	m_shape = instance.m_shape->AddRef();
	m_ownerBody = instance.m_ownerBody;

	m_subCollisionHandle = instance.m_subCollisionHandle;
	m_parent = instance.m_parent;

	return *this;
}

void ndShapeInstance::DebugShape(const ndMatrix& matrix, ndShapeDebugNotify& debugCallback) const
{
	debugCallback.m_instance = this;
	m_shape->DebugShape(GetScaledTransform(matrix), debugCallback);
}

ndShapeInfo ndShapeInstance::GetShapeInfo() const
{
	ndShapeInfo info(m_shape->GetShapeInfo());
	info.m_offsetMatrix = m_localMatrix;
	info.m_scale = m_scale;
	return info;
}

ndMatrix ndShapeInstance::CalculateInertia() const
{
	ndShape* const shape = (ndShape*)m_shape;
	if (shape->GetAsShapeNull() || !(shape->GetAsShapeConvex() || shape->GetAsShapeCompound()))
	{
		return dGetZeroMatrix();
	}
	else 
	{
		return m_shape->CalculateInertiaAndCenterOfMass(m_alignmentMatrix, m_scale, m_localMatrix);
	}
}

void ndShapeInstance::CalculateObb(ndVector& origin, ndVector& size) const
{
	size = m_shape->GetObbSize();
	origin = m_shape->GetObbOrigin();

	switch (m_scaleType)
	{
		case m_unit:
		{
			size += m_padding;
			break;
		}

		case m_uniform:
		case m_nonUniform:
		{
			size = size * m_scale + m_padding;
			origin = origin * m_scale;
			break;
		}
		case m_global:
		{
			ndVector p0;
			ndVector p1;
			m_shape->CalculateAabb(m_alignmentMatrix, p0, p1);
			size = (ndVector::m_half * (p1 - p0) * m_scale + m_padding) & ndVector::m_triplexMask;
			origin = (ndVector::m_half * (p1 + p0) * m_scale) & ndVector::m_triplexMask;;
			break;
		}
	}

	dAssert(size.m_w == ndFloat32(0.0f));
	dAssert(origin.m_w == ndFloat32(0.0f));
}

ndFloat32 ndShapeInstance::RayCast(ndRayCastNotify& callback, const ndVector& localP0, const ndVector& localP1, const ndBody* const body, ndContactPoint& contactOut) const
{
	ndFloat32 t = ndFloat32(1.2f);
	if (callback.OnRayPrecastAction(body, this))
	{
		switch (m_scaleType)
		{
			case m_unit:
			{
				t = m_shape->RayCast(callback, localP0, localP1, ndFloat32(1.0f), body, contactOut);
				if (t < ndFloat32 (1.0f)) 
				{
					contactOut.m_shapeInstance0 = this;
					contactOut.m_shapeInstance1 = this;
				}
				break;
			}

			case m_uniform:
			{
				ndVector p0(localP0 * m_invScale);
				ndVector p1(localP1 * m_invScale);
				t = m_shape->RayCast(callback, p0, p1, ndFloat32(1.0f), body, contactOut);
				if (t < ndFloat32(1.0f))
				{
					dAssert(!((ndShape*)m_shape)->GetAsShapeCompound());
					contactOut.m_shapeInstance0 = this;
					contactOut.m_shapeInstance1 = this;
				}
				break;
			}

			case m_nonUniform:
			{
				ndVector p0(localP0 * m_invScale);
				ndVector p1(localP1 * m_invScale);
				t = m_shape->RayCast(callback, p0, p1, ndFloat32(1.0f), body, contactOut);
				if (t < ndFloat32(1.0f))
				{
					dAssert(!((ndShape*)m_shape)->GetAsShapeCompound());
					ndVector normal(m_invScale * contactOut.m_normal);
					contactOut.m_normal = normal.Normalize();
					contactOut.m_shapeInstance0 = this;
					contactOut.m_shapeInstance1 = this;
				}
				break;
			}

			case m_global:
			default:
			{
				ndVector p0(m_alignmentMatrix.UntransformVector(localP0 * m_invScale));
				ndVector p1(m_alignmentMatrix.UntransformVector(localP1 * m_invScale));
				t = m_shape->RayCast(callback, p0, p1, ndFloat32(1.0f), body, contactOut);
				if (t < ndFloat32(1.0f))
				{
					dAssert(!((ndShape*)m_shape)->GetAsShapeCompound());
					ndVector normal(m_alignmentMatrix.RotateVector(m_invScale * contactOut.m_normal));
					contactOut.m_normal = normal.Normalize();
					contactOut.m_shapeInstance0 = this;
					contactOut.m_shapeInstance1 = this;
				}
				break;
			}
		}
	}
	return t;
}

ndInt32 ndShapeInstance::CalculatePlaneIntersection(const ndVector& normal, const ndVector& point, ndVector* const contactsOut) const
{
	ndInt32 count = 0;
	dAssert(normal.m_w == ndFloat32(0.0f));
	switch (m_scaleType)
	{
		case m_unit:
		{
			count = m_shape->CalculatePlaneIntersection(normal, point, contactsOut);
			break;
		}
		case m_uniform:
		{
			ndVector point1(m_invScale * point);
			count = m_shape->CalculatePlaneIntersection(normal, point1, contactsOut);
			for (ndInt32 i = 0; i < count; i++) {
				contactsOut[i] = m_scale * contactsOut[i];
			}
			break;
		}

		case m_nonUniform:
		{
			// support((p * S), n) = S * support (p, n * transp(S)) 
			ndVector point1(m_invScale * point);
			ndVector normal1(m_scale * normal);
			normal1 = normal1.Normalize();
			count = m_shape->CalculatePlaneIntersection(normal1, point1, contactsOut);
			for (ndInt32 i = 0; i < count; i++) {
				contactsOut[i] = m_scale * contactsOut[i];
			}
			break;
		}

		case m_global:
		default:
		{
			ndVector point1(m_alignmentMatrix.UntransformVector(m_invScale * point));
			//ndVector normal1(m_alignmentMatrix.UntransformVector(m_scale * normal));
			ndVector normal1(m_alignmentMatrix.UnrotateVector(m_scale * normal));
			normal1 = normal1.Normalize();
			count = m_shape->CalculatePlaneIntersection(normal1, point1, contactsOut);
			for (ndInt32 i = 0; i < count; i++) {
				contactsOut[i] = m_scale * m_alignmentMatrix.TransformVector(contactsOut[i]);
			}
		}
	}
	return count;
}

void ndShapeInstance::SetScale(const ndVector& scale)
{
	ndFloat32 scaleX = dAbs(scale.m_x);
	ndFloat32 scaleY = dAbs(scale.m_y);
	ndFloat32 scaleZ = dAbs(scale.m_z);
	dAssert(scaleX > ndFloat32(0.0f));
	dAssert(scaleY > ndFloat32(0.0f));
	dAssert(scaleZ > ndFloat32(0.0f));

	if (((ndShape*)m_shape)->GetAsShapeCompound())
	{
		dAssert(m_scaleType == m_unit);
		ndShapeCompound* const compound = ((ndShape*)m_shape)->GetAsShapeCompound();
		compound->ApplyScale(scale);
	}
	else if ((dAbs(scaleX - scaleY) < ndFloat32(1.0e-4f)) && (dAbs(scaleX - scaleZ) < ndFloat32(1.0e-4f))) 
	{
		if ((dAbs(scaleX - ndFloat32(1.0f)) < ndFloat32(1.0e-4f))) 
		{
			m_scaleType = m_unit;
			m_scale = ndVector(ndFloat32(1.0f), ndFloat32(1.0f), ndFloat32(1.0f), ndFloat32(0.0f));
			m_maxScale = m_scale;
			m_invScale = m_scale;
		}
		else 
		{
			m_scaleType = m_uniform;
			m_scale = ndVector(scaleX, scaleX, scaleX, ndFloat32(0.0f));
			m_maxScale = m_scale;
			m_invScale = ndVector(ndFloat32(1.0f) / scaleX, ndFloat32(1.0f) / scaleX, ndFloat32(1.0f) / scaleX, ndFloat32(0.0f));
		}
	}
	else 
	{
		m_scaleType = m_nonUniform;
		m_maxScale = dMax(dMax(scaleX, scaleY), scaleZ);
		m_scale = ndVector(scaleX, scaleY, scaleZ, ndFloat32(0.0f));
		m_invScale = ndVector(ndFloat32(1.0f) / scaleX, ndFloat32(1.0f) / scaleY, ndFloat32(1.0f) / scaleZ, ndFloat32(0.0f));
	}
}

void ndShapeInstance::SetGlobalScale(const ndMatrix& scaleMatrix)
{
	const ndMatrix matrix(scaleMatrix * m_localMatrix);

	ndVector scale;
	matrix.PolarDecomposition(m_localMatrix, scale, m_alignmentMatrix);
	bool uniform = (dAbs(scale[0] - scale[1]) < ndFloat32(1.0e-4f)) && (dAbs(scale[0] - scale[2]) < ndFloat32(1.0e-4f));
	if (uniform) 
	{
		SetScale(scale);
	}
	else
	{
		bool isIdentity = m_alignmentMatrix.TestIdentity();
		m_scaleType = isIdentity ? m_nonUniform : m_global;
		m_scale = scale;
		m_invScale = ndVector(ndFloat32(1.0f) / m_scale[0], ndFloat32(1.0f) / m_scale[1], ndFloat32(1.0f) / m_scale[2], ndFloat32(0.0f));
	}
}

void ndShapeInstance::SetGlobalScale(const ndVector& scale)
{
	ndMatrix matrix(dGetIdentityMatrix());
	matrix[0][0] = scale.m_x;
	matrix[1][1] = scale.m_y;
	matrix[2][2] = scale.m_z;
	SetGlobalScale(matrix);
}

ndFloat32 ndShapeInstance::CalculateBuoyancyCenterOfPresure(ndVector& com, const ndMatrix& matrix, const ndVector& fluidPlane) const
{
	com = m_shape->CalculateVolumeIntegral(m_localMatrix * matrix, fluidPlane, *this);
	ndFloat32 volume = com.m_w;
	com.m_w = ndFloat32(0.0f);
	return volume;
}

ndVector ndShapeInstance::GetBoxPadding()
{
	return m_padding;
}

bool ndShapeInstance::ndDistanceCalculator::ClosestPoint()
{
	ndContact contact;
	ndBodyKinematic body0;
	ndBodyKinematic body1;

	body0.SetCollisionShape(*m_shape0);
	body1.SetCollisionShape(*m_shape1);

	ndMatrix matrix0(m_matrix0);
	ndMatrix matrix1(m_matrix1);

	matrix0.m_posit = ndVector::m_wOne;
	matrix1.m_posit = (m_matrix1.m_posit - m_matrix0.m_posit) | ndVector::m_wOne;

	body0.SetMatrix(matrix0);
	body1.SetMatrix(matrix1);
	body0.SetMassMatrix(ndVector::m_one);
	contact.SetBodies(&body0, &body1);

	ndShapeInstance& shape0 = body0.GetCollisionShape();
	ndShapeInstance& shape1 = body1.GetCollisionShape();
	shape0.SetGlobalMatrix(shape0.GetLocalMatrix() * body0.GetMatrix());
	shape1.SetGlobalMatrix(shape1.GetLocalMatrix() * body1.GetMatrix());

	ndContactSolver solver(&contact, m_scene->GetContactNotify(), m_scene->GetTimestep(), 0);
	bool ret = solver.CalculateClosestPoints();

	m_normal = solver.m_separatingVector;
	m_point0 = solver.m_closestPoint0 + m_matrix0.m_posit;
	m_point1 = solver.m_closestPoint1 + m_matrix0.m_posit;
	m_point0.m_w = ndFloat32(1.0f);
	m_point1.m_w = ndFloat32(1.0f);
	return ret;
}

void ndShapeInstance::CalculateAabb(const ndMatrix& matrix, ndVector& p0, ndVector& p1) const
{
#if 0
	m_shape->CalculateAabb(matrix, p0, p1);
	switch (m_scaleType)
	{
		case m_unit:
		{
			p0 -= m_padding;
			p1 += m_padding;
			break;
		}

		case m_uniform:
		case m_nonUniform:
		{
			ndMatrix matrix1(matrix);
			matrix1[0] = matrix1[0].Scale(m_scale.m_x);
			matrix1[1] = matrix1[1].Scale(m_scale.m_y);
			matrix1[2] = matrix1[2].Scale(m_scale.m_z);
			matrix1 = matrix.Inverse() * matrix1;

			ndVector size0(ndVector::m_half * (p1 - p0));
			ndVector origin(matrix1.TransformVector(ndVector::m_half * (p0 + p1)));
			ndVector size(matrix1.m_front.Abs().Scale(size0.m_x) + matrix1.m_up.Abs().Scale(size0.m_y) + matrix1.m_right.Abs().Scale(size0.m_z));

			p0 = (origin - size - m_padding) & ndVector::m_triplexMask;
			p1 = (origin + size + m_padding) & ndVector::m_triplexMask;
			break;
		}

		case m_global:
		default:
		{
			ndMatrix matrix1(matrix);
			matrix1[0] = matrix1[0].Scale(m_scale.m_x);
			matrix1[1] = matrix1[1].Scale(m_scale.m_y);
			matrix1[2] = matrix1[2].Scale(m_scale.m_z);
			matrix1 = matrix.Inverse() * m_alignmentMatrix * matrix1;

			ndVector size0(ndVector::m_half * (p1 - p0));
			ndVector origin(matrix1.TransformVector(ndVector::m_half * (p0 + p1)));
			ndVector size(matrix1.m_front.Abs().Scale(size0.m_x) + matrix1.m_up.Abs().Scale(size0.m_y) + matrix1.m_right.Abs().Scale(size0.m_z));

			p0 = (origin - size - m_padding) & ndVector::m_triplexMask;
			p1 = (origin + size + m_padding) & ndVector::m_triplexMask;

			break;
		}
	}

#else
	ndMatrix scaleMatrix;
	scaleMatrix[0] = matrix[0].Scale(m_scale.m_x);
	scaleMatrix[1] = matrix[1].Scale(m_scale.m_y);
	scaleMatrix[2] = matrix[2].Scale(m_scale.m_z);
	scaleMatrix[3] = matrix[3];
	scaleMatrix = m_alignmentMatrix * scaleMatrix;

	const ndVector size0(m_shape->GetObbSize());
	const ndVector origin(scaleMatrix.TransformVector(m_shape->GetObbOrigin()));
	const ndVector size(scaleMatrix.m_front.Abs().Scale(size0.m_x) + scaleMatrix.m_up.Abs().Scale(size0.m_y) + scaleMatrix.m_right.Abs().Scale(size0.m_z));

	p0 = (origin - size - m_padding) & ndVector::m_triplexMask;
	p1 = (origin + size + m_padding) & ndVector::m_triplexMask;
#endif
	dAssert(p0.m_w == ndFloat32(0.0f));
	dAssert(p1.m_w == ndFloat32(0.0f));
}