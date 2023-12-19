#include <Windows.h>
#include <d3d11.h>
#include <iostream>

#include "ReadData.h"
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <d3d11shader.h>
#include <d3dcompiler.h>

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

ID3D11VertexShader*     pVS          = NULL;
ID3D11PixelShader*      pPS          = NULL;
ID3D11InputLayout*      pLayout      = NULL;

ID3D11Buffer* pVBuffer = NULL;
ID3D11Buffer* pCBuffer = NULL;

struct Vertex
{
	XMFLOAT3 Position;
	XMFLOAT4 Color;
};

struct CBUFFER0
{
	XMFLOAT3 pos;
	float padding;
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

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE prevInstanceHandle, _In_ LPSTR cmdLine, _In_ int nCmdShow)
{
	if(FAILED(InitWindow(hInstance, nCmdShow)))
		MessageBox(NULL, L"Failed to create window", L"Error", MB_OK | MB_ICONERROR);

	if(FAILED(InitD3D(g_hWnd)))
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

			if(msg.message == WM_QUIT)
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
		if (wParam == VK_ESCAPE)
			DestroyWindow(hWnd);
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
	g_context->OMSetRenderTargets(1, &g_backBuffer, NULL);

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
	if (g_device)	g_device->Release();
	if (pVBuffer)	pVBuffer->Release();
	if (pCBuffer)	pCBuffer->Release();
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
	CBUFFER0 cBuffer;
	cBuffer.pos = XMFLOAT3(0.5f, 0.0f, 0.0f);

	g_context->UpdateSubresource(pCBuffer, 0, 0, &cBuffer, 0, 0);

	g_context->VSSetConstantBuffers(0, 1, &pCBuffer);

	g_context->ClearRenderTargetView(g_backBuffer, Colors::DarkSlateBlue);

	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	g_context->IASetVertexBuffers(0, 1, &pVBuffer, &stride, &offset);

	g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	g_context->Draw(3, 0);

	g_swapChain->Present(0, 0);
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
		return result;
	}

	g_context->IASetInputLayout(pLayout);

	delete[] paramDesc;
	delete[] ied;

	return S_OK;
}

void InitGraphics()
{
	Vertex vertices[] = 
	{
		{ XMFLOAT3(-0.5f, -0.5f, 0.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f)},
		{ XMFLOAT3( 0.0f,  0.5f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f)},
		{ XMFLOAT3( 0.5f, -0.5f, 0.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f)}
	};

	D3D11_BUFFER_DESC bd = {0};
	bd.Usage			 = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth		 = sizeof(Vertex) * 3;
	bd.BindFlags		 = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags	 = D3D11_CPU_ACCESS_WRITE;

	D3D11_BUFFER_DESC cb = {0};
	cb.Usage = D3D11_USAGE_DEFAULT;
	cb.ByteWidth = sizeof(CBUFFER0);
	cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	if (FAILED(g_device->CreateBuffer(&cb, NULL, &pCBuffer)))
		OutputDebugString(L"Failed to create constant buffer");

	g_device->CreateBuffer(&bd, NULL, &pVBuffer);

	if (pVBuffer == 0)
		return;

	D3D11_MAPPED_SUBRESOURCE ms;
	g_context->Map(pVBuffer, NULL, D3D11_MAP_WRITE_DISCARD, NULL, &ms);
	memcpy(ms.pData, vertices, sizeof(vertices));
	g_context->Unmap(pVBuffer, NULL);

}