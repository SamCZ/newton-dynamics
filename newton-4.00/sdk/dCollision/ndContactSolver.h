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

#ifndef __D_CONTACT_SOLVER_H__
#define __D_CONTACT_SOLVER_H__

#include "ndCollisionStdafx.h"
#include "ndShape.h"
#include "ndShapeInstance.h"

class dPlane;
class ndBodyKinematic;
class ndContactNotify;
class ndPolygonMeshDesc;

D_MSV_NEWTON_ALIGN_32
class ndMinkFace
{
	public:
	dPlane m_plane;
	ndMinkFace* m_twin[3];
	dInt16 m_vertex[3];
	dInt8 m_mark;
	dInt8 m_alive;
} D_GCC_NEWTON_ALIGN_32 ;

#define D_SEPARATION_PLANES_ITERATIONS	8
#define D_CONVEX_MINK_STACK_SIZE		64
#define D_CONNICS_CONTATS_ITERATIONS	32
#define D_CONVEX_MINK_MAX_FACES			512
#define D_CONVEX_MINK_MAX_POINTS		256
#define D_MAX_EDGE_COUNT				2048
#define D_PENETRATION_TOL				dFloat32 (1.0f / 1024.0f)
#define D_MINK_VERTEX_ERR				dFloat32 (1.0e-3f)
#define D_MINK_VERTEX_ERR2				(D_MINK_VERTEX_ERR * D_MINK_VERTEX_ERR)

class ndContact;
class dCollisionParamProxy;

D_MSV_NEWTON_ALIGN_32
class ndContactSolver: public dDownHeap<ndMinkFace *, dFloat32>  
{
	public: 
	class ndBoxBoxDistance2;
	ndContactSolver(ndContact* const contact, ndContactNotify* const notification, dFloat32 timestep);
	ndContactSolver(ndShapeInstance* const instance, ndContactNotify* const notification, dFloat32 timestep);
	ndContactSolver(const ndContactSolver& src, const ndShapeInstance& instance0, const ndShapeInstance& instance1);
	~ndContactSolver() {}

	dInt32 CalculateContactsDiscrete(); // done
	dInt32 CalculateContactsContinue(); // done

	dFloat32 RayCast (const dVector& localP0, const dVector& localP1, ndContactPoint& contactOut);
	
	private:
	dInt32 ConvexContactsDiscrete(); // done
	dInt32 CompoundContactsDiscrete(); // done
	dInt32 ConvexToConvexContactsDiscrete(); // done
	dInt32 ConvexToCompoundContactsDiscrete(); // done
	dInt32 CompoundToConvexContactsDiscrete(); // done
	dInt32 CompoundToCompoundContactsDiscrete(); // done
	dInt32 ConvexToStaticMeshContactsDiscrete(); // done
	dInt32 CompoundToShapeStaticBvhContactsDiscrete(); // done
	dInt32 CompoundToStaticHeightfieldContactsDiscrete(); // done
	dInt32 CalculatePolySoupToHullContactsDescrete(ndPolygonMeshDesc& data); // done
	dInt32 ConvexToSaticStaticBvhContactsNodeDescrete(const dAabbPolygonSoup::dNode* const node); // done

	dInt32 ConvexContactsContinue(); // done
	dInt32 CompoundContactsContinue(); // done
	dInt32 ConvexToConvexContactsContinue(); // done
	dInt32 ConvexToCompoundContactsContinue(); // done
	dInt32 ConvexToStaticMeshContactsContinue(); // done
	dInt32 CalculatePolySoupToHullContactsContinue(ndPolygonMeshDesc& data); // done

	class dgPerimenterEdge
	{
		public:
		const dVector* m_vertex;
		dgPerimenterEdge* m_next;
		dgPerimenterEdge* m_prev;
	};

	class dgFaceFreeList
	{
		public:
		dgFaceFreeList* m_next;
	};

