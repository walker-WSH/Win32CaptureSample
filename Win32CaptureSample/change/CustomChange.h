#pragma once
#include <windows.h>
#include <process.h>
#include <memory>
#include <stdint.h>
#include <assert.h>
#include <string>
#include <vector>
#include "WgcIPC.h"
#include "HandleWrapper.h"

struct ST_EnumMonitorInfo {
	HMONITOR handle = 0;
	size_t index = 0;
};

struct ST_CaptureParam {
	std::string guid;
};

class CustomChange {
public:
	static CustomChange *Instance();
	static BOOL CALLBACK EnumDisplayMonitors_Callback(HMONITOR handle, HDC, LPRECT, LPARAM lParam);
	static unsigned CALLBACK CheckAliveThread(void *pParam);

	void InitParams();
	void Start();
	void Stop();

protected:
	CustomChange() {}

	bool IsAlive();
	bool InitMap();
	void UninitMap();

private:
	std::string m_strGUID;

	HANDLE m_hMapHandle = 0;
	void *m_pMapViewOfFile = nullptr;
	ST_WGCMapInfo *m_pMapInfo = nullptr;

	HANDLE m_hExitEvent = 0;
	HANDLE m_hCheckAliveThread = 0;
};
