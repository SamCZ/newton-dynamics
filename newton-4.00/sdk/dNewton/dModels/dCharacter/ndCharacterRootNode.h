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

#ifndef __D_CHARACTER_ROOT_NODE_H__
#define __D_CHARACTER_ROOT_NODE_H__

#include "ndNewtonStdafx.h"
#include "ndCharacterLimbNode.h"

class ndCharacterRootNode: public ndCharacterLimbNode
{
	public:
	D_CLASS_RELECTION(ndCharacterRootNode);

	D_NEWTON_API ndCharacterRootNode(ndCharacter* const owner, ndBodyDynamic* const body);
	D_NEWTON_API virtual ~ndCharacterRootNode ();

	ndCharacter* GetOwner() const;
	virtual ndBodyDynamic* GetBody() const;

	const dVector& GetGravityDir() const;
	void SetGravityDir(const dVector& dir);

	const dMatrix& GetLocalFrame() const;
	D_NEWTON_API void SetLocalFrame(const dMatrix& frameInGlobalSpace);

	protected:
	void UpdateGlobalPose(ndWorld* const world, dFloat32 timestep);

	dMatrix m_localFrame;
	dVector m_gravityDir;
	ndCharacter* m_owner;
	ndBodyDynamic* m_body;
};

inline ndCharacter* ndCharacterRootNode::GetOwner() const
{
	return m_owner;
}

inline ndBodyDynamic* ndCharacterRootNode::GetBody() const
{
	return m_body;
}

inline const dMatrix& ndCharacterRootNode::GetLocalFrame() const
{
	return m_localFrame;
}

inline const dVector& ndCharacterRootNode::GetGravityDir() const
{
	return m_gravityDir;
}

inline void ndCharacterRootNode::SetGravityDir(const dVector& dir)
{
	m_gravityDir = dir;
}


#endif