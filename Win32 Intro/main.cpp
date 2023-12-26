#include <Windows.h>
#include <d3d11.h>
#include <iostream>

#include "ReadData.h"
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <d3d11shader.h>
#include <d3dcompiler.h>
#include <WICTextureLoader.h>
#include "text2D.h"

#include <Mouse.h>
#include <Keyboard.h>

#include "objfilemodel.h"
#include <DDSTextureLoader.h>

using namespace DirectX;

// Global variables
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define MAX_POINT_LIGHTS 8

HINSTANCE g_hInstance = NULL;
HWND	  g_hWnd	  = NULL;
const wchar_t* windowName = L"Hello World";

IDXGISwapChain*			g_swapChain  = NULL;
ID3D11Device*			g_device	 = NULL;
ID3D11DeviceContext*	g_context	 = NULL;
ID3D11RenderTargetView* g_backBuffer = NULL;
ID3D11DepthStencilView* g_ZBuffer	 = NULL;

ID3D11VertexShader*     pVS          = NULL;
ID3D11PixelShader*      pPS          = NULL;
ID3D11InputLayout*      pLayout      = NULL;

ID3D11Buffer* pVBuffer = NULL;
ID3D11Buffer* pIBuffer = NULL;
ID3D11Buffer* pCBuffer = NULL;

ID3D11ShaderResourceView* pTexture = NULL;
ID3D11SamplerState* pSampler	   = NULL;

Text2D* pText;
ID3D11BlendState* pAlphaBlendStateEnable = NULL;
ID3D11BlendState* pAlphaBlendStateDisable = NULL;

// Skybox
ID3D11RasterizerState* pRasterSolid = NULL;
ID3D11RasterizerState* pRasterSkybox = NULL;
ID3D11DepthStencilState* pDepthWriteSolid = NULL;
ID3D11DepthStencilState* pDepthWriteSkybox = NULL;
ID3D11Buffer* pSkyboxCBuffer = NULL;
ID3D11ShaderResourceView* pSkyboxTexture = NULL;
ID3D11VertexShader* pSkyboxVS = NULL;
ID3D11PixelShader* pSkyboxPS = NULL;
ID3D11InputLayout* pSkyboxLayout = NULL;

struct SkyboxCBuffer
{
	XMMATRIX WVP;
};

#pragma region Object WVP

struct Transform
{
	XMFLOAT3 pos{ 0,0,2 };
	XMFLOAT3 rot{ 0,0,0 };
	XMFLOAT3 scl{ 0.5,0.5,0.5 };

	XMMATRIX GetWorldMaxtrix()
	{
		const XMMATRIX translation = XMMatrixTranslation(pos.x, pos.y, pos.z);
		const XMMATRIX rotation = XMMatrixRotationRollPitchYaw(rot.x, rot.y, rot.z);
		const XMMATRIX scale = XMMatrixScaling(scl.x, scl.y, scl.z);
		XMMATRIX world = scale * rotation * translation;
		return world;
	}
};

Transform cube1, cube2;

#pragma endregion

struct Vertex
{
	XMFLOAT3 Position;
	XMFLOAT4 Color;
	XMFLOAT2 UV;
	XMFLOAT3 Normal;
};


struct Camera
{
	float x = 0, y = 0, z = 0;
	float pitch = XM_PIDIV2, yaw = 0;

	XMMATRIX GetViewMatrix()
	{
		XMVECTOR eyePos { x, y, z };
		XMVECTOR camUp  { 0, 1, 0 };
		XMVECTOR lookTo{ sin(yaw) * sin(pitch),
						 cos(pitch), 
						 cos(yaw) * sin(pitch) };
		XMMATRIX view = XMMatrixLookToLH(eyePos, lookTo, camUp);
		return view;
	}
} g_camera;

Mouse mouse;
Mouse::ButtonStateTracker mouseTracker;
Keyboard keyboard;
Keyboard::KeyboardStateTracker kbTracker;

// Lighting
XMVECTOR ambientLightColour = { 0.1f, 0.1f, 0.1f, 1.0f };
XMVECTOR directionalLightShinesFrom = { 0.2788f, 0.7063f, 0.6506f };
XMVECTOR directionalLightColour = { 0.96f, 0.8f, 0.75f, 1.0f };


struct PointLight
{
	XMVECTOR position = { 0.0f, 1.0f, -1.0f }; // 16 bytes
	XMVECTOR colour = { 0.85f, 0.0f, 0.85f, 1.0f }; // 16 bytes
	float strength = 10.0f; // 4 bytes
	BOOL active = false; // 4 bytes
	float padding[2]; // 8 bytes
};

