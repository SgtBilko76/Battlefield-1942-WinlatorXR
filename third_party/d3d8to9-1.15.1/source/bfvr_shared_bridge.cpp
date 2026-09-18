/**
 * BFVR extension to pinned d3d8to9 v1.15.1.
 *
 * License: same BSD-2-Clause terms as the upstream d3d8to9 source.
 */
#include "bfvr_shared_bridge.hpp"
#include "d3d8to9.hpp"

#include <d3dcommon.h>

#include <cstring>

namespace
{
volatile LONG g_helperDeviceCreations = 0;
volatile LONG g_helperAttempts = 0;
volatile LONG g_lastHelperStage = static_cast<LONG>(
	BFVRD3D8To9SharedHelperStage::NotAttempted);
volatile LONG g_lastHelperResult = E_PENDING;
volatile LONG g_lastHelperCreateDeviceResult = E_PENDING;
volatile LONG g_lastHelperCreateTextureResult = E_PENDING;
volatile LONG g_lastGameOpenResult = E_PENDING;

void PublishHelperDiagnostics(
	BFVRD3D8To9SharedHelperStage stage,
	HRESULT result)
{
	InterlockedExchange(
		&g_lastHelperStage,
		static_cast<LONG>(stage));
	InterlockedExchange(&g_lastHelperResult, result);
}

HRESULT ValidateTranslatedDevice(
	void* opaqueDevice,
	Direct3DDevice8** translatedDevice)
{
	if (opaqueDevice == nullptr || translatedDevice == nullptr)
		return D3DERR_INVALIDCALL;

	*translatedDevice = nullptr;
	auto* const device8 = static_cast<IDirect3DDevice8*>(opaqueDevice);
	void* verifiedDevice = nullptr;
	const HRESULT result =
		device8->QueryInterface(IID_BFVRD3D8To9Device, &verifiedDevice);
	if (FAILED(result) || verifiedDevice == nullptr)
		return D3DERR_INVALIDCALL;

	*translatedDevice =
		static_cast<Direct3DDevice8*>(
			static_cast<IDirect3DDevice8*>(verifiedDevice));
	return D3D_OK;
}

void PopulateAdapterDiagnostics(
	Direct3DDevice8* translatedDevice,
	BFVRD3D8To9SharedDeviceDiagnostics& diagnostics)
{
	diagnostics.adapterOrdinal = static_cast<UINT>(-1);
	diagnostics.getCreationParametersResult = E_PENDING;
	diagnostics.getDirect3DResult = E_PENDING;
	diagnostics.queryDirect3D9ExResult = E_PENDING;
	diagnostics.getAdapterLuidResult = E_PENDING;

	IDirect3DDevice9* const gameDevice =
		translatedDevice->GetProxyInterface();
	D3DDEVICE_CREATION_PARAMETERS creation = {};
	IDirect3D9* direct3D9 = nullptr;
	IDirect3D9Ex* direct3D9Ex = nullptr;
	diagnostics.getCreationParametersResult =
		gameDevice->GetCreationParameters(&creation);
	if (SUCCEEDED(diagnostics.getCreationParametersResult))
	{
		diagnostics.adapterOrdinal = creation.AdapterOrdinal;
		diagnostics.getDirect3DResult =
			gameDevice->GetDirect3D(&direct3D9);
	}
	if (SUCCEEDED(diagnostics.getDirect3DResult) && direct3D9 != nullptr)
	{
		diagnostics.queryDirect3D9ExResult =
			direct3D9->QueryInterface(
				__uuidof(IDirect3D9Ex),
				reinterpret_cast<void**>(&direct3D9Ex));
	}
	if (SUCCEEDED(diagnostics.queryDirect3D9ExResult) &&
		direct3D9Ex != nullptr)
	{
		LUID adapterLuid = {};
		diagnostics.getAdapterLuidResult =
			direct3D9Ex->GetAdapterLUID(
				creation.AdapterOrdinal,
				&adapterLuid);
		if (SUCCEEDED(diagnostics.getAdapterLuidResult))
		{
			diagnostics.adapterLuidHigh = adapterLuid.HighPart;
			diagnostics.adapterLuidLow = adapterLuid.LowPart;
		}
	}

	if (direct3D9Ex != nullptr)
		direct3D9Ex->Release();
	if (direct3D9 != nullptr)
		direct3D9->Release();
}

bool IsSupportedSharedFormat(D3DFORMAT format)
{
	return format == D3DFMT_A2B10G10R10 ||
		format == D3DFMT_A16B16G16R16F ||
		format == D3DFMT_A8R8G8B8 ||
		format == D3DFMT_A8B8G8R8;
}

HRESULT CreateSharedTextureThroughHelper(
	Direct3DDevice8* translatedDevice,
	UINT width,
	UINT height,
	D3DFORMAT format,
	IDirect3DTexture9** openedTexture,
	HANDLE* sharedHandle)
{
	if (translatedDevice == nullptr ||
		openedTexture == nullptr ||
		sharedHandle == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}
	*openedTexture = nullptr;
	*sharedHandle = nullptr;
	InterlockedIncrement(&g_helperAttempts);
	InterlockedExchange(
		&g_lastHelperCreateDeviceResult,
		E_PENDING);
	InterlockedExchange(
		&g_lastHelperCreateTextureResult,
		E_PENDING);
	InterlockedExchange(&g_lastGameOpenResult, E_PENDING);
	PublishHelperDiagnostics(
		BFVRD3D8To9SharedHelperStage::GetCreationParameters,
		E_PENDING);

	IDirect3DDevice9* const gameDevice =
		translatedDevice->GetProxyInterface();
	D3DDEVICE_CREATION_PARAMETERS creation = {};
	IDirect3D9* direct3D9 = nullptr;
	IDirect3D9Ex* direct3D9Ex = nullptr;
	IDirect3DDevice9Ex* helperDevice = nullptr;
	IDirect3DTexture9* helperTexture = nullptr;
	IDirect3DTexture9* gameTexture = nullptr;
	HRESULT result = gameDevice->GetCreationParameters(&creation);
	PublishHelperDiagnostics(
		BFVRD3D8To9SharedHelperStage::GetCreationParameters,
		result);
	if (SUCCEEDED(result))
	{
		PublishHelperDiagnostics(
			BFVRD3D8To9SharedHelperStage::GetDirect3D,
			E_PENDING);
		result = gameDevice->GetDirect3D(&direct3D9);
		PublishHelperDiagnostics(
			BFVRD3D8To9SharedHelperStage::GetDirect3D,
			result);
	}
	if (SUCCEEDED(result) && direct3D9 != nullptr)
	{
		PublishHelperDiagnostics(
			BFVRD3D8To9SharedHelperStage::QueryDirect3D9Ex,
			E_PENDING);
		result = direct3D9->QueryInterface(
			__uuidof(IDirect3D9Ex),
			reinterpret_cast<void**>(&direct3D9Ex));
		PublishHelperDiagnostics(
			BFVRD3D8To9SharedHelperStage::QueryDirect3D9Ex,
			result);
	}
	if (SUCCEEDED(result) && direct3D9Ex != nullptr)
	{
		D3DPRESENT_PARAMETERS presentation = {};
		presentation.BackBufferWidth = 1;
		presentation.BackBufferHeight = 1;
		presentation.BackBufferFormat = D3DFMT_UNKNOWN;
		presentation.BackBufferCount = 1;
		presentation.SwapEffect = D3DSWAPEFFECT_DISCARD;
		presentation.hDeviceWindow = creation.hFocusWindow == nullptr
			? GetDesktopWindow()
			: creation.hFocusWindow;
		presentation.Windowed = TRUE;
		presentation.PresentationInterval =
			D3DPRESENT_INTERVAL_IMMEDIATE;
		PublishHelperDiagnostics(
			BFVRD3D8To9SharedHelperStage::CreateHelperDevice,
			E_PENDING);
		result = direct3D9Ex->CreateDeviceEx(
			creation.AdapterOrdinal,
			creation.DeviceType,
			presentation.hDeviceWindow,
			D3DCREATE_SOFTWARE_VERTEXPROCESSING |
				D3DCREATE_FPU_PRESERVE,
			&presentation,
			nullptr,
			&helperDevice);
		InterlockedExchange(
			&g_lastHelperCreateDeviceResult,
			result);
		PublishHelperDiagnostics(
			BFVRD3D8To9SharedHelperStage::CreateHelperDevice,
			result);
	}

	HANDLE createdHandle = nullptr;
	if (SUCCEEDED(result) && helperDevice != nullptr)
	{
		PublishHelperDiagnostics(
			BFVRD3D8To9SharedHelperStage::CreateHelperTexture,
			E_PENDING);
		result = helperDevice->CreateTexture(
			width,
			height,
			1,
			D3DUSAGE_RENDERTARGET,
			format,
			D3DPOOL_DEFAULT,
			&helperTexture,
			&createdHandle);
		InterlockedExchange(
			&g_lastHelperCreateTextureResult,
			result);
		PublishHelperDiagnostics(
			BFVRD3D8To9SharedHelperStage::CreateHelperTexture,
			result);
	}
	HANDLE openHandle = createdHandle;
	if (SUCCEEDED(result) &&
		helperTexture != nullptr &&
		createdHandle != nullptr)
	{
		PublishHelperDiagnostics(
			BFVRD3D8To9SharedHelperStage::OpenOnGameDevice,
			E_PENDING);
		result = gameDevice->CreateTexture(
			width,
			height,
			1,
			D3DUSAGE_RENDERTARGET,
			format,
			D3DPOOL_DEFAULT,
			&gameTexture,
			&openHandle);
		InterlockedExchange(
			&g_lastGameOpenResult,
			result);
		PublishHelperDiagnostics(
			BFVRD3D8To9SharedHelperStage::OpenOnGameDevice,
			result);
	}
	if (SUCCEEDED(result) && gameTexture != nullptr)
	{
		*openedTexture = gameTexture;
		*sharedHandle = createdHandle;
		gameTexture = nullptr;
		InterlockedIncrement(&g_helperDeviceCreations);
		PublishHelperDiagnostics(
			BFVRD3D8To9SharedHelperStage::Complete,
			D3D_OK);
	}
	else if (SUCCEEDED(result))
	{
		result = E_FAIL;
		PublishHelperDiagnostics(
			BFVRD3D8To9SharedHelperStage::OpenOnGameDevice,
			result);
	}

	if (gameTexture != nullptr)
		gameTexture->Release();
	if (helperTexture != nullptr)
		helperTexture->Release();
	if (helperDevice != nullptr)
		helperDevice->Release();
	if (direct3D9Ex != nullptr)
		direct3D9Ex->Release();
	if (direct3D9 != nullptr)
		direct3D9->Release();
	return result;
}
} // namespace

