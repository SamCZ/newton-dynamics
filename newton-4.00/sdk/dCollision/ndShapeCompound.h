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

#ifndef __D_SHAPE_COMPOUND_H__
#define __D_SHAPE_COMPOUND_H__

#include "ndCollisionStdafx.h"
#include "ndShape.h"

class ndBodyKinematic;

#define D_COMPOUND_STACK_DEPTH	256

class ndShapeCompound: public ndShape
{
	public:
	enum ndNodeType
	{
		m_leaf,
		m_node,
	};

	class ndNodeBase;
	class ndTreeArray : public dTree<ndNodeBase*, dInt32, dContainersFreeListAlloc<ndNodeBase*>>
	{
		public:
		ndTreeArray();
		void AddNode(ndNodeBase* const node, dInt32 index, const ndShapeInstance* const parent);
	};

	D_CLASS_REFLECTION(ndShapeCompound);
	D_COLLISION_API ndShapeCompound();
	D_COLLISION_API ndShapeCompound(const dLoadSaveBase::dLoadDescriptor& desc);
	D_COLLISION_API virtual ~ndShapeCompound();

	void SetOwner(const ndShapeInstance* const myInstance);

	const ndTreeArray& GetTree() const;

	D_COLLISION_API virtual void BeginAddRemove();
	D_COLLISION_API virtual ndTreeArray::dNode* AddCollision(ndShapeInstance* const part);
	D_COLLISION_API virtual void EndAddRemove();

	protected:
	class ndSpliteInfo;
	ndShapeCompound(const ndShapeCompound& source, const ndShapeInstance* const myInstance);
	virtual ndShapeInfo GetShapeInfo() const;
	virtual void DebugShape(const dMatrix& matrix, ndShapeDebugCallback& debugCallback) const;
	virtual dFloat32 RayCast(ndRayCastNotify& callback, const dVector& localP0, const dVector& localP1, dFloat32 maxT, const ndBody* const body, ndContactPoint& contactOut) const;
	virtual void Save(const dLoadSaveBase::dSaveDescriptor& desc) const;

	virtual dFloat32 GetVolume() const;
	virtual dFloat32 GetBoxMinRadius() const;
	virtual dFloat32 GetBoxMaxRadius() const;

	virtual ndShapeCompound* GetAsShapeCompound();
	virtual dVector SupportVertex(const dVector& dir, dInt32* const vertexIndex) const;
	virtual dVector SupportVertexSpecial(const dVector& dir, dFloat32 skinThickness, dInt32* const vertexIndex) const;
	virtual dVector SupportVertexSpecialProjectPoint(const dVector& point, const dVector& dir) const;
	virtual dInt32 CalculatePlaneIntersection(const dVector& normal, const dVector& point, dVector* const contactsOut) const;
	virtual dVector CalculateVolumeIntegral(const dMatrix& globalMatrix, const dVector& plane, const ndShapeInstance& parentScale) const;
	
	D_COLLISION_API virtual void CalculateAabb(const dMatrix& matrix, dVector& p0, dVector& p1) const;
	//D_COLLISION_API dInt32 CalculatePlaneIntersection(const dFloat32* const vertex, const dInt32* const index, dInt32 indexCount, dInt32 strideInFloat, const dPlane& localPlane, dVector* const contactsOut) const;

	virtual void MassProperties();
	void ApplyScale(const dVector& scale);
	void SetSubShapeOwner(ndBodyKinematic* const body);
	void ImproveNodeFitness(ndNodeBase* const node) const;
	dFloat64 CalculateEntropy(dInt32 count, ndNodeBase** array);
	static dInt32 CompareNodes(const ndNodeBase* const nodeA, const ndNodeBase* const nodeB, void*);
	ndNodeBase* BuildTopDown(ndNodeBase** const leafArray, dInt32 firstBox, dInt32 lastBox, ndNodeBase** rootNodesMemory, dInt32& rootIndex);
	ndNodeBase* BuildTopDownBig(ndNodeBase** const leafArray, dInt32 firstBox, dInt32 lastBox, ndNodeBase** rootNodesMemory, dInt32& rootIndex);
	dFloat32 CalculateSurfaceArea(ndNodeBase* const node0, ndNodeBase* const node1, dVector& minBox, dVector& maxBox) const;
	dMatrix CalculateInertiaAndCenterOfMass(const dMatrix& alignMatrix, const dVector& localScale, const dMatrix& matrix) const;
	dFloat32 CalculateMassProperties(const dMatrix& offset, dVector& inertia, dVector& crossInertia, dVector& centerOfMass) const;

