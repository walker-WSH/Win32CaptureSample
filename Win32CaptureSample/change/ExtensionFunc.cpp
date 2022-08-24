#include "./../pch.h"
#include "./../App.h"
#include "./../SampleWindow.h"
#include "./../SimpleCapture.h"
#include "CustomChange.h"

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

void SampleWindow::HandleCaptureItemClosed()
{
	TerminateProcess(GetCurrentProcess(), (UINT)E_WgcExitCode::CaptureClosed);
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

void CheckDXError(winrt::com_ptr<ID3D11Device> d3dDevice, HRESULT hr)
{
	HRESULT translatedHr;

	HRESULT deviceRemovedReason = d3dDevice->GetDeviceRemovedReason();
	switch (deviceRemovedReason) {
	case DXGI_ERROR_DEVICE_REMOVED:
	case DXGI_ERROR_DEVICE_RESET:
	case E_OUTOFMEMORY:
		// Our device has been stopped due to an external event on the GPU so map them all to
		// device removed and continue processing the condition
		translatedHr = DXGI_ERROR_DEVICE_REMOVED;
		break;

	case S_OK:
		// Device is not removed so use original error
		translatedHr = hr;
		break;

	default:
		// Device is removed but not a error we want to remap
		translatedHr = deviceRemovedReason;
		break;
	}

	switch (translatedHr) {
	case DXGI_ERROR_DEVICE_REMOVED:
	case DXGI_ERROR_ACCESS_LOST:
	case DXGI_ERROR_INVALID_CALL:
		// should reset capture
		TerminateProcess(GetCurrentProcess(), (UINT)E_WgcExitCode::DXError);
		break;

	default:
		break;
	}
}

bool SimpleCapture::CreateSharedTexture(const D3D11_TEXTURE2D_DESC &descTemp, DXGI_FORMAT fmt)
{
	winrt::com_ptr<ID3D11Device> d3dDevice = GetDXGIInterfaceFromObject<ID3D11Device>(m_device);

	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = descTemp.Width;
	desc.Height = descTemp.Height;
	desc.Format = fmt;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

	HANDLE hdl = 0;
	winrt::com_ptr<ID3D11Texture2D> textureTemp;
	HRESULT hr = d3dDevice->CreateTexture2D(&desc, nullptr, (ID3D11Texture2D **)textureTemp.put_void());
	if (!textureTemp) {
		assert(false);
		CheckDXError(d3dDevice, hr);
		return false;
	}

	winrt::com_ptr<IDXGIResource> res;
	textureTemp->QueryInterface(__uuidof(IDXGIResource), (void **)res.put_void());
	if (!res) {
		assert(false);
		CheckDXError(d3dDevice, hr);
		return false;
	}

	res->GetSharedHandle(&hdl);
	if (!hdl) {
		assert(false);
		CheckDXError(d3dDevice, hr);
		return false;
	}

	textureTemp->GetDesc(&m_textureDesc);
	m_sharedTexture = textureTemp;
	m_hSharedHandle = hdl;

	return true;
}

void SimpleCapture::OnTextureCaptured(winrt::com_ptr<ID3D11Texture2D> texture)
{
	if (!texture)
		return;

	D3D11_TEXTURE2D_DESC descTemp;
	texture->GetDesc(&descTemp);

	DXGI_FORMAT fmt;
	switch (descTemp.Format) {
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		fmt = DXGI_FORMAT_B8G8R8A8_UNORM;
		break;
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		fmt = DXGI_FORMAT_R8G8B8A8_UNORM;
		break;
	default:
		fmt = descTemp.Format;
		break;
	}

	if (fmt != DXGI_FORMAT_B8G8R8A8_UNORM && fmt != DXGI_FORMAT_R8G8B8A8_UNORM) {
		assert(false);
		return;
	}

	if (m_sharedTexture) {
		if (m_textureDesc.Width != descTemp.Width || m_textureDesc.Height != descTemp.Height || m_textureDesc.Format != fmt) {
			m_sharedTexture = nullptr;
			m_hSharedHandle = 0;
		}
	}

	if (!m_sharedTexture) {
		bool bOK = CreateSharedTexture(descTemp, fmt);
		if (!bOK) {
			assert(false);
			return;
		}
	}

	m_d3dContext->CopyResource(m_sharedTexture.get(), texture.get());

	ST_WGCOutputInfo output;
	output.width = m_textureDesc.Width;
	output.height = m_textureDesc.Height;
	output.previousUpdate = GetTickCount();
	output.sharedHanle = (uint64_t)m_hSharedHandle;

	memmove(&CustomChange::Instance()->m_pMapInfo->output, &output, sizeof(ST_WGCOutputInfo));

#ifdef _DEBUG
	char buf[200];
	snprintf(buf, 200, "WGC handle %llu \n", (uint64_t)m_hSharedHandle);
	OutputDebugStringA(buf);
#endif
}
