/*
===========================================================================
Copyright (C) 2019 Gian 'myT' Schellenbaum

This file is part of Challenge Quake 3 (CNQ3).

Challenge Quake 3 is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Challenge Quake 3 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Challenge Quake 3. If not, see <https://www.gnu.org/licenses/>.
===========================================================================
*/
// shared OpenGL client code for printing debug context output

#include "client.h"
#include "../renderer/tr_local.h"
#if defined(_WIN32) && defined(_DEBUG)
#include <Windows.h>
#endif
#include "GL/glew.h"


static const char* GL_DebugSourceString( GLenum source )
{
	switch (source) {
		case GL_DEBUG_SOURCE_API: return "API";
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM: return "window system";
		case GL_DEBUG_SOURCE_SHADER_COMPILER: return "shader compiler";
		case GL_DEBUG_SOURCE_THIRD_PARTY: return "third-party";
		case GL_DEBUG_SOURCE_APPLICATION: return "application";
		case GL_DEBUG_SOURCE_OTHER: return "other";
		default: return "?";
	}
}


static const char* GL_DebugTypeString( GLenum type )
{
	switch (type) {
		case GL_DEBUG_TYPE_ERROR: return "^1error";
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "^1deprecated behavior";
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: return "^1undefined behavior";
		case GL_DEBUG_TYPE_PORTABILITY: return "portability";
		case GL_DEBUG_TYPE_PERFORMANCE: return "^3performance";
		case GL_DEBUG_TYPE_MARKER: return "marker";
		case GL_DEBUG_TYPE_PUSH_GROUP: return "push group";
		case GL_DEBUG_TYPE_POP_GROUP: return "pop group";
		case GL_DEBUG_TYPE_OTHER: return "other";
		default: return "?";
	}
}


static const char* GL_DebugSeverityString( GLenum severity )
{
	switch (severity) {
		case GL_DEBUG_SEVERITY_LOW: return "low-severity";
		case GL_DEBUG_SEVERITY_MEDIUM: return "medium-severity";
		case GL_DEBUG_SEVERITY_HIGH: return "^3high-severity";
		case GL_DEBUG_SEVERITY_NOTIFICATION: return "notification";
		default: return "?";
	}
}


static void GLAPIENTRY GL_DebugCallback( GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam )
{
	const char* msg = va(
		"^5GL Debug: ^7%s^7/%s^7/%s^7: %s\n",
		GL_DebugSeverityString(severity), GL_DebugSourceString(source), GL_DebugTypeString(type), message);
	Com_Printf(msg);
#if defined(_WIN32) && defined(_DEBUG)
	OutputDebugStringA(msg);
#endif
}


qbool CL_GL_WantDebug()
{
#if defined(_DEBUG)
	return r_khr_debug->integer != 0;
#else
	return r_khr_debug->integer == 1;
#endif
}


void CL_GL_Init()
{
	const qbool enableDebug = CL_GL_WantDebug() && (GLEW_KHR_debug || GLEW_VERSION_4_3);
	if (!enableDebug)
		return;

	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS); // bad performance but can get access to the call stack...
	glDebugMessageCallback(&GL_DebugCallback, NULL);
	ri.Printf(PRINT_ALL, "OpenGL debug output is now active\n");
}