	ndTreeArray m_array;
	dFloat64 m_treeEntropy;
	dFloat32 m_boxMinRadius;
	dFloat32 m_boxMaxRadius;
	ndNodeBase* m_root;
	const ndShapeInstance* m_myInstance;
	dInt32 m_idIndex;

	friend class ndBodyKinematic;
	friend class ndShapeInstance;
	friend class ndContactSolver;
};

inline ndShapeCompound* ndShapeCompound::GetAsShapeCompound()
{ 
	return this; 
}

inline void ndShapeCompound::SetOwner(const ndShapeInstance* const instance)
{
	m_myInstance = instance;
}

class ndShapeCompound::ndNodeBase: public dClassAlloc
{
	public:
	ndNodeBase();
	ndNodeBase(const ndNodeBase& copyFrom);
	ndNodeBase(ndShapeInstance* const instance);
	ndNodeBase(ndNodeBase* const left, ndNodeBase* const right);
	~ndNodeBase();

	//void Sanity(int level = 0);
	void CalculateAABB();
	ndShapeInstance* GetShape() const;
	void SetBox(const dVector& p0, const dVector& p1);

	dVector m_p0;
	dVector m_p1;
	dVector m_size;
	dVector m_origin;
	dFloat32 m_area;
	dInt32 m_type;
	ndNodeBase* m_left;
	ndNodeBase* m_right;
	ndNodeBase* m_parent;
	ndTreeArray::dNode* m_myNode;
	ndShapeInstance* m_shapeInstance;
};

inline ndShapeCompound::ndNodeBase::ndNodeBase()
	:dClassAlloc()
	,m_type(m_node)
	,m_left(nullptr)
	,m_right(nullptr)
	,m_parent(nullptr)
	,m_myNode(nullptr)
	,m_shapeInstance(nullptr)
{
}

inline ndShapeCompound::ndNodeBase::ndNodeBase(const ndNodeBase& copyFrom)
	:dClassAlloc()
	,m_p0(copyFrom.m_p0)
	,m_p1(copyFrom.m_p1)
	,m_size(copyFrom.m_size)
	,m_origin(copyFrom.m_origin)
	,m_area(copyFrom.m_area)
	,m_type(copyFrom.m_type)
	,m_left(nullptr)
	,m_right(nullptr)
	,m_parent(nullptr)
	,m_myNode(nullptr)
	,m_shapeInstance(nullptr)
{
	dAssert(!copyFrom.m_shapeInstance);
}

inline ndShapeCompound::ndNodeBase::ndNodeBase(ndShapeInstance* const instance)
	:dClassAlloc()
	,m_type(m_leaf)
	,m_left(nullptr)
	,m_right(nullptr)
	,m_parent(nullptr)
	,m_myNode(nullptr)
	,m_shapeInstance(new ndShapeInstance(*instance))
{
	CalculateAABB();
}

inline ndShapeCompound::ndNodeBase::ndNodeBase(ndNodeBase* const left, ndNodeBase* const right)
	:dClassAlloc()
	,m_type(m_node)
	,m_left(left)
	,m_right(right)
	,m_parent(nullptr)
	,m_myNode(nullptr)
	,m_shapeInstance(nullptr)
{
	m_left->m_parent = this;
	m_right->m_parent = this;

	dVector p0(left->m_p0.GetMin(right->m_p0));
	dVector p1(left->m_p1.GetMax(right->m_p1));
	SetBox(p0, p1);
}

inline ndShapeInstance* ndShapeCompound::ndNodeBase::GetShape() const
{
	return m_shapeInstance;
}

inline void ndShapeCompound::ndNodeBase::CalculateAABB()
{
	dVector p0;
	dVector p1;
	m_shapeInstance->CalculateAabb(m_shapeInstance->GetLocalMatrix(), p0, p1);
	SetBox(p0, p1);
}

inline void ndShapeCompound::ndNodeBase::SetBox(const dVector& p0, const dVector& p1)
{
	m_p0 = p0;
	m_p1 = p1;
	dAssert(m_p0.m_w == dFloat32(0.0f));
	dAssert(m_p1.m_w == dFloat32(0.0f));
	m_size = dVector::m_half * (m_p1 - m_p0);
	m_origin = dVector::m_half * (m_p1 + m_p0);
	m_area = m_size.DotProduct(m_size.ShiftTripleRight()).m_x;
}

inline const ndShapeCompound::ndTreeArray& ndShapeCompound::GetTree() const
{
	return m_array;
}

#endif 