	inline ndMinkFace* NewFace();
	inline void PushFace(ndMinkFace* const face);
	inline void DeleteFace(ndMinkFace* const face);
	inline ndMinkFace* AddFace(dInt32 v0, dInt32 v1, dInt32 v2);

	bool CalculateClosestPoints();
	dInt32 CalculateClosestSimplex();
	
	dInt32 CalculateIntersectingPlane(dInt32 count);
	dInt32 PruneContacts(dInt32 count, dInt32 maxCount) const;
	dInt32 PruneSupport(dInt32 count, const dVector& dir, const dVector* const points) const;
	dInt32 CalculateContacts(const dVector& point0, const dVector& point1, const dVector& normal);
	dInt32 Prune2dContacts(const dMatrix& matrix, dInt32 count, ndContactPoint* const contactArray, dInt32 maxCount) const;
	dInt32 Prune3dContacts(const dMatrix& matrix, dInt32 count, ndContactPoint* const contactArray, dInt32 maxCount) const;
	dInt32 ConvexPolygonsIntersection(const dVector& normal, dInt32 count1, dVector* const shape1, dInt32 count2, dVector* const shape2, dVector* const contactOut, dInt32 maxContacts) const;
	dInt32 ConvexPolygonToLineIntersection(const dVector& normal, dInt32 count1, dVector* const shape1, dInt32 count2, dVector* const shape2, dVector* const contactOut, dVector* const mem) const;

	dBigVector ReduceLine(dInt32& indexOut);
	dBigVector ReduceTriangle (dInt32& indexOut);
	dBigVector ReduceTetrahedrum (dInt32& indexOut);
	void SupportVertex(const dVector& dir, dInt32 vertexIndex);

	void TranslateSimplex(const dVector& step);
	void CalculateContactFromFeacture(dInt32 featureType);

	ndShapeInstance m_instance0;
	ndShapeInstance m_instance1;
	dVector m_closestPoint0;
	dVector m_closestPoint1;
	dVector m_separatingVector;
	union
	{
		dVector m_buffer[2 * D_CONVEX_MINK_MAX_POINTS];
		struct
		{
			dVector m_hullDiff[D_CONVEX_MINK_MAX_POINTS];
			dVector m_hullSum[D_CONVEX_MINK_MAX_POINTS];
		};
	};

	ndContact* m_contact;
	dgFaceFreeList* m_freeFace;
	ndContactNotify* m_notification;
	ndContactPoint* m_contactBuffer;
	dFloat32 m_timestep;
	dFloat32 m_skinThickness;
	dFloat32 m_separationDistance;

	dInt32 m_maxCount;
	dInt32 m_vertexIndex;
	dUnsigned32 m_pruneContacts			: 1;
	dUnsigned32 m_intersectionTestOnly	: 1;
	
	dInt32 m_faceIndex;
	ndMinkFace* m_faceStack[D_CONVEX_MINK_STACK_SIZE];
	ndMinkFace* m_coneFaceList[D_CONVEX_MINK_STACK_SIZE];
	ndMinkFace* m_deletedFaceList[D_CONVEX_MINK_STACK_SIZE];
	ndMinkFace m_facePool[D_CONVEX_MINK_MAX_FACES];
	dInt8 m_heapBuffer[D_CONVEX_MINK_MAX_FACES * (sizeof (dFloat32) + sizeof (ndMinkFace *))];

	static dVector m_pruneUpDir;
	static dVector m_pruneSupportX;

	static dVector m_hullDirs[14]; 
	static dInt32 m_rayCastSimplex[4][4];

	friend class ndScene;
	friend class ndShapeInstance;
	friend class ndPolygonMeshDesc;
	friend class ndConvexCastNotify;
	friend class ndShapeConvexPolygon;
	friend class ndBodyPlayerCapsuleContactSolver;
} D_GCC_NEWTON_ALIGN_32;

#endif 


