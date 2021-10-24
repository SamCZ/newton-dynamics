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

#ifndef __D_SHAPE_H__ 
#define __D_SHAPE_H__ 

#include "ndCollisionStdafx.h"

class ndBody;
class ndShape;
class ndShapeBox;
class ndShapeNull;
class ndShapeCone;
class ndShapePoint;
class ndShapeConvex;
class ndShapeSphere;
class ndShapeCapsule;
class ndContactPoint;
class ndShapeCompound;
class ndShapeCylinder;
class ndShapeCompound;
class ndRayCastNotify;
class ndShapeInstance;
class ndShapeStatic_bvh;
class ndShapeStaticMesh;
class ndShapeHeightfield;
class ndShapeConvexPolygon;
class ndShapeDebugCallback;
class ndShapeChamferCylinder;
class ndShapeStaticProceduralMesh;


enum ndShapeID
{
	// do not change the order of these enum
	m_box = 0,
	m_cone,
	m_sphere,
	m_capsule,
	m_cylinder,
	m_chamferCylinder,
	m_convexHull, // this must be the last convex shape ID

	// special and non convex collisions.
	m_compound,
	m_staticMesh,
	m_heightField,
	m_staticProceduralMesh,
	m_nullCollision,
	m_pointCollision,
	m_polygonCollision,
	m_boundingBoxHierachy,
	
	//m_deformableClothPatch,
	//m_deformableSolidMesh,
	// these are for internal use only	
	//m_contactCloud,
	//m_lumpedMassCollision
};

class ndShapeMaterial
{
	public:
	ndShapeMaterial()
	{
		memset(this, 0, sizeof(ndShapeMaterial));
	}

	dInt64 m_userId;
	union nData
	{
		void* m_userData;
		dUnsigned64 m_alignPad;
	} m_data;

	union dExtraData
	{
		dUnsigned64 m_intData;
		dFloat32 m_floatData;
	} m_userParam[6];
};

struct ndBoxInfo
{
	dFloat32 m_x;
	dFloat32 m_y;
	dFloat32 m_z;
};

struct ndPointInfo
{
	dFloat32 m_noUsed;
};

struct ndSphereInfo
{
	dFloat32 m_radius;
};

struct ndCylinderInfo
{
	dFloat32 m_radio0;
	dFloat32 m_radio1;
	dFloat32 m_height;
};

struct ndCapsuleInfo
{
	dFloat32 m_radio0;
	dFloat32 m_radio1;
	dFloat32 m_height;
};

struct ndConeInfo
{
	dFloat32 m_radius;
	dFloat32 m_height;
};

struct ndChamferCylinderInfo
{
	dFloat32 m_r;
	dFloat32 m_height;
};

struct ndConvexHullInfo
{
	dInt32 m_vertexCount;
	dInt32 m_strideInBytes;
	dInt32 m_faceCount;
	dVector* m_vertex;
};

struct ndCoumpoundInfo
{
	dInt32 m_noUsed;
};

struct ndProceduralInfoInfo
{
	dInt32 m_noUsed;
};

struct ndCollisionBvhInfo
{
	dInt32 m_vertexCount;
	dInt32 m_indexCount;
};

struct ndHeighfieldInfo
{
	dInt32 m_width;
	dInt32 m_height;
	dInt32 m_gridsDiagonals;
	dFloat32 m_verticalScale;
	dFloat32 m_horizonalScale_x;
	dFloat32 m_horizonalScale_z;
	dInt16* m_elevation;
	dInt8* m_atributes;
};

D_MSV_NEWTON_ALIGN_32
class ndShapeInfo
{
	public:


	dMatrix m_offsetMatrix;
	dVector m_scale;
	ndShapeMaterial m_shapeMaterial;
	ndShapeID m_collisionType;
	union
	{
		ndBoxInfo m_box;
		ndConeInfo m_cone;
		ndPointInfo m_point;
		ndSphereInfo m_sphere;
		ndCapsuleInfo m_capsule;
		ndCollisionBvhInfo m_bvh;
		ndCylinderInfo m_cylinder;
		ndCoumpoundInfo m_compound;
		ndConvexHullInfo m_convexhull;
		ndHeighfieldInfo m_heightfield;
		ndProceduralInfoInfo m_procedural;
		ndChamferCylinderInfo m_chamferCylinder;
		
		dFloat32 m_paramArray[32];
	};
} D_GCC_NEWTON_ALIGN_32;

D_MSV_NEWTON_ALIGN_32
class ndShape: public dClassAlloc
{
	public:
	D_CLASS_REFLECTION(ndShape);
	dInt32 GetRefCount() const;
	virtual dInt32 Release() const;
	virtual const ndShape* AddRef() const;