extern "C" UINT WINAPI BFVRD3D8To9GetSharedBridgeVersion()
{
	return BFVR_D3D8TO9_SHARED_BRIDGE_VERSION;
}

extern "C" HRESULT WINAPI BFVRD3D8To9GetSharedDeviceDiagnostics(
	void* opaqueDevice,
	BFVRD3D8To9SharedDeviceDiagnostics* diagnostics)
{
	if (diagnostics == nullptr ||
		diagnostics->size < sizeof(BFVRD3D8To9SharedDeviceDiagnostics))
	{
		return E_INVALIDARG;
	}

	Direct3DDevice8* translatedDevice = nullptr;
	const HRESULT result =
		ValidateTranslatedDevice(opaqueDevice, &translatedDevice);
	if (FAILED(result))
		return result;

	BFVRD3D8To9SharedDeviceDiagnostics snapshot = {};
	snapshot.size = sizeof(snapshot);
	snapshot.version = BFVR_D3D8TO9_SHARED_BRIDGE_VERSION;
	snapshot.extendedDevice =
		translatedDevice->UsesExtendedDevice() ? TRUE : FALSE;
	snapshot.cooperativeLevel =
		translatedDevice->GetProxyInterface()->TestCooperativeLevel();
	snapshot.helperDeviceCreations =
		InterlockedCompareExchange(
			&g_helperDeviceCreations,
			0,
			0);
	snapshot.helperAttempts =
		InterlockedCompareExchange(
			&g_helperAttempts,
			0,
			0);
	snapshot.lastHelperStage = static_cast<DWORD>(
		InterlockedCompareExchange(
			&g_lastHelperStage,
			0,
			0));
	snapshot.lastHelperResult =
		InterlockedCompareExchange(
			&g_lastHelperResult,
			0,
			0);
	snapshot.lastHelperCreateDeviceResult =
		InterlockedCompareExchange(
			&g_lastHelperCreateDeviceResult,
			0,
			0);
	snapshot.lastHelperCreateTextureResult =
		InterlockedCompareExchange(
			&g_lastHelperCreateTextureResult,
			0,
			0);
	snapshot.lastGameOpenResult =
		InterlockedCompareExchange(
			&g_lastGameOpenResult,
			0,
			0);
	PopulateAdapterDiagnostics(translatedDevice, snapshot);
	*diagnostics = snapshot;
	translatedDevice->Release();
	return D3D_OK;
}

