# Copyright (c) <2014-2017> <Newton Game Dynamics>
#
# This software is provided 'as-is', without any express or implied
# warranty. In no event will the authors be held liable for any damages
# arising from the use of this software.
#
# Permission is granted to anyone to use this software for any purpose,
# including commercial applications, and to alter it and redistribute it
# freely.

cmake_minimum_required(VERSION 3.9.0 FATAL_ERROR)

add_subdirectory(hacd/src/VHACD_Lib)

if (NEWTON_BUILD_SANDBOX_DEMOS)
	if (MSVC OR MINGW)
        if(NOT TARGET glfw)
			add_subdirectory(glfw)
		endif()
	endif(MSVC OR MINGW)

    add_subdirectory(imgui)
	#add_subdirectory(openAL)
	add_subdirectory(glatter)
	add_subdirectory(openFBX/src)
endif()




