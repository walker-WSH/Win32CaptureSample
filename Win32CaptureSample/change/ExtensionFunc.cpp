#include "./../pch.h"
#include "./../App.h"
#include "./../SampleWindow.h"

namespace winrt {
using namespace Windows::Foundation::Metadata;
using namespace Windows::Graphics::Capture;
using namespace Windows::Graphics::DirectX;
using namespace Windows::System;
using namespace Windows::UI;
using namespace Windows::UI::Composition;
using namespace Windows::UI::Composition::Desktop;
}

namespace util {
using namespace robmikh::common::desktop::controls;
}

void SampleWindow::AutoStartCapture()
{
	if (CustomChange::Instance()->m_pMapInfo->input.type == E_CaptureType::TypeWindow) {
		auto hWnd = (HWND)CustomChange::Instance()->m_pMapInfo->input.hWnd;
		if (!IsWindow(hWnd)) {
			TerminateProcess(GetCurrentProcess(), (UINT)E_WgcExitCode::NotFound);
			return;
		}

		if (IsIconic(hWnd))
			ShowWindow(hWnd, SW_RESTORE);

		auto item = m_app->TryStartCaptureFromWindowHandle(hWnd);
		if (!item) {
			TerminateProcess(GetCurrentProcess(), (UINT)E_WgcExitCode::Unavailable);
			return;
		}

		OnCaptureStarted(item, CaptureType::ProgrammaticWindow);

	} else {
		std::vector<ST_EnumMonitorInfo> monitorList;
		EnumDisplayMonitors(nullptr, nullptr, &CustomChange::EnumDisplayMonitors_Callback, (LPARAM)&monitorList);

		int index = CustomChange::Instance()->m_pMapInfo->input.monitorIndex;
		if (index < 0 || index >= monitorList.size()) {
			TerminateProcess(GetCurrentProcess(), (UINT)E_WgcExitCode::NotFound);
			return;
		}

		auto item = m_app->TryStartCaptureFromMonitorHandle(monitorList[index].handle);
		if (!item) {
			TerminateProcess(GetCurrentProcess(), (UINT)E_WgcExitCode::Unavailable);
			return;
		}

		OnCaptureStarted(item, CaptureType::ProgrammaticMonitor);
	}

	auto isBorderRequiredPresent = winrt::Windows::Foundation::Metadata::ApiInformation::IsPropertyPresent(winrt::name_of<winrt::GraphicsCaptureSession>(), L"IsBorderRequired");
	if (isBorderRequiredPresent)
		m_app->IsBorderRequired(false);

	auto isCursorEnablePresent = winrt::ApiInformation::IsApiContractPresent(L"Windows.Foundation.UniversalApiContract", 9);
	if (isCursorEnablePresent)
		m_app->IsCursorEnabled(CustomChange::Instance()->m_pMapInfo->input.cursor);

	m_app->PixelFormat(m_pixelFormats[0].PixelFormat);
}