extern "C" HRESULT WINAPI BFVRD3D8To9GetVertexShaderIdentity(
	void* opaqueDevice,
	DWORD d3d8Handle,
	BFVRD3D8To9VertexShaderIdentity* identity)
{
	if (identity == nullptr ||
		identity->size <
			sizeof(BFVRD3D8To9VertexShaderIdentity))
	{
		return E_INVALIDARG;
	}

	Direct3DDevice8* translatedDevice = nullptr;
	const HRESULT result =
		ValidateTranslatedDevice(opaqueDevice, &translatedDevice);
	if (FAILED(result))
		return result;

	BFVRD3D8To9VertexShaderIdentity snapshot = {};
	const HRESULT identityResult =
		translatedDevice->GetVertexShaderIdentity(
			d3d8Handle,
			snapshot);
	if (SUCCEEDED(identityResult))
		*identity = snapshot;
	translatedDevice->Release();
	return identityResult;
}

extern "C" HRESULT WINAPI BFVRD3D8To9CreateSharedRenderTarget(
	void* opaqueDevice,
	UINT width,
	UINT height,
	DWORD d3dFormat,
	HANDLE* sharedHandle,
	void** d3d8Surface)
{
	if (sharedHandle == nullptr || d3d8Surface == nullptr ||
		width == 0 || height == 0)
		return D3DERR_INVALIDCALL;

	*sharedHandle = nullptr;
	*d3d8Surface = nullptr;

	Direct3DDevice8* translatedDevice = nullptr;
	HRESULT result =
		ValidateTranslatedDevice(opaqueDevice, &translatedDevice);
	if (FAILED(result))
		return result;

	const D3DFORMAT format = static_cast<D3DFORMAT>(d3dFormat);
	if (!IsSupportedSharedFormat(format))
	{
		translatedDevice->Release();
		return D3DERR_INVALIDCALL;
	}

	IDirect3DTexture9* texture = nullptr;
	HANDLE handle = nullptr;
	wchar_t forceHelper[2] = {};
	const bool helperForced =
		GetEnvironmentVariableW(
			L"BFVR_D3D8TO9_FORCE_SHARED_HELPER",
			forceHelper,
			static_cast<DWORD>(_countof(forceHelper))) == 1 &&
		forceHelper[0] == L'1';
	result = helperForced
		? D3DERR_INVALIDCALL
		: translatedDevice->GetProxyInterface()->CreateTexture(
			width,
			height,
			1,
			D3DUSAGE_RENDERTARGET,
			format,
			D3DPOOL_DEFAULT,
			&texture,
			&handle);
	if (FAILED(result))
	{
		result = CreateSharedTextureThroughHelper(
			translatedDevice,
			width,
			height,
			format,
			&texture,
			&handle);
	}
	if (FAILED(result) || texture == nullptr || handle == nullptr)
	{
		if (texture != nullptr)
			texture->Release();
		translatedDevice->Release();
		return FAILED(result) ? result : E_FAIL;
	}

	IDirect3DSurface9* surface9 = nullptr;
	result = texture->GetSurfaceLevel(0, &surface9);
	texture->Release();
	if (FAILED(result) || surface9 == nullptr)
	{
		translatedDevice->Release();
		return FAILED(result) ? result : E_FAIL;
	}

	auto* const surface8 =
		translatedDevice->ProxyAddressLookupTable
			->FindAddress<Direct3DSurface8>(surface9);
	if (surface8 == nullptr)
	{
		surface9->Release();
		translatedDevice->Release();
		return E_OUTOFMEMORY;
	}

	*sharedHandle = handle;
	*d3d8Surface = static_cast<IDirect3DSurface8*>(surface8);
	translatedDevice->Release();
	return D3D_OK;
}

