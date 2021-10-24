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

#ifndef __D_POLYHEDRA_MASS_PROPERTIES_H__
#define __D_POLYHEDRA_MASS_PROPERTIES_H__

#include "dCoreStdafx.h"
#include "dTypes.h"
#include "dClassAlloc.h"

class dPolyhedraMassProperties: public dClassAlloc
{
	public:
	D_CORE_API dPolyhedraMassProperties();

	D_CORE_API void AddCGFace (dInt32 indexCount, const dVector* const faceVertex);
	D_CORE_API void AddInertiaFace (dInt32 indexCount, const dVector* const faceVertex);
	D_CORE_API void AddInertiaAndCrossFace (dInt32 indexCount, const dVector* const faceVertex);
	
	D_CORE_API dFloat32 MassProperties (dVector& cg, dVector& inertia, dVector& crossInertia);

	private:
	dFloat32 intg[10];
	dFloat32 mult[10];
};

#endif
