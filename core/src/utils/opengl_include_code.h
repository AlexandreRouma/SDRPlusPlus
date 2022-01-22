#pragma once

#if defined(_WIN32)
#include <windows.h>
#include <GL/gl.h>
#elif define(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif