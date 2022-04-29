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

#ifndef __ND_CONVEXHULL_3D__
#define __ND_CONVEXHULL_3D__

#include "vhacdConvexHullUtils.h"

namespace nd
{
	namespace VHACD
	{
		class ConvexHullVertex;
		class ConvexHullAABBTreeNode;

		class ConvexHullFace
		{
			public:
			ConvexHullFace();
			double Evalue(const hullVector* const pointArray, const hullVector& point) const;
			hullPlane GetPlaneEquation(const hullVector* const pointArray, bool& isValid) const;

			public:
			int m_index[3];

			private:
			int m_mark;
			List<ConvexHullFace>::ndNode* m_twin[3];

			friend class ConvexHull;
		};

		class ConvexHull : public List<ConvexHullFace>
		{
			class ndNormalMap;

			public:
			ConvexHull(const ConvexHull& source);
			ConvexHull(const double* const vertexCloud, int strideInBytes, int count, double distTol, int maxVertexCount = 0x7fffffff);
			~ConvexHull();

			const std::vector<hullVector>& GetVertexPool() const;

			private:
			void BuildHull(const double* const vertexCloud, int strideInBytes, int count, double distTol, int maxVertexCount);

			void GetUniquePoints(std::vector<ConvexHullVertex>& points);
			int InitVertexArray(std::vector<ConvexHullVertex>& points, void* const memoryPool, int maxMemSize);

			ConvexHullAABBTreeNode* BuildTreeNew(std::vector<ConvexHullVertex>& points, char** const memoryPool, int& maxMemSize) const;
			ConvexHullAABBTreeNode* BuildTreeOld(std::vector<ConvexHullVertex>& points, char** const memoryPool, int& maxMemSize);
			ConvexHullAABBTreeNode* BuildTreeRecurse(ConvexHullAABBTreeNode* const parent, ConvexHullVertex* const points, int count, int baseIndex, char** const memoryPool, int& maxMemSize) const;

			ndNode* AddFace(int i0, int i1, int i2);

			void CalculateConvexHull3d(ConvexHullAABBTreeNode* vertexTree, std::vector<ConvexHullVertex>& points, int count, double distTol, int maxVertexCount);

			int SupportVertex(ConvexHullAABBTreeNode** const tree, const std::vector<ConvexHullVertex>& points, const hullVector& dir, const bool removeEntry = true) const;
			double TetrahedrumVolume(const hullVector& p0, const hullVector& p1, const hullVector& p2, const hullVector& p3) const;

			hullVector m_aabbP0;
			hullVector m_aabbP1;
			double m_diag;
			std::vector<hullVector> m_points;
		};
	}
}
#endif
