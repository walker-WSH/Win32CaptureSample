#include "CustomChange.h"
#include "StringConvert.h"

CustomChange *CustomChange::Instance()
{
	static CustomChange ins;
	return &ins;
}

BOOL CALLBACK CustomChange::EnumDisplayMonitors_Callback(HMONITOR handle, HDC, LPRECT, LPARAM lParam)
{
	auto monitorList = (std::vector<ST_EnumMonitorInfo> *)(lParam);

	ST_EnumMonitorInfo info;
	info.handle = handle;
	info.index = monitorList->size();

	monitorList->push_back(info);
	return TRUE;
}

unsigned CALLBACK CustomChange::CheckAliveThread(void *pParam)
{
	CustomChange *self = reinterpret_cast<CustomChange *>(pParam);
	DWORD startTime = GetTickCount();

	while (WAIT_OBJECT_0 != WaitForSingleObject(self->m_hExitEvent, 1000)) {
		if (!self->IsAlive())
			TerminateProcess(GetCurrentProcess(), (UINT)E_WgcExitCode::ExitSelf);

		DWORD crt = GetTickCount();
		if (self->m_pMapInfo->output.sharedHanle) {
			DWORD pre = self->m_dwPreHeartBeat;
			if (crt > pre && (crt - pre) >= MAIN_THREAD_BLOCK_TIMEOUT) {
				assert(false);
				OutputDebugStringA("[WGC Process] main thread is blocked \n");
				TerminateProcess(GetCurrentProcess(), (UINT)E_WgcExitCode::MainThreadBlock);
			}
		} else {
			if (crt - startTime >= WAIT_FIRST_VIDEO_TIMEOUT) {
				OutputDebugStringA("[WGC Process] timeout to wait first frame \n");
				TerminateProcess(GetCurrentProcess(), (UINT)E_WgcExitCode::WaitVideoTimeout);
			}
		}
	}

	return 0;
}

LONG WINAPI ExceptionFilter(struct _EXCEPTION_POINTERS *pExceptionPointers)
{
	TerminateProcess(GetCurrentProcess(), (UINT)E_WgcExitCode::Crahed);
	return EXCEPTION_EXECUTE_HANDLER;
}

void CustomChange::InitParams()
{
	SetUnhandledExceptionFilter(ExceptionFilter);

	int argc = 0;
	LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (!argv) {
		assert(false);
		OutputDebugStringA("[WGC Process] invalid params \n");
		TerminateProcess(GetCurrentProcess(), (UINT)E_WgcExitCode::InvalidParam);
		return;
	}

	std::shared_ptr<LPWSTR> freeArg(argv, [](LPWSTR *ptr) { LocalFree(ptr); });

	if (argc != 2 || !argv[1]) {
		assert(false);
		OutputDebugStringA("[WGC Process] invalid params \n");
		TerminateProcess(GetCurrentProcess(), (UINT)E_WgcExitCode::InvalidParam);
		return;
	}

	m_strGUID = str::w2u(argv[1]);

	if (!IsAlive()) {
		TerminateProcess(GetCurrentProcess(), (UINT)E_WgcExitCode::ExitSelf);
		return;
	}

	if (!InitMap()) {
		TerminateProcess(GetCurrentProcess(), (UINT)E_WgcExitCode::FailInitMap);
		return;
	}

	if (m_pMapInfo->input.type == E_WGCType::TypeWindow) {
		if (!IsWindow(HWND(m_pMapInfo->input.more.wd.hWnd))) {
			TerminateProcess(GetCurrentProcess(), (UINT)E_WgcExitCode::NotFound);
			return;
		}
	} else if (m_pMapInfo->input.type == E_WGCType::TypeMonitor) {
		std::vector<ST_EnumMonitorInfo> monitorList;
		EnumDisplayMonitors(nullptr, nullptr, EnumDisplayMonitors_Callback, (LPARAM)&monitorList);

		int index = m_pMapInfo->input.more.md.monitorIndex;
		if (index < 0 || index >= monitorList.size()) {
			TerminateProcess(GetCurrentProcess(), (UINT)E_WgcExitCode::NotFound);
			return;
		}
	} else {
		assert(false);
		OutputDebugStringA("[WGC Process] invalid params \n");
		TerminateProcess(GetCurrentProcess(), (UINT)E_WgcExitCode::InvalidParam);
		return;
	}
}

void CustomChange::Start()
{
	m_hExitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_hCheckAliveThread = (HANDLE)_beginthreadex(0, 0, &CustomChange::CheckAliveThread, this, 0, 0);
}

void CustomChange::Stop()
{
	SetEvent(m_hExitEvent);
	WaitForSingleObject(m_hCheckAliveThread, INFINITE);

	UninitMap();

	CHandleWrapper::CloseHandleEx(m_hCheckAliveThread);
	CHandleWrapper::CloseHandleEx(m_hExitEvent);

	TerminateProcess(GetCurrentProcess(), (UINT)E_WgcExitCode::Normal);
}

bool CustomChange::IsAlive()
{
	std::string name = m_strGUID + std::string(CAPTURE_ALIVE_EVENT);

	HANDLE hdl = CHandleWrapper::GetAlreadyEvent(name.c_str());
	if (!CHandleWrapper::IsHandleValid(hdl))
		return false;

	CHandleWrapper::CloseHandleEx(hdl);
	return true;
}

bool CustomChange::InitMap()
{
	SIZE_T size = ALIGN(sizeof(ST_WGCMapInfo), 64);
	std::string name = m_strGUID + std::string(CAPTURE_INFO_MAP);

	bool bNewCreate = false;
	m_hMapHandle = CHandleWrapper::GetMap(name.c_str(), (unsigned)size, &bNewCreate);
	if (!CHandleWrapper::IsHandleValid(m_hMapHandle) || bNewCreate) {
		assert(false);
		OutputDebugStringA("[WGC Process] fail to init map \n");
		return false;
	}

	m_pMapViewOfFile = MapViewOfFile(m_hMapHandle, FILE_MAP_ALL_ACCESS, 0, 0, size);
	if (!m_pMapViewOfFile) {
		assert(false);
		OutputDebugStringA("[WGC Process] fail to map view \n");
		return false;
	}

	m_pMapInfo = (ST_WGCMapInfo *)m_pMapViewOfFile;
	return true;
}

void CustomChange::UninitMap()
{
	if (m_pMapViewOfFile)
		UnmapViewOfFile(m_pMapViewOfFile);

	m_pMapViewOfFile = nullptr;
	m_pMapInfo = nullptr;

	CHandleWrapper::CloseHandleEx(m_hMapHandle);
}