PointLight pointLights[MAX_POINT_LIGHTS];

// Object loading
ObjFileModel* model;

struct CBUFFER0
{
	XMMATRIX WVP;
	XMVECTOR ambientLightColour;
	XMVECTOR directionalLightDir;
	XMVECTOR directionalLightCol;
	
	PointLight pointLights[MAX_POINT_LIGHTS];
};

// Forward declarations
HRESULT InitWindow(HINSTANCE instanceHandle, int nCmdShow);
LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
HRESULT InitD3D(HWND hWnd);
HRESULT InitPipeline();
void    CleanD3D();
void	OpenConsole();
void	RenderFrame();
void	InitGraphics();
void	HandleInput();
void	InitScene();

HRESULT LoadVertexShader(LPCWSTR fileName, LPCSTR entrypoint, ID3D11VertexShader** vs, ID3D11InputLayout** il);
HRESULT LoadPixelShader(LPCWSTR fileName, LPCSTR entrypoint, ID3D11PixelShader** ps);

HRESULT LoadCompiledVertexShader(LPCWSTR fileName, ID3D11VertexShader** vs, ID3D11InputLayout** il);
HRESULT LoadCompiledPixelShader(LPCWSTR fileName, ID3D11PixelShader** ps);

void DrawSkybox();

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE prevInstanceHandle, _In_ LPSTR cmdLine, _In_ int nCmdShow)
{
	if (FAILED(InitWindow(hInstance, nCmdShow)))
		MessageBox(NULL, L"Failed to create window", L"Error", MB_OK | MB_ICONERROR);

	if (FAILED(InitD3D(g_hWnd)))
		MessageBox(NULL, L"Failed to create D3D device", L"Error", MB_OK | MB_ICONERROR);

	InitGraphics();

#if (_DEBUG)
	//OpenConsole();
#endif

	Mouse::Get().SetWindow(g_hWnd);
	Mouse::Get().SetMode(Mouse::Mode::MODE_RELATIVE);

	MSG msg;

	InitScene();

	while (true)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT)
				break;
		}
		else
		{
			// Update and render game
			HandleInput();
			RenderFrame();
		}
	}
	CleanD3D();
	return 0;
}

HRESULT InitWindow(HINSTANCE instanceHandle, int nCmdShow)
{
	g_hInstance = instanceHandle;

	// Register class
	WNDCLASSEX wcex = {};
	wcex.cbSize			= sizeof(WNDCLASSEX);
	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WindowProc;
	wcex.hInstance		= instanceHandle;
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszClassName	= windowName;

	if (!RegisterClassEx(&wcex))
		return E_FAIL;

	RECT wr = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
	AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

	// Create window
	g_hWnd = CreateWindow(
		windowName,
		windowName,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		wr.right - wr.left, wr.bottom - wr.top,
		NULL,
		NULL,
		instanceHandle,
		NULL);

	if (!g_hWnd)
		return E_FAIL;

	ShowWindow(g_hWnd, nCmdShow);

	return S_OK;
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_ACTIVATE:
	case WM_ACTIVATEAPP:
	case WM_INPUT:
		Keyboard::ProcessMessage(message, wParam, lParam);
		Mouse::ProcessMessage(message, wParam, lParam);
		break;

	case WM_SYSKEYDOWN:
		if (wParam == VK_RETURN && (lParam & 0x60000000) == 0x20000000)
		{
			// Alt + Enter
			// Toggle fullscreen
			BOOL fullscreen;
			g_swapChain->GetFullscreenState(&fullscreen, NULL);
			g_swapChain->SetFullscreenState(!fullscreen, NULL);
		}
		Keyboard::ProcessMessage(message, wParam, lParam);

	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYUP:
		Keyboard::ProcessMessage(message, wParam, lParam);
		break;

	case WM_MOUSEACTIVATE:
		return MA_ACTIVATEANDEAT;

	case WM_MOUSEMOVE:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MOUSEWHEEL:
	case WM_XBUTTONDOWN:
	case WM_XBUTTONUP:
	case WM_MOUSEHOVER:
		Mouse::ProcessMessage(message, wParam, lParam);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
		break;
	}

	//CleanD3D();

	return 0;
}

