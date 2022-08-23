#pragma once
#include <Windows.h>

/* clang-format off */

class CHandleWrapper {
public:
	static HANDLE GetAlreadyMap(const char *pName);
	static HANDLE GetMap(const char *pName, unsigned size, bool *pNewCreate = NULL);

	static HANDLE GetAlreadyEvent(const char *pName);
	static HANDLE GetEvent(const char *pName, bool bNeedManualReset = true, bool *pNewCreate = NULL);

	// WaitForSingleObject, ReleaseMutex
	static HANDLE GetMutex(const char *pName, bool *pNewCreate = NULL);
	static bool IsMutexAlready(const char *pName);

	static bool IsHandleValid(const HANDLE &hHandle);
	static bool IsHandleSigned(const HANDLE &hHandle, DWORD dwMilliSecond = 0);
	static void CloseHandleEx(HANDLE &hHandle);
	static void WaitThreadEnd(HANDLE &hThread, DWORD dwMilliSecond = INFINITE);
};

/* clang-format on */