extern "C" HRESULT WINAPI BFVRD3D8To9WaitForGpu(
	void* opaqueDevice,
	DWORD timeoutMilliseconds)
{
	Direct3DDevice8* translatedDevice = nullptr;
	HRESULT result =
		ValidateTranslatedDevice(opaqueDevice, &translatedDevice);
	if (FAILED(result))
		return result;

	IDirect3DQuery9* query = nullptr;
	result = translatedDevice->GetProxyInterface()->CreateQuery(
		D3DQUERYTYPE_EVENT,
		&query);
	if (SUCCEEDED(result) && query != nullptr)
		result = query->Issue(D3DISSUE_END);

	const DWORD startedAt = GetTickCount();
	while (SUCCEEDED(result) && query != nullptr)
	{
		result = query->GetData(nullptr, 0, D3DGETDATA_FLUSH);
		if (result != S_FALSE)
			break;
		if (GetTickCount() - startedAt >= timeoutMilliseconds)
		{
			result = D3DERR_WASSTILLDRAWING;
			break;
		}
		SwitchToThread();
	}

	if (query != nullptr)
		query->Release();
	translatedDevice->Release();
	return result;
}

namespace
{
struct ComposeVertex
{
	float x;
	float y;
	float z;
	float rhw;
	float u;
	float v;
};

IDirect3DTexture9* TextureOfSurface(void* surface8)
{
	if (surface8 == nullptr)
		return nullptr;
	auto* const surface =
		static_cast<Direct3DSurface8*>(static_cast<IDirect3DSurface8*>(surface8));
	IDirect3DTexture9* texture = nullptr;
	if (FAILED(surface->GetProxyInterface()->GetContainer(
			IID_IDirect3DTexture9,
			reinterpret_cast<void**>(&texture))))
		return nullptr;
	return texture;
}

HRESULT DrawTexturedQuad(
	IDirect3DDevice9* device,
	IDirect3DTexture9* texture,
	float left,
	float top,
	float right,
	float bottom)
{
	// -0.5 aligns pre-transformed vertices with D3D9 pixel centres.
	const ComposeVertex vertices[4] = {
		{left - 0.5f, top - 0.5f, 0.0f, 1.0f, 0.0f, 0.0f},
		{right - 0.5f, top - 0.5f, 0.0f, 1.0f, 1.0f, 0.0f},
		{left - 0.5f, bottom - 0.5f, 0.0f, 1.0f, 0.0f, 1.0f},
		{right - 0.5f, bottom - 0.5f, 0.0f, 1.0f, 1.0f, 1.0f}};
	device->SetTexture(0, texture);
	return device->DrawPrimitiveUP(
		D3DPT_TRIANGLESTRIP,
		2,
		vertices,
		sizeof(ComposeVertex));
}

void SetComposeStates(IDirect3DDevice9* device)
{
	device->SetVertexShader(nullptr);
	device->SetPixelShader(nullptr);
	device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
	device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
	device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	device->SetRenderState(D3DRS_LIGHTING, FALSE);
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
	device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	device->SetRenderState(D3DRS_FOGENABLE, FALSE);
	device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
	device->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
	device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
	device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
	device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
	device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
	device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
	device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
	device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	device->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE);
}
} // namespace

