#include "CustomChange.h"
#include "HandleWrapper.h"
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
	while (WAIT_OBJECT_0 != WaitForSingleObject(self->m_hExitEvent, 1000)) {
		if (!self->IsAlive())
			TerminateProcess(GetCurrentProcess(), (UINT)E_WgcExitCode::ExitSelf);
	}
	return 0;
}

void CustomChange::InitParams()
{
	int argc = 0;
	LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (!argv) {
		TerminateProcess(GetCurrentProcess(), (UINT)E_WgcExitCode::InvalidParam);
		assert(false);
		return;
	}

	std::shared_ptr<LPWSTR> freeArg(argv, [](LPWSTR *ptr) { LocalFree(ptr); });

	if (argc != 5) {
		TerminateProcess(GetCurrentProcess(), (UINT)E_WgcExitCode::InvalidParam);
		assert(false);
		return;
	}

	LPWSTR cursor = argv[1];
	LPWSTR method = argv[2];
	LPWSTR value = argv[3]; // HWND or monitor index
	LPWSTR guid = argv[4];

	if (!cursor || !method || !value || !guid) {
		TerminateProcess(GetCurrentProcess(), (UINT)E_WgcExitCode::InvalidParam);
		assert(false);
		return;
	}

	m_CaptureParams.cursor = !!std::stoi(cursor);
	m_CaptureParams.value = std::stoull(value);
	m_CaptureParams.guid = str::w2u(guid);

	if (0 == wcscmp(method, L"window")) {
		m_CaptureParams.type = E_CaptureType::TypeWindow;

		if (!IsWindow(HWND(m_CaptureParams.value))) {
			TerminateProcess(GetCurrentProcess(), (UINT)E_WgcExitCode::NotFound);
			return;
		}
	} else if (0 == wcscmp(method, L"monitor")) {
		m_CaptureParams.type = E_CaptureType::TypeMonitor;

		std::vector<ST_EnumMonitorInfo> monitorList;
		EnumDisplayMonitors(nullptr, nullptr, EnumDisplayMonitors_Callback, (LPARAM)&monitorList);

		if (m_CaptureParams.value < 0 || m_CaptureParams.value >= monitorList.size()) {
			TerminateProcess(GetCurrentProcess(), (UINT)E_WgcExitCode::NotFound);
			return;
		}
	} else {
		TerminateProcess(GetCurrentProcess(), (UINT)E_WgcExitCode::InvalidParam);
		assert(false);
		return;
	}

	if (!IsAlive()) {
		TerminateProcess(GetCurrentProcess(), (UINT)E_WgcExitCode::ExitSelf);
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

	CloseHandle(m_hCheckAliveThread);
	CloseHandle(m_hExitEvent);

	TerminateProcess(GetCurrentProcess(), (UINT)E_WgcExitCode::Normal);
}

bool CustomChange::IsAlive()
{
	std::string name = m_CaptureParams.guid + std::string(CAPTURE_ALIVE_EVENT);

	HANDLE hdl = CHandleWrapper::GetAlreadyEvent(name.c_str());
	if (!CHandleWrapper::IsHandleValid(hdl))
		return false;

	CHandleWrapper::CloseHandleEx(hdl);
	return true;
}
