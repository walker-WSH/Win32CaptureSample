#pragma once
#include <windows.h>
#include <stdint.h>
#include <assert.h>

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
	NotFound,
	Unsupported,
	ExitSelf,
};
