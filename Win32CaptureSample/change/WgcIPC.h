#pragma once
#include <windows.h>
#include <stdint.h>
#include <assert.h>
#include <dxgi.h>

#define ALIGN(bytes, align) (((bytes) + ((align)-1)) & ~((align)-1))

/*
 EVENT HANDLE
 full name : guid + ${CAPTURE_ALIVE_EVENT}
 check already openned in child process.
 it is created/deleted by main process. Child process should exit itselft when this HANDLE is not openned.
*/
#define CAPTURE_ALIVE_EVENT "wgc_capture_alive"

/*
 map HANDLE
 full name : guid + ${CAPTURE_INFO_MAP}
 it is created by main process. Child process should open it and write data
*/
#define CAPTURE_INFO_MAP "wgc_capture_info"

enum class E_WgcExitCode {
	Normal = 10000,
	InvalidParam,
	Unavailable,
	Crahed,
	NotFound,
	Unsupported,
	ExitSelf,
	FailInitMap,
	DXError,
	DXFailCreate,
	AdapterNotFound,
	MainThreadBlock,
	CaptureClosed,
	WaitVideoTimeout,
};

enum class E_CaptureType {
	TypeUnknown = 0,
	TypeWindow,
	TypeMonitor,
};

//---------------------------------------- struct start ---------------------------------
#pragma pack(push)
#pragma pack(1)
struct ST_WindowDetail {
	uint64_t hWnd; // Can't use HWND since its size is not fixed
};

struct ST_MonitorDetail {
	int monitorIndex;
};

struct ST_WGCInputInfo {
	LUID adapterLuid = {0};
	bool cursor = true;
	E_CaptureType type = E_CaptureType::TypeUnknown;
	union MoreInfo {
		ST_WindowDetail wd;
		ST_MonitorDetail md;
	} more;
};

struct ST_WGCOutputInfo {
	uint32_t previousUpdate = 0; // GetTickCount
	uint64_t sharedHanle = 0;    // Can't use HANDLE since its size is not fixed
	uint32_t width = 0;
	uint32_t height = 0;
	DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
};

struct ST_WGCMapInfo {
	ST_WGCInputInfo input;   // writen by main process
	ST_WGCOutputInfo output; // writen by child process
};
#pragma pack(pop)
//---------------------------------------- struct end ------------------------------------
