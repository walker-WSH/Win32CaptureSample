#pragma once
#include <windows.h>
#include <process.h>
#include <memory>
#include <stdint.h>
#include <assert.h>
#include <string>
#include <vector>
#include <atomic>
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include "WgcIPC.h"
#include "HandleWrapper.h"

#define WM_START_CAPTURE (WM_USER + 1)
#define WM_TIMERID_HEARTBEAT 2000
#define MAIN_THREAD_BLOCK_TIMEOUT 5000
#define WAIT_FIRST_VIDEO_TIMEOUT 5000

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

	ST_WGCMapInfo *m_pMapInfo = nullptr;
	std::atomic<DWORD> m_dwPreHeartBeat = GetTickCount();

protected:
	CustomChange() {}

	bool IsAlive();
	bool InitMap();
	void UninitMap();

private:
	std::string m_strGUID;

	HANDLE m_hMapHandle = 0;
	void *m_pMapViewOfFile = nullptr;

	HANDLE m_hExitEvent = 0;
	HANDLE m_hCheckAliveThread = 0;
};
