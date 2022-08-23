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
using namespace robmikh::common::desktop;
using namespace robmikh::common::uwp;
}

HANDLE g_hSharedHandle = 0;
D3D11_TEXTURE2D_DESC g_textureDesc{};
winrt::com_ptr<ID3D11Texture2D> g_pSharedTexture;

static const IID dxgiFactory2 = {0x50c83a1c, 0xe072, 0x4c48, {0x87, 0xb0, 0x36, 0x30, 0xfa, 0x36, 0xa6, 0xd0}};
const static std::vector<D3D_FEATURE_LEVEL> featureLevels = {
	D3D_FEATURE_LEVEL_11_0,
	D3D_FEATURE_LEVEL_10_1,
	D3D_FEATURE_LEVEL_10_0,
	D3D_FEATURE_LEVEL_9_3,
};

void SampleWindow::HandleCaptureItemClosed()
{
	TerminateProcess(GetCurrentProcess(), (UINT)E_WgcExitCode::CaptureClosed);
}

void SampleWindow::AutoStartCapture()
{
	if (CustomChange::Instance()->m_pMapInfo->input.type == E_CaptureType::TypeWindow) {
		auto hWnd = (HWND)CustomChange::Instance()->m_pMapInfo->input.more.wd.hWnd;
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

		int index = CustomChange::Instance()->m_pMapInfo->input.more.md.monitorIndex;
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
	HRESULT hr = d3dDevice->CreateTexture2D(&desc, nullptr, textureTemp.put());
	if (!textureTemp) {
		assert(false);
		CheckDXError(d3dDevice, hr);
		return false;
	}

	winrt::com_ptr<IDXGIResource> res;
	textureTemp->QueryInterface(__uuidof(IDXGIResource), res.put_void());
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

	textureTemp->GetDesc(&g_textureDesc);
	g_pSharedTexture = textureTemp;
	g_hSharedHandle = hdl;

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

	if (g_pSharedTexture) {
		if (g_textureDesc.Width != descTemp.Width || g_textureDesc.Height != descTemp.Height || g_textureDesc.Format != fmt) {
			g_pSharedTexture = nullptr;
			g_hSharedHandle = 0;
		}
	}

	if (!g_pSharedTexture) {
		bool bOK = CreateSharedTexture(descTemp, fmt);
		if (!bOK) {
			assert(false);
			return;
		}
	}

	m_d3dContext->CopyResource(g_pSharedTexture.get(), texture.get());

	ST_WGCOutputInfo output;
	output.width = g_textureDesc.Width;
	output.height = g_textureDesc.Height;
	output.previousUpdate = GetTickCount();
	output.sharedHanle = (uint64_t)g_hSharedHandle;

	memmove(&CustomChange::Instance()->m_pMapInfo->output, &output, sizeof(ST_WGCOutputInfo));

#ifdef _DEBUG
	char buf[MAX_PATH];
	snprintf(buf, MAX_PATH, "WGC handle %llu \n", (uint64_t)g_hSharedHandle);
	OutputDebugStringA(buf);
#endif
}

using get_file_version_info_size_wt = DWORD(WINAPI *)(LPCWSTR md, LPDWORD unused);
using get_file_version_info_wt = BOOL(WINAPI *)(LPCWSTR md, DWORD unused, DWORD len, LPVOID data);
using ver_query_value_wt = BOOL(WINAPI *)(LPVOID data, LPCWSTR subblock, LPVOID *buf, PUINT sizeout);

bool InitFun(get_file_version_info_size_wt &pFunc1, get_file_version_info_wt &pFunc2, ver_query_value_wt &pFunc3)
{
	HMODULE ver = GetModuleHandleW(L"version");
	if (!ver)
		return false;

	pFunc1 = (get_file_version_info_size_wt)GetProcAddress(ver, "GetFileVersionInfoSizeW");
	pFunc2 = (get_file_version_info_wt)GetProcAddress(ver, "GetFileVersionInfoW");
	pFunc3 = (ver_query_value_wt)GetProcAddress(ver, "VerQueryValueW");

	if (!pFunc1 || !pFunc2 || !pFunc3)
		return false;

	return true;
}

VS_FIXEDFILEINFO GetDllVersion(const wchar_t *pDllName)
{
	LPVOID data = nullptr;
	VS_FIXEDFILEINFO ret = {};

	do {
		get_file_version_info_size_wt pGetFileVersionInfoSize;
		get_file_version_info_wt pGetFileVersionInfo;
		ver_query_value_wt pVerQueryValue;

		if (!InitFun(pGetFileVersionInfoSize, pGetFileVersionInfo, pVerQueryValue))
			break;

		DWORD size = pGetFileVersionInfoSize(pDllName, nullptr);
		if (!size)
			break;

		data = HeapAlloc(GetProcessHeap(), 0, size);
		if (!pGetFileVersionInfo(L"kernel32", 0, size, data))
			break;

		UINT len = 0;
		VS_FIXEDFILEINFO *info = nullptr;
		pVerQueryValue(data, L"\\", (LPVOID *)&info, &len);

		ret = *info;
	} while (false);

	if (data) {
		HeapFree(GetProcessHeap(), 0, data);
	}

	return ret;
}

unsigned GetWinVersion()
{
	VS_FIXEDFILEINFO info = GetDllVersion(L"kernel32");

	auto major = (int)HIWORD(info.dwFileVersionMS);
	auto minor = (int)LOWORD(info.dwFileVersionMS);

	return (major << 8) | minor;
}

winrt::com_ptr<IDXGIAdapter1> InitFactory(const LUID &luid)
{
	winrt::com_ptr<IDXGIFactory1> factory;
	winrt::com_ptr<IDXGIAdapter1> adapter;

	IID factoryIID = (GetWinVersion() >= 0x602) ? dxgiFactory2 : __uuidof(IDXGIFactory1);
	HRESULT hr = CreateDXGIFactory1(factoryIID, (void **)factory.put_void());
	if (FAILED(hr))
		return nullptr;

	int adapterIndex = 0;
	while (factory->EnumAdapters1(adapterIndex++, adapter.put()) == S_OK) {
		DXGI_ADAPTER_DESC adpDesc;
		adapter->GetDesc(&adpDesc);

		bool bChoosen = (adpDesc.AdapterLuid.HighPart == luid.HighPart && adpDesc.AdapterLuid.LowPart == luid.LowPart);
		if (!bChoosen) {
#ifdef _DEBUG
			if (!luid.HighPart && !luid.LowPart)
				bChoosen = true; // using default adapter
#endif
		}

		if (bChoosen) {
			OutputDebugStringW(L"WGC select adapter: ");
			OutputDebugStringW(adpDesc.Description);
			OutputDebugStringW(L"\n");
			return adapter;
		}
	}

	return nullptr;
}

winrt::com_ptr<ID3D11Device> CreateDX11Device()
{
	winrt::com_ptr<IDXGIAdapter1> adp = InitFactory(CustomChange::Instance()->m_pMapInfo->input.adapterLuid);
	if (!adp) {
		assert(false);
		TerminateProcess(GetCurrentProcess(), (UINT)E_WgcExitCode::AdapterNotFound);
		return nullptr;
	}

	winrt::com_ptr<ID3D11Device> pDX11Device = nullptr;
	winrt::com_ptr<ID3D11DeviceContext> pDeviceContext = nullptr;

	D3D_FEATURE_LEVEL levelUsed = D3D_FEATURE_LEVEL_9_3;
	HRESULT hr = D3D11CreateDevice(adp.get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, featureLevels.data(), (UINT)featureLevels.size(), D3D11_SDK_VERSION,
				       pDX11Device.put(), &levelUsed, pDeviceContext.put());
	if (FAILED(hr)) {
		assert(false);
		TerminateProcess(GetCurrentProcess(), (UINT)E_WgcExitCode::DXFailCreate);
		return nullptr;
	}

	return pDX11Device;
}