HRESULT InitD3D(HWND hWnd)
{
	DXGI_SWAP_CHAIN_DESC scd = {};

	scd.BufferCount							    = 1;
	scd.BufferDesc.Format					    = DXGI_FORMAT_R8G8B8A8_UNORM;
	scd.BufferDesc.Width					    = SCREEN_WIDTH;
	scd.BufferDesc.Height					    = SCREEN_HEIGHT;
	scd.BufferDesc.RefreshRate.Numerator		= 60;
	scd.BufferDesc.RefreshRate.Denominator		= 1;
	scd.BufferUsage							    = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scd.OutputWindow						    = hWnd;
	scd.SampleDesc.Count					    = 1;
	scd.Windowed							    = TRUE;
	scd.Flags								    = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	HRESULT hr;

	hr = D3D11CreateDeviceAndSwapChain(
		NULL,
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,
		D3D11_CREATE_DEVICE_DEBUG,
		NULL,
		NULL,
		D3D11_SDK_VERSION,
		&scd,
		&g_swapChain,
		&g_device,
		NULL,
		&g_context);

	if (FAILED(hr))
		return hr;

	// Setup for back buffer
	ID3D11Texture2D* pBackBufferTexture = nullptr;
	hr = g_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBufferTexture);

	if (FAILED(hr))
		return hr;

	// Using back buffer, create render target view
	hr = g_device->CreateRenderTargetView(pBackBufferTexture, NULL, &g_backBuffer);

	if (FAILED(hr))
		return hr;

	pBackBufferTexture->Release();

	// Set render target
	D3D11_TEXTURE2D_DESC tex2dDesc = {0};
	tex2dDesc.Width				= SCREEN_WIDTH;
	tex2dDesc.Height			= SCREEN_HEIGHT;
	tex2dDesc.ArraySize			= 1;
	tex2dDesc.MipLevels			= 1;
	tex2dDesc.Format 	   	    = DXGI_FORMAT_D24_UNORM_S8_UINT;
	tex2dDesc.SampleDesc.Count	= scd.SampleDesc.Count;
	tex2dDesc.BindFlags			= D3D11_BIND_DEPTH_STENCIL;
	tex2dDesc.Usage				= D3D11_USAGE_DEFAULT;

	ID3D11Texture2D* pZBufferTexture = nullptr;
	hr = g_device->CreateTexture2D(&tex2dDesc, NULL, &pZBufferTexture);
	if (FAILED(hr))
	{
		OutputDebugString(L"Failed to create Z buffer texture");
		return E_FAIL;
	}

	D3D11_DEPTH_STENCIL_VIEW_DESC dsvd;
	ZeroMemory(&dsvd, sizeof(D3D11_DEPTH_STENCIL_VIEW_DESC));
	dsvd.Format				= tex2dDesc.Format;
	dsvd.ViewDimension		= D3D11_DSV_DIMENSION_TEXTURE2D;
	hr = g_device->CreateDepthStencilView(pZBufferTexture, &dsvd, &g_ZBuffer);
	if (FAILED(hr))
	{
		OutputDebugString(L"Failed to create Z buffer");
		return E_FAIL;
	}
	pZBufferTexture->Release();

	
	g_context->OMSetRenderTargets(1, &g_backBuffer, g_ZBuffer);

	// Setup viewport
	D3D11_VIEWPORT vp = {};
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	vp.Width	= SCREEN_WIDTH;
	vp.Height	= SCREEN_HEIGHT;
	vp.MinDepth = 0;
	vp.MaxDepth = 1;
	g_context->RSSetViewports(1, &vp);

	// Setup pipeline
	InitPipeline();

	pText = new Text2D("font1.png", g_device, g_context);

	// Alpha blending
	D3D11_BLEND_DESC blendDesc = {0};
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].BlendOp	= D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlend	 = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend	= D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.AlphaToCoverageEnable = FALSE;
	g_device->CreateBlendState(&blendDesc, &pAlphaBlendStateEnable);

	blendDesc.RenderTarget[0].BlendEnable = FALSE;
	g_device->CreateBlendState(&blendDesc, &pAlphaBlendStateDisable);

	// Skybox
	D3D11_RASTERIZER_DESC rasterDesc;
	ZeroMemory(&rasterDesc, sizeof(D3D11_RASTERIZER_DESC));
	rasterDesc.CullMode				= D3D11_CULL_BACK;
	rasterDesc.FillMode				= D3D11_FILL_SOLID;
	g_device->CreateRasterizerState(&rasterDesc, &pRasterSolid);
	rasterDesc.CullMode				= D3D11_CULL_FRONT;
	g_device->CreateRasterizerState(&rasterDesc, &pRasterSkybox);

	D3D11_DEPTH_STENCIL_DESC depthDesc = { 0 };
	depthDesc.DepthEnable = true;
	depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depthDesc.DepthFunc = D3D11_COMPARISON_LESS;
	g_device->CreateDepthStencilState(&depthDesc, &pDepthWriteSolid);
	depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	g_device->CreateDepthStencilState(&depthDesc, &pDepthWriteSkybox);

	return S_OK;
}

