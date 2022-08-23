#pragma once
#include <windows.h>
#include <process.h>
#include <memory>
#include <stdint.h>
#include <assert.h>
#include <string>
#include <vector>
#include "WgcIPC.h"

enum class E_CaptureType {
	TypeUnknown = 0,
	TypeWindow,
	TypeMonitor,
};

struct ST_EnumMonitorInfo {
	HMONITOR handle = 0;
	size_t index = 0;
};

struct ST_CaptureParam {
	E_CaptureType type = E_CaptureType::TypeUnknown;
	uint64_t value = 0; // HWND or monitor index
	bool cursor = true;
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

private:
	ST_CaptureParam m_CaptureParams;

	HANDLE m_hExitEvent = 0;
	HANDLE m_hCheckAliveThread = 0;
};
