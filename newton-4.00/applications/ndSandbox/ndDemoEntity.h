/* Copyright (c) <2003-2019> <Newton Game Dynamics>
* 
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
* 
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely
*/

#ifndef __DEMO_ENTITY_H__
#define __DEMO_ENTITY_H__

class ndShaderPrograms;
class ndDemoMeshInterface;
class ndDemoEntityManager;

class ndDemoEntity: public dNodeHierarchy<ndDemoEntity>
{
#if 0
	public:
	class UserData
	{
		public:
		UserData()
		{
		}

		virtual ~UserData()
		{
		}
		
		virtual void OnRender (dFloat32 timestep) const = 0;
	};
	
	ndDemoEntity(ndDemoEntityManager& world, const dScene* const scene, dScene::dTreeNode* const rootSceneNode, dTree<DemoMeshInterface*, dScene::dTreeNode*>& meshCache, ndDemoEntityManager::EntityDictionary& entityDictionary, ndDemoEntity* const parent = NULL);

	DemoMeshInterface* GetMesh() const;
	void SetMesh (DemoMeshInterface* const m_mesh, const dMatrix& meshMatrix);

	const dMatrix& GetMeshMatrix() const;  
	void SetMeshMatrix(const dMatrix& matrix);  

	UserData* GetUserData ();
	void SetUserData (UserData* const data);
	
	static ndDemoEntity* LoadNGD_mesh (const char* const fileName, NewtonWorld* const world, const ShaderPrograms& shaderCache);

	const dMatrix& GetRenderMatrix () const;
	dMatrix CalculateGlobalMatrix (const ndDemoEntity* const root = NULL) const;

	dMatrix GetNextMatrix () const;
	dMatrix GetCurrentMatrix () const;
	virtual void SetNextMatrix (ndDemoEntityManager& world, const dQuaternion& rotation, const dVector& position);

	void InterpolateMatrixUnsafe(dFloat32 param);

	virtual void ResetMatrix(ndDemoEntityManager& world, const dMatrix& matrix);
	virtual void InterpolateMatrix (ndDemoEntityManager& world, dFloat32 param);
	dMatrix CalculateInterpolatedGlobalMatrix (const ndDemoEntity* const root = NULL) const;

	void RenderBone() const;
	NewtonCollision* CreateCollisionFromchildren(NewtonWorld* const world) const;

	static void TransformCallback(const NewtonBody* body, const dFloat32* matrix, int threadIndex);

	protected:
	mutable dMatrix m_matrix;			// interpolated matrix

	dMatrix m_meshMatrix;
	DemoMeshInterface* m_mesh;
	UserData* m_userData;
	
	bool m_isVisible;
	dAddRtti(dClassInfo,DOMMY_API);
#endif

	public:
	ndDemoEntity(const ndDemoEntity& copyFrom);
	ndDemoEntity(const dMatrix& matrix, ndDemoEntity* const parent);
	virtual ~ndDemoEntity(void);

	dNodeBaseHierarchy* CreateClone() const;

	void SetMatrixUsafe(const dQuaternion& rotation, const dVector& position);
	virtual void SetMatrix(ndDemoEntityManager& world, const dQuaternion& rotation, const dVector& position);

	virtual void Render(dFloat32 timeStep, ndDemoEntityManager* const scene, const dMatrix& matrix) const;

	// interpolated matrix
	dMatrix m_matrix;
	dVector m_curPosition;				// position one physics simulation step in the future
	dVector m_nextPosition;             // position at the current physics simulation step
	dQuaternion m_curRotation;          // rotation one physics simulation step in the future  
	dQuaternion m_nextRotation;         // rotation at the current physics simulation step  

	dSpinLock m_lock;

	dList <ndDemoEntity*>::dListNode* m_rootNode;
	friend class ndDemoEntityManager;
};


#endif