namespace
{
constexpr char kComposeEffectShader[] = R"(
sampler2D Source : register(s0);
// movement strength, death blend, grading enabled, unused
float4 Vignette : register(c0);
// profile, exposure EV, contrast, saturation
float4 Grading : register(c1);

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
	float3 color = tex2D(Source, uv).rgb;
	if (Vignette.z > 0.5)
	{
		// Same grading as the PC presenter's linear world pass.
		const float3 lumaWeights = float3(0.2126, 0.7152, 0.0722);
		float3 lin = pow(max(color, 0.0001), 2.2) * pow(2.0, Grading.y);
		if (Grading.x > 0.5 && Grading.x < 1.5)
		{
			lin = saturate((lin * (2.51 * lin + 0.03)) / (lin * (2.43 * lin + 0.59) + 0.14));
		}
		else if (Grading.x >= 1.5)
		{
			lin = lin * 1.10 / (1.0 + lin * 0.10);
			lin = lerp(dot(lin, lumaWeights).xxx, lin, 1.18);
		}
		lin = max((lin - 0.18) * (1.0 + Grading.z) + 0.18, 0.0);
		lin = saturate(lerp(dot(lin, lumaWeights).xxx, lin, 1.0 + Grading.w));
		color = pow(max(lin, 0.0001), 1.0 / 2.2);
	}
	// Same aperture as the PC presenter's comfort vignette layer.
	float radius = length(uv * 2.0 - 1.0);
	float strength = saturate(Vignette.x);
	float movement = smoothstep(lerp(1.50, 0.38, strength), lerp(1.80, 0.68, strength), radius);
	float death = saturate(Vignette.y);
	float opacity = lerp(movement, smoothstep(0.16, 0.52, radius), death);
	float3 tint = float3(0.22, 0.012, 0.008) * death;
	return float4(lerp(color, tint, opacity), 1.0);
}
)";

using D3DCompileFn = HRESULT(WINAPI*)(
	LPCVOID source,
	SIZE_T sourceSize,
	LPCSTR sourceName,
	const void* defines,
	void* include,
	LPCSTR entryPoint,
	LPCSTR target,
	UINT flags1,
	UINT flags2,
	ID3DBlob** code,
	ID3DBlob** errors);

// Compiled once per device with the system HLSL compiler (Wine ships one);
// without it the world is composed unchanged.
IDirect3DPixelShader9* GetComposeEffectShader(Direct3DDevice8* translatedDevice)
{
	if (translatedDevice->BFVRComposeShader != nullptr || translatedDevice->BFVRComposeShaderFailed)
		return translatedDevice->BFVRComposeShader;
	translatedDevice->BFVRComposeShaderFailed = true;
	const HMODULE compiler = LoadLibraryW(L"d3dcompiler_47.dll");
	const auto compile = compiler == nullptr
		? nullptr
		: reinterpret_cast<D3DCompileFn>(GetProcAddress(compiler, "D3DCompile"));
	if (compile == nullptr)
		return nullptr;
	for (const char* profile : {"ps_3_0", "ps_2_0"})
	{
		ID3DBlob* code = nullptr;
		ID3DBlob* errors = nullptr;
		const HRESULT compiled = compile(
			kComposeEffectShader,
			sizeof(kComposeEffectShader) - 1,
			"BFVRComposeEffect",
			nullptr,
			nullptr,
			"main",
			profile,
			0,
			0,
			&code,
			&errors);
		if (errors != nullptr)
			errors->Release();
		if (FAILED(compiled) || code == nullptr)
		{
			if (code != nullptr)
				code->Release();
			continue;
		}
		IDirect3DPixelShader9* shader = nullptr;
		const HRESULT created = translatedDevice->GetProxyInterface()->CreatePixelShader(
			static_cast<const DWORD*>(code->GetBufferPointer()),
			&shader);
		code->Release();
		if (SUCCEEDED(created) && shader != nullptr)
		{
			translatedDevice->BFVRComposeShader = shader;
			translatedDevice->BFVRComposeShaderFailed = false;
			return shader;
		}
	}
	return nullptr;
}