	virtual ndShapeBox* GetAsShapeBox() { return nullptr; }
	virtual ndShapeNull* GetAsShapeNull() { return nullptr; }
	virtual ndShapeCone* GetAsShapeCone() { return nullptr; }
	virtual ndShapePoint* GetAsShapePoint() { return nullptr; }
	virtual ndShapeConvex* GetAsShapeConvex() { return nullptr; }
	virtual ndShapeSphere* GetAsShapeSphere() { return nullptr; }
	virtual ndShapeCapsule* GetAsShapeCapsule() { return nullptr; }
	virtual ndShapeCylinder* GetAsShapeCylinder() { return nullptr; }
	virtual ndShapeCompound* GetAsShapeCompound() { return nullptr; }
	virtual ndShapeStatic_bvh* GetAsShapeStaticBVH() { return nullptr; }
	virtual ndShapeStaticMesh* GetAsShapeStaticMesh() { return nullptr; }
	virtual ndShapeHeightfield* GetAsShapeHeightfield() { return nullptr; }
	virtual ndShapeConvexPolygon* GetAsShapeAsConvexPolygon() { return nullptr; }
	virtual ndShapeChamferCylinder* GetAsShapeChamferCylinder() { return nullptr; }
	virtual ndShapeStaticProceduralMesh* GetAsShapeStaticProceduralMesh() { return nullptr; }

	virtual dInt32 GetConvexVertexCount() const;

	dVector GetObbSize() const;
	dVector GetObbOrigin() const;
	dFloat32 GetUmbraClipSize() const;

	D_COLLISION_API virtual void MassProperties();

	virtual void DebugShape(const dMatrix& matrix, ndShapeDebugCallback& debugCallback) const = 0;

	virtual ndShapeInfo GetShapeInfo() const;
	virtual dFloat32 GetVolume() const = 0;
	virtual dFloat32 GetBoxMinRadius() const = 0;
	virtual dFloat32 GetBoxMaxRadius() const = 0;

	virtual void CalculateAabb(const dMatrix& matrix, dVector& p0, dVector& p1) const = 0;
	virtual dVector SupportVertex(const dVector& dir, dInt32* const vertexIndex) const = 0;
	virtual dVector SupportVertexSpecialProjectPoint(const dVector& point, const dVector& dir) const = 0;
	virtual dVector SupportVertexSpecial(const dVector& dir, dFloat32 skinSkinThickness, dInt32* const vertexIndex) const = 0;
	virtual dInt32 CalculatePlaneIntersection(const dVector& normal, const dVector& point, dVector* const contactsOut) const = 0;
	virtual dVector CalculateVolumeIntegral(const dMatrix& globalMatrix, const dVector& globalPlane, const ndShapeInstance& parentScale) const = 0;
	virtual dFloat32 RayCast(ndRayCastNotify& callback, const dVector& localP0, const dVector& localP1, dFloat32 maxT, const ndBody* const body, ndContactPoint& contactOut) const = 0;

	virtual dMatrix CalculateInertiaAndCenterOfMass(const dMatrix& alignMatrix, const dVector& localScale, const dMatrix& matrix) const;
	virtual dFloat32 CalculateMassProperties(const dMatrix& offset, dVector& inertia, dVector& crossInertia, dVector& centerOfMass) const;

	D_COLLISION_API virtual void Save(const dLoadSaveBase::dSaveDescriptor& desc) const;

	protected:
	D_COLLISION_API ndShape(ndShapeID id);
	D_COLLISION_API ndShape (const ndShape& source);
	D_COLLISION_API virtual ~ndShape();

	dVector m_inertia;	
	dVector m_crossInertia;	
	dVector m_centerOfMass;
	dVector m_boxSize;
	dVector m_boxOrigin;
	mutable dAtomic<dInt32> m_refCount;
	ndShapeID m_collisionId;
	static dVector m_flushZero;

} D_GCC_NEWTON_ALIGN_32;

inline dInt32 ndShape::GetConvexVertexCount() const
{
	return 0;
}

inline const ndShape* ndShape::AddRef() const
{
	m_refCount.fetch_add(1);
	return this;
}

inline dInt32 ndShape::Release() const
{
	dInt32 count = m_refCount.fetch_add(-1);
	dAssert(count >= 1);
	if (count == 1) 
	{
		delete this;
	}
	return count;
}

inline dInt32 ndShape::GetRefCount() const
{
	return m_refCount.load();
}

inline dFloat32 ndShape::CalculateMassProperties(const dMatrix&, dVector&, dVector&, dVector&) const
{ 
	dAssert(0); 
	return 0; 
}

inline dMatrix ndShape::CalculateInertiaAndCenterOfMass(const dMatrix&, const dVector&, const dMatrix&) const
{
	dAssert(0);
	return dGetZeroMatrix();
}

inline dVector ndShape::GetObbOrigin() const
{
	return m_boxOrigin;
}

inline dVector ndShape::GetObbSize() const
{
	return m_boxSize;
}

inline dFloat32 ndShape::GetUmbraClipSize() const
{
	return dFloat32(3.0f) * GetBoxMaxRadius();
}

#endif 


