/**
 * BFVR extension to pinned d3d8to9 v1.15.1.
 *
 * This ABI is intentionally C-shaped and uses opaque D3D8 pointers so the
 * BFVR client can resolve it dynamically without importing D3D9.
 */
#pragma once

#include <windows.h>

constexpr UINT BFVR_D3D8TO9_SHARED_BRIDGE_VERSION = 8;
constexpr DWORD BFVR_D3D8TO9_DEPTH_EXPORT_TIMING_VERSION = 1;
constexpr DWORD BFVR_D3DFMT_INTZ =
	static_cast<DWORD>('I') |
	(static_cast<DWORD>('N') << 8) |
	(static_cast<DWORD>('T') << 16) |
	(static_cast<DWORD>('Z') << 24);

enum class BFVRD3D8To9SharedHelperStage : DWORD
{
	NotAttempted = 0,
	GetCreationParameters = 1,
	GetDirect3D = 2,
	QueryDirect3D9Ex = 3,
	CreateHelperDevice = 4,
	CreateHelperTexture = 5,
	OpenOnGameDevice = 6,
	Complete = 7,
};

// Implemented only by this BFVR-patched d3d8to9 device wrapper. The exported
// bridge functions use it to reject native or foreign IDirect3DDevice8 objects
// before accessing the translator implementation.
inline constexpr GUID IID_BFVRD3D8To9Device =
{ 0x9a901225, 0xaf89, 0x472a, { 0x88, 0xa1, 0xcd, 0xe4, 0x09, 0x44, 0x7f, 0xa2 } };

using BFVRD3D8To9GetSharedBridgeVersionFn =
	UINT(WINAPI*)();

using BFVRD3D8To9CreateSharedRenderTargetFn =
	HRESULT(WINAPI*)(
		void* d3d8Device,
		UINT width,
		UINT height,
		DWORD d3dFormat,
		HANDLE* sharedHandle,
		void** d3d8Surface);

using BFVRD3D8To9WaitForGpuFn =
	HRESULT(WINAPI*)(
		void* d3d8Device,
		DWORD timeoutMilliseconds);

enum class BFVRD3D8To9DepthExportEncoding : DWORD
{
	PackedRgba8 = 1,
	FloatRgba16 = 2,
};

struct BFVRD3D8To9DepthExportTiming
{
	DWORD size;
	DWORD version;
	BOOL gpuTimestampsValid;
	BOOL gpuTimestampDisjoint;
	ULONGLONG timestampFrequency;
	ULONGLONG elapsedTicks;
	double elapsedMilliseconds;
};

using BFVRD3D8To9CreateTextureBackedDepthStencilFn =
	HRESULT(WINAPI*)(
		void* d3d8Device,
		UINT width,
		UINT height,
		DWORD renderTargetFormat,
		void** d3d8DepthSurface);

using BFVRD3D8To9ResolveDepthToSharedTargetFn =
	HRESULT(WINAPI*)(
		void* d3d8Device,
		void* d3d8DepthSurface,
		void* d3d8TargetSurface,
		DWORD encoding,
		BFVRD3D8To9DepthExportTiming* timing);

struct BFVRD3D8To9SharedDeviceDiagnostics
{
	DWORD size;
	DWORD version;
	BOOL extendedDevice;
	HRESULT cooperativeLevel;
	LONG helperDeviceCreations;
	LONG helperAttempts;
	DWORD lastHelperStage;
	HRESULT lastHelperResult;
	HRESULT lastHelperCreateDeviceResult;
	HRESULT lastHelperCreateTextureResult;
	HRESULT lastGameOpenResult;
	UINT adapterOrdinal;
	LONG adapterLuidHigh;
	DWORD adapterLuidLow;
	HRESULT getCreationParametersResult;
	HRESULT getDirect3DResult;
	HRESULT queryDirect3D9ExResult;
	HRESULT getAdapterLuidResult;
};

using BFVRD3D8To9GetSharedDeviceDiagnosticsFn =
	HRESULT(WINAPI*)(
		void* d3d8Device,
		BFVRD3D8To9SharedDeviceDiagnostics* diagnostics);

struct BFVRD3D8To9VertexShaderIdentity
{
	DWORD size;
	DWORD version;
	DWORD d3d8Handle;
	BOOL programmable;
	DWORD originalFunctionByteCount;
	DWORD creationOrdinal;
	ULONGLONG originalFunctionHash;
};

using BFVRD3D8To9GetVertexShaderIdentityFn =
	HRESULT(WINAPI*)(
		void* d3d8Device,
		DWORD d3d8Handle,
		BFVRD3D8To9VertexShaderIdentity* identity);