bool IsNonZero(float value)
{
	return value > 0.0001f || value < -0.0001f;
}

bool HasColorGrading(const BFVRD3D8To9SideBySideParamsV2* params)
{
	return params->colorProfile > 0.5f || IsNonZero(params->colorExposureEv) ||
		IsNonZero(params->colorContrast) || IsNonZero(params->colorSaturation);
}

bool HasWorldEffects(const BFVRD3D8To9SideBySideParamsV2* params)
{
	return params != nullptr &&
		(params->vignetteStrength > 0.001f || params->vignetteDeathBlend > 0.001f ||
		 HasColorGrading(params));
}
} // namespace

extern "C" HRESULT WINAPI BFVRD3D8To9ComposeSideBySide(
	void* opaqueDevice,
	void* leftSurface8,
	void* rightSurface8,
	void* uiSurface8,
	const BFVRD3D8To9SideBySideParams* params)
{
	if (params == nullptr ||
		params->size < sizeof(BFVRD3D8To9SideBySideParams) ||
		(params->version != BFVR_D3D8TO9_SIDE_BY_SIDE_VERSION &&
		 params->version != BFVR_D3D8TO9_SIDE_BY_SIDE_VERSION_OVERLAYS))
		return E_INVALIDARG;
	const BFVRD3D8To9SideBySideParamsV2* const overlayParams =
		params->version == BFVR_D3D8TO9_SIDE_BY_SIDE_VERSION_OVERLAYS &&
			params->size >= sizeof(BFVRD3D8To9SideBySideParamsV2)
		? reinterpret_cast<const BFVRD3D8To9SideBySideParamsV2*>(params)
		: nullptr;

	Direct3DDevice8* translatedDevice = nullptr;
	HRESULT result = ValidateTranslatedDevice(opaqueDevice, &translatedDevice);
	if (FAILED(result))
		return result;
	IDirect3DDevice9* const device = translatedDevice->GetProxyInterface();

	IDirect3DTexture9* leftTexture = nullptr;
	IDirect3DTexture9* rightTexture = nullptr;
	IDirect3DTexture9* uiTexture = nullptr;
	const bool monoUi = (params->flags & BFVR_D3D8TO9_SIDE_BY_SIDE_MONO_UI) != 0;
	const bool singleEye = (params->flags & BFVR_D3D8TO9_SIDE_BY_SIDE_SINGLE_EYE) != 0;
	if (!monoUi && (params->flags & BFVR_D3D8TO9_SIDE_BY_SIDE_WORLD) != 0)
	{
		leftTexture = TextureOfSurface(leftSurface8);
		if (!singleEye)
			rightTexture = TextureOfSurface(rightSurface8);
	}
	if ((params->flags & BFVR_D3D8TO9_SIDE_BY_SIDE_UI) != 0)
		uiTexture = TextureOfSurface(uiSurface8);

	IDirect3DSurface9* backBuffer = nullptr;
	IDirect3DSurface9* priorTarget = nullptr;
	IDirect3DSurface9* priorDepth = nullptr;
	IDirect3DStateBlock9* savedState = nullptr;
	D3DSURFACE_DESC description = {};
	bool effectsUnavailable = false;

	result = device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);
	if (SUCCEEDED(result))
		result = backBuffer->GetDesc(&description);
	if (SUCCEEDED(result))
		result = device->CreateStateBlock(D3DSBT_ALL, &savedState);
	if (SUCCEEDED(result))
		result = device->GetRenderTarget(0, &priorTarget);
	if (SUCCEEDED(result))
	{
		// A device without a depth-stencil surface is valid.
		device->GetDepthStencilSurface(&priorDepth);
		result = device->SetRenderTarget(0, backBuffer);
	}
	if (SUCCEEDED(result))
	{
		device->SetDepthStencilSurface(nullptr);
		const float width = static_cast<float>(description.Width);
		const float height = static_cast<float>(description.Height);
		const float halfWidth = static_cast<float>(description.Width / 2);
		D3DVIEWPORT9 viewport = {0, 0, description.Width, description.Height, 0.0f, 1.0f};
		device->SetViewport(&viewport);
		const bool sceneStarted = SUCCEEDED(device->BeginScene());
		device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
		SetComposeStates(device);

		const bool effectsRequested =
			(leftTexture != nullptr || rightTexture != nullptr) && HasWorldEffects(overlayParams);
		IDirect3DPixelShader9* const effectShader =
			effectsRequested ? GetComposeEffectShader(translatedDevice) : nullptr;
		effectsUnavailable = effectsRequested && effectShader == nullptr;
		if (effectShader != nullptr)
		{
			const float constants[8] = {
				overlayParams->vignetteStrength,
				overlayParams->vignetteDeathBlend,
				HasColorGrading(overlayParams) ? 1.0f : 0.0f,
				0.0f,
				overlayParams->colorProfile,
				overlayParams->colorExposureEv,
				overlayParams->colorContrast,
				overlayParams->colorSaturation};
			device->SetPixelShader(effectShader);
			device->SetPixelShaderConstantF(0, constants, 2);
		}
		if (leftTexture != nullptr)
			DrawTexturedQuad(device, leftTexture, 0.0f, 0.0f, singleEye ? width : halfWidth, height);
		if (rightTexture != nullptr)
			DrawTexturedQuad(device, rightTexture, halfWidth, 0.0f, width, height);
		if (effectShader != nullptr)
			device->SetPixelShader(nullptr);

		if (uiTexture != nullptr)
		{
			float scale = params->uiScale;
			if (!(scale > 0.05f && scale <= 1.0f))
				scale = 1.0f;
			const bool fullWidthUi = monoUi || singleEye;
			const float regionWidth = fullWidthUi ? width : halfWidth;
			const int regions = fullWidthUi ? 1 : 2;
			float widthScale = overlayParams != nullptr ? overlayParams->uiWidthScale : 0.0f;
			if (!(widthScale > 0.05f && widthScale <= 1.0f))
				widthScale = scale;
			const float panelWidth = regionWidth * widthScale;
			const float panelHeight = height * scale;
			device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
			for (int eye = 0; eye < regions; ++eye)
			{
				const float centerX = regionWidth * (static_cast<float>(eye) + 0.5f);
				const float centerY = height * 0.5f;
				DrawTexturedQuad(
					device,
					uiTexture,
					centerX - panelWidth * 0.5f,
					centerY - panelHeight * 0.5f,
					centerX + panelWidth * 0.5f,
					centerY + panelHeight * 0.5f);
			}
			device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		}

		if (overlayParams != nullptr && overlayParams->overlayCount != 0)
		{
			// BFVR panel art is premultiplied BGRA.
			device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
			device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
			device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
			const UINT count = overlayParams->overlayCount < BFVR_D3D8TO9_MAX_OVERLAY_QUADS
				? overlayParams->overlayCount
				: BFVR_D3D8TO9_MAX_OVERLAY_QUADS;
			for (UINT index = 0; index < count; ++index)
			{
				const BFVRD3D8To9OverlayQuad& quad = overlayParams->overlays[index];
				if (quad.texture == nullptr)
					continue;
				BFVRD3D8To9OverlayVertex vertices[4] = {};
				for (int corner = 0; corner < 4; ++corner)
				{
					vertices[corner] = quad.vertices[corner];
					vertices[corner].x = vertices[corner].x * width - 0.5f;
					vertices[corner].y = vertices[corner].y * height - 0.5f;
				}
				device->SetTexture(0, static_cast<IDirect3DTexture9*>(quad.texture));
				device->DrawPrimitiveUP(
					D3DPT_TRIANGLESTRIP,
					2,
					vertices,
					sizeof(BFVRD3D8To9OverlayVertex));
			}
			device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
			device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		}

		device->SetTexture(0, nullptr);
		if (params->syncColor != 0)
		{
			// A block (not a single pixel) survives scaling to the X screen.
			const D3DRECT syncRect = {0, 0, 8, 8};
			device->Clear(1, &syncRect, D3DCLEAR_TARGET, params->syncColor, 1.0f, 0);
		}
		if (sceneStarted)
			device->EndScene();

		device->SetRenderTarget(0, priorTarget);
		device->SetDepthStencilSurface(priorDepth);
		savedState->Apply();
	}

	if (savedState != nullptr)
		savedState->Release();
	if (priorDepth != nullptr)
		priorDepth->Release();
	if (priorTarget != nullptr)
		priorTarget->Release();
	if (backBuffer != nullptr)
		backBuffer->Release();
	if (uiTexture != nullptr)
		uiTexture->Release();
	if (rightTexture != nullptr)
		rightTexture->Release();
	if (leftTexture != nullptr)
		leftTexture->Release();
	translatedDevice->Release();
	// S_FALSE: composed, but the requested world effects were skipped.
	return SUCCEEDED(result) && effectsUnavailable ? S_FALSE : result;
}