void CleanD3D()
{
	if (pAlphaBlendStateDisable) pAlphaBlendStateDisable->Release();
	if (pAlphaBlendStateEnable)  pAlphaBlendStateEnable->Release();
	if (g_backBuffer)			 g_backBuffer->Release();
	if (g_swapChain)			 g_swapChain->Release();
	if (g_context)				 g_context->Release();
	if (g_ZBuffer)				 g_ZBuffer->Release();
	if (g_device)				 g_device->Release();
	if (pVBuffer)				 pVBuffer->Release();
	if (pIBuffer)				 pIBuffer->Release();
	if (pCBuffer)				 pCBuffer->Release();
	if (pTexture)				 pTexture->Release();
	if (pSampler)				 pSampler->Release();
	if (pLayout)				 pLayout->Release();
	if (pVS)					 pVS->Release();
	if (pPS)					 pPS->Release();
	delete pText;
	delete model;

	// Skybox
	if (pRasterSolid) pRasterSolid->Release();
	if (pRasterSkybox) pRasterSkybox->Release();
	if (pDepthWriteSolid) pDepthWriteSolid->Release();
	if (pDepthWriteSkybox) pDepthWriteSkybox->Release();
	if (pSkyboxCBuffer) pSkyboxCBuffer->Release();
	if (pSkyboxTexture) pSkyboxTexture->Release();
	if (pSkyboxVS) pSkyboxVS->Release();
	if (pSkyboxPS) pSkyboxPS->Release();
}

void OpenConsole()
{
	if (AllocConsole())
	{
		FILE* fp = nullptr;
		freopen_s(&fp, "CONOUT$", "w", stdout);
		freopen_s(&fp, "CONOUT$", "w", stderr);
		freopen_s(&fp, "CONIN$", "r", stdin);
		std::ios::sync_with_stdio(true);
	}
}

