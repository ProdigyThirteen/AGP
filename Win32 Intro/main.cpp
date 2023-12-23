#include <Windows.h>
#include <d3d11.h>
#include <iostream>

#include "ReadData.h"
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <d3d11shader.h>
#include <d3dcompiler.h>
#include "WICTextureLoader.h"

using namespace DirectX;

// Global variables
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

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
};

struct CBUFFER0
{
	XMMATRIX WVP;
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


// Forward declarations
HRESULT InitWindow(HINSTANCE instanceHandle, int nCmdShow);
LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
HRESULT InitD3D(HWND hWnd);
HRESULT InitPipeline();
void    CleanD3D();
void	OpenConsole();
void	RenderFrame();
void	InitGraphics();

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

	MSG msg;

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

	case WM_KEYDOWN:
		switch (wParam)
		{
			case VK_ESCAPE:
				PostQuitMessage(0);
				break;

			case 'W':
				g_camera.z += 0.1f;
				break;

			case 'S':
				g_camera.z -= 0.1f;
				break;

			case 'A':
				g_camera.x -= 0.1f;
				break;

			case 'D':
				g_camera.x += 0.1f;
				break;

			case VK_SPACE:
				g_camera.y += 0.1f;
				break;

			case VK_SHIFT:
				g_camera.y -= 0.1f;
				break;

			case VK_DOWN:
				g_camera.pitch += XM_PI / 8;
				break;

			case VK_UP:
				g_camera.pitch -= XM_PI / 8;
				break;

			case VK_LEFT:
				g_camera.yaw -= XM_PI / 8;
				break;

			case VK_RIGHT:
				g_camera.yaw += XM_PI / 8;
				break;

			default:
				break;
		}
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

	return S_OK;
}

void CleanD3D()
{
	if (g_backBuffer) g_backBuffer->Release();
	if (g_swapChain) g_swapChain->Release();
	if (g_context)	g_context->Release();
	if (g_ZBuffer)	g_ZBuffer->Release();
	if (g_device)	g_device->Release();
	if (pVBuffer)	pVBuffer->Release();
	if (pIBuffer)	pIBuffer->Release();
	if (pCBuffer)	pCBuffer->Release();
	if (pTexture)	pTexture->Release();
	if (pSampler)	pSampler->Release();
	if (pLayout)	pLayout->Release();
	if (pVS)		pVS->Release();
	if (pPS)		pPS->Release();
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


	// Update constant buffer
	g_context->UpdateSubresource(pCBuffer, 0, 0, &cBuffer, 0, 0);
	g_context->VSSetConstantBuffers(0, 1, &pCBuffer);

	g_context->DrawIndexed(36, 0, 0);


	world = cube2.GetWorldMaxtrix();
	cBuffer.WVP = world * view * projection;
	g_context->UpdateSubresource(pCBuffer, 0, 0, &cBuffer, 0, 0);
	g_context->VSSetConstantBuffers(0, 1, &pCBuffer);
	g_context->DrawIndexed(36, 0, 0);

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

	return S_OK;
}

void InitGraphics()
{
	Vertex vertices[] =
	{
		{XMFLOAT3{-0.5f, -0.5f, -0.5f}, XMFLOAT4{1.0f,  0.0f,  0.0f,  1.0f}, XMFLOAT2{ 0.0f, 1.0f }},  // Front BL
		{XMFLOAT3{-0.5f,  0.5f, -0.5f}, XMFLOAT4{0.0f,  1.0f,  0.0f,  1.0f}, XMFLOAT2{ 0.0f, 0.0f }},  // Front TL
		{XMFLOAT3{ 0.5f,  0.5f, -0.5f}, XMFLOAT4{0.0f,  0.0f,  1.0f,  1.0f}, XMFLOAT2{ 1.0f, 0.0f }},  // Front TR
		{XMFLOAT3{ 0.5f, -0.5f, -0.5f}, XMFLOAT4{1.0f,  1.0f,  1.0f,  1.0f}, XMFLOAT2{ 1.0f, 1.0f }},  // Front BR

		{XMFLOAT3{-0.5f, -0.5f,  0.5f}, XMFLOAT4{0.0f,  1.0f,  1.0f,  1.0f}, XMFLOAT2{ 0.0f, 1.0f }},  // Back BL
		{XMFLOAT3{-0.5f,  0.5f,  0.5f}, XMFLOAT4{1.0f,  0.0f,  1.0f,  1.0f}, XMFLOAT2{ 0.0f, 0.0f }},  // Back TL
		{XMFLOAT3{ 0.5f,  0.5f,  0.5f}, XMFLOAT4{1.0f,  1.0f,  0.0f,  1.0f}, XMFLOAT2{ 1.0f, 0.0f }},  // Back TR
		{XMFLOAT3{ 0.5f, -0.5f,  0.5f}, XMFLOAT4{0.0f,  0.0f,  0.0f,  1.0f}, XMFLOAT2{ 1.0f, 1.0f }},  // Back BR
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

	D3D11_MAPPED_SUBRESOURCE ms;
	g_context->Map(pVBuffer, NULL, D3D11_MAP_WRITE_DISCARD, NULL, &ms);
	memcpy(ms.pData, vertices, sizeof(vertices));
	g_context->Unmap(pVBuffer, NULL);

	// Texture
	CreateWICTextureFromFile(g_device, g_context, L"Box.bmp", NULL, &pTexture);

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