extern "C" HRESULT WINAPI BFVRD3D8To9CreateLocalRenderTarget(
	void* opaqueDevice,
	UINT width,
	UINT height,
	DWORD d3dFormat,
	void** d3d8Surface)
{
	if (d3d8Surface == nullptr || width == 0 || height == 0)
		return D3DERR_INVALIDCALL;
	*d3d8Surface = nullptr;

	Direct3DDevice8* translatedDevice = nullptr;
	HRESULT result = ValidateTranslatedDevice(opaqueDevice, &translatedDevice);
	if (FAILED(result))
		return result;

	IDirect3DTexture9* texture = nullptr;
	result = translatedDevice->GetProxyInterface()->CreateTexture(
		width,
		height,
		1,
		D3DUSAGE_RENDERTARGET,
		static_cast<D3DFORMAT>(d3dFormat),
		D3DPOOL_DEFAULT,
		&texture,
		nullptr);
	if (FAILED(result) || texture == nullptr)
	{
		translatedDevice->Release();
		return FAILED(result) ? result : E_FAIL;
	}

	IDirect3DSurface9* surface9 = nullptr;
	result = texture->GetSurfaceLevel(0, &surface9);
	texture->Release();
	if (FAILED(result) || surface9 == nullptr)
	{
		translatedDevice->Release();
		return FAILED(result) ? result : E_FAIL;
	}

	auto* const surface8 =
		translatedDevice->ProxyAddressLookupTable
			->FindAddress<Direct3DSurface8>(surface9);
	translatedDevice->Release();
	if (surface8 == nullptr)
	{
		surface9->Release();
		return E_OUTOFMEMORY;
	}
	*d3d8Surface = static_cast<IDirect3DSurface8*>(surface8);
	return D3D_OK;
}

