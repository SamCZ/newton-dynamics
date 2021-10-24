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


#include "dCoreStdafx.h"
#include "ndCollisionStdafx.h"
#include "ndContact.h"
#include "ndContactList.h"
#include "ndBodyKinematic.h"

ndContact* ndContactList::CreateContact(ndBodyKinematic* const body0, ndBodyKinematic* const body1)
{
	dNode* node;
	{
		dScopeSpinLock lock(m_lock);
		node = Append();
	}
	ndContact* const contact = &node->GetInfo();
	contact->SetBodies(body0, body1);
	contact->m_linkNode = node;
	contact->AttachToBodies();
	return contact;
}

void ndContactList::DeleteContact(ndContact* const contact)
{
	dAssert(contact->m_isAttached);
	contact->DetachFromBodies();
	Remove(contact->m_linkNode);
}

void ndContactList::DeleteAllContacts()
{
	while (GetFirst())
	{
		DeleteContact(&GetFirst()->GetInfo());
	}
	//FlushFreeList();
}
