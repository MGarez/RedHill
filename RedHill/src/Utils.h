#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif

#include <windows.h>

// Auxiliary methods to interrupt
inline std::string HrToString(HRESULT hr)
{
	char s_str[64] = {};
	sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(hr));
	return s_str;
}

inline void CrashIfFailed(HRESULT hr)
{
	if (FAILED(hr))
	{
		::OutputDebugStringA(HrToString(hr).c_str());
		::__debugbreak();
	}
}