// Optional: a process-local (non-shared) default-pool render-target texture,
// for in-process presenters that never hand the target to another device.
using BFVRD3D8To9CreateLocalRenderTargetFn =
	HRESULT(WINAPI*)(
		void* d3d8Device,
		UINT width,
		UINT height,
		DWORD d3dFormat,
		void** d3d8Surface);

// Optional side-by-side composition for WinlatorXR (standalone headsets).
// Draws the two world eye targets into the left and right halves of the
// current back buffer, blends the UI target over each eye, and writes the
// WinlatorXR frame-sync block at pixel (0,0). All device state, the render
// target and the depth-stencil surface are restored before returning.
constexpr DWORD BFVR_D3D8TO9_SIDE_BY_SIDE_VERSION = 1;
constexpr DWORD BFVR_D3D8TO9_SIDE_BY_SIDE_WORLD = 0x1;
constexpr DWORD BFVR_D3D8TO9_SIDE_BY_SIDE_UI = 0x2;
// Draw the UI once across the whole back buffer instead of per eye (for a
// flat virtual-screen presentation of menus). The world is not drawn.
constexpr DWORD BFVR_D3D8TO9_SIDE_BY_SIDE_MONO_UI = 0x4;
// Alternate-eye presentation: draw leftSurface8 (the eye rendered this frame)
// across the whole back buffer and the UI once over it.
constexpr DWORD BFVR_D3D8TO9_SIDE_BY_SIDE_SINGLE_EYE = 0x8;

struct BFVRD3D8To9SideBySideParams
{
	DWORD size;
	DWORD version;
	DWORD flags;
	// Fraction of each eye's width and height covered by the UI panel.
	float uiScale;
	// D3DCOLOR of the 8x8 frame-sync block; 0 draws no block.
	DWORD syncColor;
};

using BFVRD3D8To9ComposeSideBySideFn =
	HRESULT(WINAPI*)(
		void* d3d8Device,
		void* leftSurface8,
		void* rightSurface8,
		void* uiSurface8,
		const BFVRD3D8To9SideBySideParams* params);

// Optional overlay textures and quads for BFVR-owned panels on WinlatorXR
// (Quick Menu, VR Settings). Textures are dynamic default-pool A8R8G8B8 and
// must be released before the device is Reset.
using BFVRD3D8To9CreateOverlayTextureFn =
	HRESULT(WINAPI*)(
		void* d3d8Device,
		UINT width,
		UINT height,
		void** overlayTexture);

// pixels: premultiplied 0xAARRGGBB, width x height, top row first.
using BFVRD3D8To9UpdateOverlayTextureFn =
	HRESULT(WINAPI*)(
		void* overlayTexture,
		const DWORD* pixels,
		UINT width,
		UINT height);

using BFVRD3D8To9ReleaseOverlayTextureFn =
	void(WINAPI*)(void* overlayTexture);

constexpr DWORD BFVR_D3D8TO9_SIDE_BY_SIDE_VERSION_OVERLAYS = 2;
constexpr UINT BFVR_D3D8TO9_MAX_OVERLAY_QUADS = 16;

// Pre-transformed back-buffer vertex. x and y are normalized to the back
// buffer (0,0 top-left, 1,1 bottom-right); z in [0,1]; rhw = 1/w keeps
// perspective-correct texturing.
struct BFVRD3D8To9OverlayVertex
{
	float x;
	float y;
	float z;
	float rhw;
	float u;
	float v;
};

// Triangle-strip order: bottom-left, top-left, bottom-right, top-right.
struct BFVRD3D8To9OverlayQuad
{
	void* texture;
	BFVRD3D8To9OverlayVertex vertices[4];
};

// Version 2 parameters: version 1 fields, world effects, and overlay quads,
// which are drawn after the UI with premultiplied-alpha blending.
struct BFVRD3D8To9SideBySideParamsV2
{
	BFVRD3D8To9SideBySideParams base;
	// Comfort vignette over each world eye: movement aperture 0..1 and the
	// dark-red death-camera blend 0..1.
	float vignetteStrength;
	float vignetteDeathBlend;
	// Color grading of the world: profile 0 original, 1 filmic, 2 vibrant;
	// exposure in EV; contrast and saturation as offsets around 0. Neutral
	// values leave the world untouched.
	float colorProfile;
	float colorExposureEv;
	float colorContrast;
	float colorSaturation;
	UINT overlayCount;
	BFVRD3D8To9OverlayQuad overlays[BFVR_D3D8TO9_MAX_OVERLAY_QUADS];
};
