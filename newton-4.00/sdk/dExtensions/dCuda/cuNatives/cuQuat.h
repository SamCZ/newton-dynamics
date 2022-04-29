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

#ifndef __ND_CUQUAT_H__
#define __ND_CUQUAT_H__

#include "cuVector.h"
#include "cuMatrix3x3.h"

class cuQuat: public cuVector
{
	public:
	inline __device__ __host__ cuQuat()
		:cuVector(0.0f, 0.0f, 0.0f, 1.0f)
	{
	}

	inline __device__ __host__ cuQuat(float x, float y, float z, float w)
		:cuVector(x, y, z, w)
	{
	}

	inline __device__ __host__ cuQuat(const cuQuat& src)
		:cuVector(src)
	{
	}

	inline __device__ __host__ cuQuat(const cuVector& src)
		:cuVector(src)
	{
	}

	inline __device__ __host__ cuQuat(const cuVector& unitAxis, float angle)
		:cuVector()
	{
		angle = angle * 0.5f;
		w = cos(angle);
		const float sinAng = sin(angle);
		x = unitAxis.x * sinAng;
		y = unitAxis.y * sinAng;
		z = unitAxis.z * sinAng;
	}

	inline cuQuat(const ndVector& src)
		:cuVector(src)
	{
	}

	inline cuMatrix3x3 __device__ __host__ GetMatrix3x3 () const
	{
		//const cuQuat quat0 = *this;
		const cuQuat quat1 (Scale (2.0));
		
		const float x2 = x * quat1.x;
		const float y2 = y * quat1.y;
		const float z2 = z * quat1.z;

		const float xy = x * quat1.y;
		const float xz = x * quat1.z;
		const float xw = x * quat1.w;
		const float yz = y * quat1.z;
		const float yw = y * quat1.w;
		const float zw = z * quat1.w;

		const cuVector front(1.0f - y2 - z2, xy + zw, xz - yw, 0.0f);
		const cuVector up   (xy - zw, 1.0f - x2 - z2, yz + xw, 0.0f);
		const cuVector right(xz + yw, yz - xw, 1.0f - x2 - y2, 0.0f);
		return cuMatrix3x3(front, up, right);
	}

	inline cuQuat __device__ __host__ Normalize() const
	{
		return cuVector::Normalize();
	}

	inline cuQuat __device__ __host__ operator* (const cuQuat &q) const
	{
		const cuVector x_( q.w,  q.z, -q.y, -q.x);
		const cuVector y_(-q.z,  q.w,  q.x, -q.y);
		const cuVector z_( q.y, -q.x,  q.w, -q.z);
		const cuVector w_(q);
		return x_ * cuVector(x) + y_ * cuVector(y) + z_ * cuVector(z) + w_ * cuVector(w);
	}
};

#endif