extern "C" HRESULT WINAPI BFVRD3D8To9CreateOverlayTexture(
	void* opaqueDevice,
	UINT width,
	UINT height,
	void** overlayTexture)
{
	if (overlayTexture == nullptr || width == 0 || height == 0)
		return D3DERR_INVALIDCALL;
	*overlayTexture = nullptr;
	Direct3DDevice8* translatedDevice = nullptr;
	HRESULT result = ValidateTranslatedDevice(opaqueDevice, &translatedDevice);
	if (FAILED(result))
		return result;
	IDirect3DTexture9* texture = nullptr;
	result = translatedDevice->GetProxyInterface()->CreateTexture(
		width,
		height,
		1,
		D3DUSAGE_DYNAMIC,
		D3DFMT_A8R8G8B8,
		D3DPOOL_DEFAULT,
		&texture,
		nullptr);
	translatedDevice->Release();
	if (FAILED(result) || texture == nullptr)
		return FAILED(result) ? result : E_FAIL;
	*overlayTexture = texture;
	return D3D_OK;
}

extern "C" HRESULT WINAPI BFVRD3D8To9UpdateOverlayTexture(
	void* overlayTexture,
	const DWORD* pixels,
	UINT width,
	UINT height)
{
	if (overlayTexture == nullptr || pixels == nullptr)
		return D3DERR_INVALIDCALL;
	auto* const texture = static_cast<IDirect3DTexture9*>(overlayTexture);
	D3DSURFACE_DESC description = {};
	HRESULT result = texture->GetLevelDesc(0, &description);
	if (FAILED(result))
		return result;
	if (description.Width != width || description.Height != height)
		return D3DERR_INVALIDCALL;
	D3DLOCKED_RECT locked = {};
	result = texture->LockRect(0, &locked, nullptr, D3DLOCK_DISCARD);
	if (FAILED(result))
		return result;
	for (UINT row = 0; row < height; ++row)
	{
		std::memcpy(
			static_cast<BYTE*>(locked.pBits) + static_cast<size_t>(row) * locked.Pitch,
			pixels + static_cast<size_t>(row) * width,
			static_cast<size_t>(width) * sizeof(DWORD));
	}
	return texture->UnlockRect(0);
}

extern "C" void WINAPI BFVRD3D8To9ReleaseOverlayTexture(void* overlayTexture)
{
	if (overlayTexture != nullptr)
		static_cast<IDirect3DTexture9*>(overlayTexture)->Release();
}
