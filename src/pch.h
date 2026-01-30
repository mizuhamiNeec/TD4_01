#pragma once

#ifndef _CRTDBG_MAP_ALLOC
#define _CRTDBG_MAP_ALLOC
#endif
#include <crtdbg.h>
#include <cstdlib>

#ifdef _DEBUG
#define NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#else
#define NEW new
#endif

//-----------------------------------------------------------------------------
// Windows
//-----------------------------------------------------------------------------
#include <Windows.h>

//-----------------------------------------------------------------------------
// Unnamed
//-----------------------------------------------------------------------------
#include <core/unnamed/UnnamedMacro.h>
#include <core/unnamed/string/StrUtil.h>
#include <engine/unnamed/subsystem/console/Log.h>

//-----------------------------------------------------------------------------
// STD
//-----------------------------------------------------------------------------
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <malloc.h>
#include <stdexcept>
#include <string>
