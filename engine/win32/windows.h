#pragma once


#if defined(WINVER)
#undef WINVER
#endif
#if defined(_WIN32_WINNT)
#undef _WIN32_WINNT
#endif
#define WINVER			_WIN32_WINNT_VISTA  
#define _WIN32_WINNT	_WIN32_WINNT_VISTA

#if defined(UNICODE)
#undef UNICODE
#endif
#if defined(_UNICODE)
#undef _UNICODE
#endif

#include <Windows.h>