void RenderFrame()
{
	// Clear back buffer
	g_context->ClearRenderTargetView(g_backBuffer, Colors::DarkSlateBlue);
	g_context->ClearDepthStencilView(g_ZBuffer, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	DrawSkybox();
	
	// Set vertex buffer, index buffer, primitive topology (per mesh)
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	g_context->IASetVertexBuffers(0, 1, &pVBuffer, &stride, &offset);
	g_context->IASetIndexBuffer(pIBuffer, DXGI_FORMAT_R32_UINT, 0);
   	g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Set texture and sampler
	g_context->PSSetSamplers(0, 1, &pSampler);
	g_context->PSSetShaderResources(0, 1, &pTexture);

	// View and projection matrices
	XMMATRIX world;// = cube1.GetWorldMaxtrix();
	XMMATRIX view = g_camera.GetViewMatrix();
	XMMATRIX projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(60), (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT, 0.1f, 100.0f);


	// World matrix and constant buffer (per object)
	world = cube1.GetWorldMaxtrix();
	CBUFFER0 cBuffer;
	cBuffer.WVP = world * view * projection;

	// Lighting
	cBuffer.ambientLightColour = ambientLightColour;
	cBuffer.directionalLightCol = directionalLightColour;
	XMMATRIX transpose = XMMatrixTranspose(world);
	cBuffer.directionalLightDir = XMVector3Transform(directionalLightShinesFrom, transpose);

	// Point lights
	for (size_t i = 0; i < MAX_POINT_LIGHTS; ++i)
	{
		if (!pointLights[i].active)
			continue;

		XMMATRIX inverse = XMMatrixInverse(nullptr, world);

		cBuffer.pointLights[i].position = XMVector3Transform(pointLights[i].position, inverse);
		cBuffer.pointLights[i].colour = pointLights[i].colour;
		cBuffer.pointLights[i].strength = pointLights[i].strength;
		cBuffer.pointLights[i].active = pointLights[i].active;
	}


	// Update constant buffer
	g_context->UpdateSubresource(pCBuffer, 0, 0, &cBuffer, 0, 0);
	g_context->VSSetConstantBuffers(0, 1, &pCBuffer);

	//g_context->DrawIndexed(36, 0, 0);
	model->Draw();


	world = cube2.GetWorldMaxtrix();
	cBuffer.WVP = world * view * projection;
	g_context->UpdateSubresource(pCBuffer, 0, 0, &cBuffer, 0, 0);
	g_context->VSSetConstantBuffers(0, 1, &pCBuffer);
	//g_context->DrawIndexed(36, 0, 0);
	model->Draw();

	// Text
	g_context->OMSetBlendState(pAlphaBlendStateEnable, NULL, 0xffffffff);
	pText->AddText("Hello World", -1, +1, 0.075f);
	pText->RenderText();
	g_context->OMSetBlendState(pAlphaBlendStateDisable, NULL, 0xffffffff);

	g_swapChain->Present(0, 0);

	static float fakeTime = 0;
	fakeTime += 0.0001f;
	cube1.pos.x = sin(fakeTime);
	cube1.pos.y = cos(fakeTime);

	cube1.rot.x = fakeTime;
	cube1.rot.y = fakeTime;
}

HRESULT InitPipeline()
{
	/*{
		HRESULT result;

		auto vertexShaderBytecode = DX::ReadData(L"CompiledShaders/VertexShader.cso");
		auto pixelShaderBytecode = DX::ReadData(L"CompiledShaders/PixelShader.cso");

		g_device->CreateVertexShader(vertexShaderBytecode.data(), vertexShaderBytecode.size(), NULL, &pVS);
		g_device->CreatePixelShader(pixelShaderBytecode.data(), pixelShaderBytecode.size(), NULL, &pPS);

		g_context->VSSetShader(pVS, 0, 0);
		g_context->PSSetShader(pPS, 0, 0);

		// Shader reflection
		ID3D11ShaderReflection* vShaderReflection = nullptr;
		D3DReflect(vertexShaderBytecode.data(), vertexShaderBytecode.size(), IID_ID3D11ShaderReflection, (void**)&vShaderReflection);

		D3D11_SHADER_DESC shaderDesc;
		vShaderReflection->GetDesc(&shaderDesc);

		auto paramDesc = new D3D11_SIGNATURE_PARAMETER_DESC[shaderDesc.InputParameters]{ 0 };
		for (UINT i = 0; i < shaderDesc.InputParameters; i++)
		{
			vShaderReflection->GetInputParameterDesc(i, &paramDesc[i]);
		}

		auto ied = new D3D11_INPUT_ELEMENT_DESC[shaderDesc.InputParameters]{ 0 };
		for (size_t i = 0; i < shaderDesc.InputParameters; i++)
		{
			ied[i].SemanticName = paramDesc[i].SemanticName;
			ied[i].SemanticIndex = paramDesc[i].SemanticIndex;

			if (paramDesc[i].ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
			{
				switch (paramDesc[i].Mask)
				{
				case 1:
					ied[i].Format = DXGI_FORMAT_R32_FLOAT;
					break;

				case 3:
					ied[i].Format = DXGI_FORMAT_R32G32_FLOAT;
					break;

				case 7:
					ied[i].Format = DXGI_FORMAT_R32G32B32_FLOAT;
					break;

				case 15:
					ied[i].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
					break;

				default:
					break;
				}
			}

			ied[i].InputSlot = 0;
			ied[i].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
			ied[i].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			ied[i].InstanceDataStepRate = 0;

		}

		result = g_device->CreateInputLayout(ied, shaderDesc.InputParameters, vertexShaderBytecode.data(), vertexShaderBytecode.size(), &pLayout);
		if (FAILED(result))
		{
			OutputDebugString(L"Failed to create input layout");
			std::cout << "Failed to create input layout\n";
			return result;
		}
		OutputDebugString(L"Successfully created input layout\n");

		g_context->IASetInputLayout(pLayout);

		delete[] paramDesc;
		delete[] ied;
	}*/

	LoadCompiledVertexShader(L"CompiledShaders/VertexShader.cso", &pVS, &pLayout);
	LoadCompiledPixelShader(L"CompiledShaders/PixelShader.cso", &pPS);

	LoadVertexShader(L"SkyboxVShader.hlsl", "main", &pSkyboxVS, &pSkyboxLayout);
	LoadPixelShader(L"SkyboxPShader.hlsl", "main", &pSkyboxPS);
	
	return S_OK;
}

void InitGraphics()
{
	Vertex vertices[] =
	{
		//          x,    y,       z,              r,    g,     b,     a,               u,    v,                 nx,       ny,       nz
		{XMFLOAT3{-0.5f, -0.5f, -0.5f}, XMFLOAT4{1.0f,  0.0f,  0.0f,  1.0f}, XMFLOAT2{ 0.0f, 1.0f }, XMFLOAT3{-0.5773f, -0.5773f, -0.5773f}},  // Front BL
		{XMFLOAT3{-0.5f,  0.5f, -0.5f}, XMFLOAT4{0.0f,  1.0f,  0.0f,  1.0f}, XMFLOAT2{ 0.0f, 0.0f }, XMFLOAT3{-0.5773f,  0.5773f, -0.5773f}},  // Front TL
		{XMFLOAT3{ 0.5f,  0.5f, -0.5f}, XMFLOAT4{0.0f,  0.0f,  1.0f,  1.0f}, XMFLOAT2{ 1.0f, 0.0f }, XMFLOAT3{ 0.5773f,  0.5773f, -0.5773f}},  // Front TR
		{XMFLOAT3{ 0.5f, -0.5f, -0.5f}, XMFLOAT4{1.0f,  1.0f,  1.0f,  1.0f}, XMFLOAT2{ 1.0f, 1.0f }, XMFLOAT3{ 0.5773f, -0.5773f, -0.5773f}},  // Front BR
																								   
		{XMFLOAT3{-0.5f, -0.5f,  0.5f}, XMFLOAT4{0.0f,  1.0f,  1.0f,  1.0f}, XMFLOAT2{ 0.0f, 1.0f }, XMFLOAT3{-0.5773f, -0.5773f,  0.5773f}},  // Back BL
		{XMFLOAT3{-0.5f,  0.5f,  0.5f}, XMFLOAT4{1.0f,  0.0f,  1.0f,  1.0f}, XMFLOAT2{ 0.0f, 0.0f }, XMFLOAT3{-0.5773f,  0.5773f,  0.5773f}},  // Back TL
		{XMFLOAT3{ 0.5f,  0.5f,  0.5f}, XMFLOAT4{1.0f,  1.0f,  0.0f,  1.0f}, XMFLOAT2{ 1.0f, 0.0f }, XMFLOAT3{ 0.5773f,  0.5773f,  0.5773f}},  // Back TR
		{XMFLOAT3{ 0.5f, -0.5f,  0.5f}, XMFLOAT4{0.0f,  0.0f,  0.0f,  1.0f}, XMFLOAT2{ 1.0f, 1.0f }, XMFLOAT3{ 0.5773f, -0.5773f,  0.5773f}},  // Back BR
	};

	const unsigned int indices[] =
	{
		0,1,2,2,3,0, // Front
		7,6,5,5,4,7, // Back
		4,5,1,1,0,4, // Left
		3,2,6,6,7,3, // Right
		1,5,6,6,2,1, // Top
		4,0,3,3,7,4  // Bottom
	};

	D3D11_BUFFER_DESC ibd = {0};
	ibd.Usage			= D3D11_USAGE_DEFAULT;
	ibd.ByteWidth		= sizeof(indices);
	ibd.BindFlags		= D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA initData = {0};
	initData.pSysMem = indices;

	if(FAILED(g_device->CreateBuffer(&ibd, &initData, &pIBuffer)))
	{
		OutputDebugString(L"Failed to create index buffer");
		std::cout << "Failed to create index buffer\n";
		return;
	}
	OutputDebugString(L"Successfully created index buffer\n");


	D3D11_BUFFER_DESC bd = {0};
	bd.Usage			 = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth		 = sizeof(vertices);
	bd.BindFlags		 = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags	 = D3D11_CPU_ACCESS_WRITE;

	D3D11_BUFFER_DESC cb = {0};
	cb.Usage = D3D11_USAGE_DEFAULT;
	cb.ByteWidth = sizeof(CBUFFER0);
	cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	if (FAILED(g_device->CreateBuffer(&cb, NULL, &pCBuffer)))
	{
		OutputDebugString(L"Failed to create constant buffer");
		std::cout << "Failed to create constant buffer\n";
		return;
	}
	OutputDebugString(L"Successfully created constant buffer\n");

	g_device->CreateBuffer(&bd, NULL, &pVBuffer);

	if (pVBuffer == 0)
		return;

	cb.ByteWidth = sizeof(SkyboxCBuffer);
	if (FAILED(g_device->CreateBuffer(&cb, NULL, &pSkyboxCBuffer)))
	{
		OutputDebugString(L"Failed to create skybox constant buffer");
		return;
	}

	D3D11_MAPPED_SUBRESOURCE ms;
	g_context->Map(pVBuffer, NULL, D3D11_MAP_WRITE_DISCARD, NULL, &ms);
	memcpy(ms.pData, vertices, sizeof(vertices));
	g_context->Unmap(pVBuffer, NULL);

	// Texture
	CreateWICTextureFromFile(g_device, g_context, L"Box.bmp", NULL, &pTexture);
	CreateDDSTextureFromFile(g_device, L"skybox01.dds", NULL, &pSkyboxTexture);

	// Sampler
	D3D11_SAMPLER_DESC sampDesc;
	ZeroMemory(&sampDesc, sizeof(sampDesc));
	sampDesc.Filter			= D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU		= D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV		= D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW		= D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.MaxLOD			= D3D11_FLOAT32_MAX;
	g_device->CreateSamplerState(&sampDesc, &pSampler);

}

void HandleInput()
{
	auto kbState = Keyboard::Get().GetState();
	kbTracker.Update(kbState);

	auto mouseState = Mouse::Get().GetState();
	mouseTracker.Update(mouseState);

	float sense = XM_2PI * 0.00025f;
	g_camera.yaw += mouseState.x * sense;
	g_camera.pitch += mouseState.y * sense;


	// Get camera forward vector
	const XMVECTOR lookTo = XMVectorSet(sin(g_camera.yaw) * sin(g_camera.pitch),
								  cos(g_camera.pitch),
								  cos(g_camera.yaw) * sin(g_camera.pitch),
								  0);

	// Get camera right vector
	const XMVECTOR camRight = XMVector3Cross(lookTo, XMVectorSet(0, 1, 0, 0));

	if (kbState.Escape)
		PostQuitMessage(0);

	if (kbState.W)
	{
		g_camera.x += XMVectorGetX(lookTo) * 0.0001f;
		g_camera.y += XMVectorGetY(lookTo) * 0.0001f;
		g_camera.z += XMVectorGetZ(lookTo) * 0.0001f;
	}

	if (kbState.S)
	{
		g_camera.x -= XMVectorGetX(lookTo) * 0.0001f;
		g_camera.y -= XMVectorGetY(lookTo) * 0.0001f;
		g_camera.z -= XMVectorGetZ(lookTo) * 0.0001f;
	}

	if (kbState.D)
	{
		g_camera.x -= XMVectorGetX(camRight) * 0.0001f;
		g_camera.z -= XMVectorGetZ(camRight) * 0.0001f;
	}

	if (kbState.A)
	{
		g_camera.x += XMVectorGetX(camRight) * 0.0001f;
		g_camera.z += XMVectorGetZ(camRight) * 0.0001f;
	}

	if (kbState.Space)
	{
		g_camera.y += 0.0001f;
	}

	if (kbState.LeftControl)
	{
		g_camera.y -= 0.0001f;
	}

}

void InitScene()
{
	pointLights[0] = { XMVECTOR{ 1.5f, 0, -1 }, { 0.9f, 0, 0.85f, 1.0f }, 10.0f, true };
	pointLights[1] = { XMVECTOR{ -1.5f, 0, -1 }, { 0.0f, 0.9f, 0.85f, 1.0f }, 20.0f, true };

	model = new ObjFileModel((char*)"cube.obj", g_device, g_context);
}

HRESULT LoadVertexShader(LPCWSTR fileName, LPCSTR entrypoint, ID3D11VertexShader** vs, ID3D11InputLayout** il)
{
	HRESULT result;
	ID3DBlob* VSblob, * errorBlob;

	result = D3DCompileFromFile(fileName, NULL, NULL, entrypoint, "vs_4_0", NULL, NULL, &VSblob, &errorBlob);
	if (FAILED(result))
	{
		OutputDebugStringA(reinterpret_cast<const char*>(errorBlob->GetBufferPointer()));
		errorBlob->Release();
		return result;
	}

	result = g_device->CreateVertexShader(VSblob->GetBufferPointer(), VSblob->GetBufferSize(), NULL, vs);
	if (FAILED(result))
	{
		OutputDebugString(L"Failed to create vertex shader");
		return result;
	}

	D3D11_INPUT_ELEMENT_DESC ied[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",	  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",	  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	result = g_device->CreateInputLayout(ied, ARRAYSIZE(ied), VSblob->GetBufferPointer(), VSblob->GetBufferSize(), il);
	VSblob->Release();
	if (FAILED(result))
	{
		OutputDebugString(L"Failed to create input layout");
		return result;
	}

	return S_OK;
}

HRESULT LoadPixelShader(LPCWSTR fileName, LPCSTR entrypoint, ID3D11PixelShader** ps)
{
	HRESULT result;
	ID3DBlob* PSblob, * errorBlob;

	result = D3DCompileFromFile(fileName, NULL, NULL, entrypoint, "ps_4_0", NULL, NULL, &PSblob, &errorBlob);
	if (FAILED(result))
	{
		OutputDebugStringA(reinterpret_cast<const char*>(errorBlob->GetBufferPointer()));
		errorBlob->Release();
		return result;
	}

	result = g_device->CreatePixelShader(PSblob->GetBufferPointer(), PSblob->GetBufferSize(), NULL, ps);
	PSblob->Release();
	if (FAILED(result))
	{
		OutputDebugString(L"Failed to create pixel shader");
		return result;
	}

	return S_OK;
}

HRESULT LoadCompiledVertexShader(LPCWSTR fileName, ID3D11VertexShader** vs, ID3D11InputLayout** il)
{
	HRESULT result;

	auto VertexBytecode = DX::ReadData(fileName);

	g_device->CreateVertexShader(VertexBytecode.data(), VertexBytecode.size(), NULL, vs);

	g_context->VSSetShader(*vs, 0, 0);


#pragma region Shader reflection
	ID3D11ShaderReflection* vShaderReflection = nullptr;
	D3DReflect(VertexBytecode.data(), VertexBytecode.size(), IID_ID3D11ShaderReflection, (void**)&vShaderReflection);

	D3D11_SHADER_DESC shaderDesc;
	vShaderReflection->GetDesc(&shaderDesc);

	auto paramDesc = new D3D11_SIGNATURE_PARAMETER_DESC[shaderDesc.InputParameters] {0};
	for(UINT i = 0; i < shaderDesc.InputParameters; i++)
	{
		vShaderReflection->GetInputParameterDesc(i, &paramDesc[i]);
	}

	auto ied = new D3D11_INPUT_ELEMENT_DESC[shaderDesc.InputParameters] {0};
	for(size_t i = 0; i < shaderDesc.InputParameters; i++)
	{
		ied[i].SemanticName			= paramDesc[i].SemanticName;
		ied[i].SemanticIndex		= paramDesc[i].SemanticIndex;

		if (paramDesc[i].ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
		{
			switch (paramDesc[i].Mask)
			{
			case 1:
				ied[i].Format = DXGI_FORMAT_R32_FLOAT;
				break;

			case 3:
				ied[i].Format = DXGI_FORMAT_R32G32_FLOAT;
				break;

			case 7:
				ied[i].Format = DXGI_FORMAT_R32G32B32_FLOAT;
				break;

			case 15:
				ied[i].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
				break;

			default:
				break;
			}
		}

		ied[i].InputSlot			= 0;
		ied[i].AlignedByteOffset	= D3D11_APPEND_ALIGNED_ELEMENT;
		ied[i].InputSlotClass		= D3D11_INPUT_PER_VERTEX_DATA;
		ied[i].InstanceDataStepRate = 0;

	}

	
	result = g_device->CreateInputLayout(ied, shaderDesc.InputParameters, VertexBytecode.data(), VertexBytecode.size(), &pLayout);
	if (FAILED(result))
	{
		OutputDebugString(L"Failed to create input layout");
		std::cout << "Failed to create input layout\n";
		return result;
	}
	OutputDebugString(L"Successfully created input layout\n");

	g_context->IASetInputLayout(pLayout);

	delete[] paramDesc;
	delete[] ied;

#pragma endregion

	return S_OK;
}

HRESULT LoadCompiledPixelShader(LPCWSTR fileName, ID3D11PixelShader** ps)
{
	HRESULT result;

	auto PixelBytecode = DX::ReadData(fileName);

	result = g_device->CreatePixelShader(PixelBytecode.data(), PixelBytecode.size(), NULL, ps);
	if (FAILED(result))
	{
		OutputDebugString(L"Failed to create pixel shader");
		return result;
	}

	g_context->PSSetShader(*ps, 0, 0);

	return S_OK;
}

void DrawSkybox()
{
	// Front-face culling and disable depth writing
	g_context->OMSetDepthStencilState(pDepthWriteSkybox, 1);
	g_context->RSSetState(pRasterSkybox);

	// Set skybox shaders
	g_context->VSSetShader(pSkyboxVS, 0, 0);
	g_context->PSSetShader(pSkyboxPS, 0, 0);
	g_context->IASetInputLayout(pSkyboxLayout);

	// Constant buffer data
	SkyboxCBuffer cBuffer;
	XMMATRIX translation, projection, view;
	translation = XMMatrixTranslation(g_camera.x, g_camera.y, g_camera.z);
	projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(60), (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT, 0.1f, 100.0f);
	view = g_camera.GetViewMatrix();
	cBuffer.WVP = translation * view * projection;
	g_context->UpdateSubresource(pSkyboxCBuffer, 0, 0, &cBuffer, 0, 0);

	// Set shader resources
	g_context->VSSetConstantBuffers(0, 1, &pSkyboxCBuffer);
	g_context->PSSetSamplers(0, 1, &pSampler);
	g_context->PSSetShaderResources(0, 1, &pSkyboxTexture);

	model->Draw();

	// Reset depth writing and culling
	g_context->OMSetDepthStencilState(pDepthWriteSolid, 1);
	g_context->RSSetState(pRasterSolid);

	// Reset shaders
	g_context->VSSetShader(pVS, 0, 0);
	g_context->PSSetShader(pPS, 0, 0);
	g_context->IASetInputLayout(pLayout);
}