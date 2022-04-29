/* Copyright (c) <2003-2016> <Newton Game Dynamics>
* 
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
* 
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely
*/

#include "ndSandboxStdafx.h"
#include "ndAnimationPose.h"
#include "ndAnimationSequence.h"
#include "ndAnimationSequencePlayer.h"

ndAnimationSequencePlayer::ndAnimationSequencePlayer(ndAnimationSequence* const sequence)
	:ndAnimationBlendTreeNode(nullptr)
	,m_sequence(sequence)
	,m_param(ndFloat32 (0.0f))
{
}

ndAnimationSequencePlayer::~ndAnimationSequencePlayer()
{
}

ndFloat32 ndAnimationSequencePlayer::GetParam() const
{
	return m_param;
}

void ndAnimationSequencePlayer::SetParam(ndFloat32 param)
{
	m_param = dMod(param, ndFloat32 (1.0f));
}

ndAnimationSequence* ndAnimationSequencePlayer::GetSequence()
{
	return m_sequence;
}

void ndAnimationSequencePlayer::Evaluate(ndAnimationPose& output)
{
	m_sequence->CalculatePose(output, m_param);
}


