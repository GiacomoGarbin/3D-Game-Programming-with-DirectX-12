#include "ApplicationFramework.h"

#include <WindowsX.h>


LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// forward hwnd on because we can get messages (e.g., WM_CREATE)
	// before CreateWindow returns, and thus before mMainWindow is valid
	return ApplicationFramework::GetApplicationFramework()->MsgProc(hwnd, msg, wParam, lParam);
}

ApplicationFramework::ApplicationFramework(HINSTANCE instance)
	: mApplicationInstance(instance)
{
	assert(mApplicationFramework == nullptr);
	mApplicationFramework = this;
}

ApplicationFramework::~ApplicationFramework()
{
	if (mDevice)
	{
		FlushCommandQueue();
	}
}

ApplicationFramework* ApplicationFramework::mApplicationFramework = nullptr;

ApplicationFramework* ApplicationFramework::GetApplicationFramework()
{
	return mApplicationFramework;
}

bool ApplicationFramework::init()
{
	if (!InitMainWindow())
	{
		return false;
	}

	if (!InitDirect3D())
	{
		return false;
	}

	OnResize();

	return true;
}

bool ApplicationFramework::InitMainWindow()
{
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = mApplicationInstance;
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.lpszMenuName = 0;
	wc.lpszClassName = L"MainWnd";

	if (!RegisterClass(&wc))
	{
		MessageBox(0, L"RegisterClass Failed.", 0, 0);
		return false;
	}

	RECT R = { 0, 0, mMainWindowWidth, mMainWindowHeight };
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	int width = R.right - R.left;
	int height = R.bottom - R.top;

	mMainWindow = CreateWindow(L"MainWnd", mMainWindowTitle.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, mApplicationInstance, 0);
	if (!mMainWindow)
	{
		MessageBox(0, L"CreateWindow Failed.", 0, 0);
		return false;
	}

	ShowWindow(mMainWindow, SW_SHOW);
	UpdateWindow(mMainWindow);

	return true;
}

bool ApplicationFramework::InitDirect3D()
{
	// enable D3D12 debug layer
	{
		Microsoft::WRL::ComPtr<ID3D12Debug> controller;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&controller)));
		controller->EnableDebugLayer();
	}

	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mFactory)));
	ThrowIfFailed(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&mDevice)));
	ThrowIfFailed(mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));

	mRTVDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	mDSVDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	mCBVSRVUAVDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// check 4x MSAA support
	{

	}

	CreateCommandObjects();
	CreateSwapChain();
	CreateRTVAndDSVDescriptorHeaps();

	return true;
}

void ApplicationFramework::CreateCommandObjects()
{
	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

	ThrowIfFailed(mDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&mCommandQueue)));
	ThrowIfFailed(mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(mCommandAllocator.GetAddressOf())));
	ThrowIfFailed(mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCommandAllocator.Get(), nullptr, IID_PPV_ARGS(mCommandList.GetAddressOf())));

	mCommandList->Close();
}

void ApplicationFramework::CreateSwapChain()
{
	mSwapChain.Reset();

	DXGI_SWAP_CHAIN_DESC desc;
	desc.BufferDesc.Width = mMainWindowWidth;
	desc.BufferDesc.Height = mMainWindowHeight;
	desc.BufferDesc.RefreshRate.Numerator = 60;
	desc.BufferDesc.RefreshRate.Denominator = 1;
	desc.BufferDesc.Format = mBackBufferFormat;
	desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	desc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	desc.SampleDesc.Count = m4xMSAAState ? 4 : 1;
	desc.SampleDesc.Quality = m4xMSAAState ? (m4xMSAAQuality - 1) : 0;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.BufferCount = SwapChainBufferSize;
	desc.OutputWindow = mMainWindow;
	desc.Windowed = true;
	desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	ThrowIfFailed(mFactory->CreateSwapChain(mCommandQueue.Get(), &desc, mSwapChain.GetAddressOf()));
}

void ApplicationFramework::CreateRTVAndDSVDescriptorHeaps()
{
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc;
		desc.NumDescriptors = SwapChainBufferSize;
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		desc.NodeMask = 0;

		ThrowIfFailed(mDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(mRTVHeap.GetAddressOf())));
	}

	{
		D3D12_DESCRIPTOR_HEAP_DESC desc;
		desc.NumDescriptors = 1;
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		desc.NodeMask = 0;

		ThrowIfFailed(mDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(mDSVHeap.GetAddressOf())));
	}

	{
		D3D12_DESCRIPTOR_HEAP_DESC desc;
		desc.NumDescriptors = 1;
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		desc.NodeMask = 0;

		ThrowIfFailed(mDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(mSRVHeap.GetAddressOf())));
	}
}

void ApplicationFramework::OnResize()
{
	FlushCommandQueue();

	ImGui_ImplDX12_InvalidateDeviceObjects();

	ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));

	for (auto& buffer : mSwapChainBuffer)
	{
		buffer.Reset();
	}

	mDepthStencilBuffer.Reset();

	ThrowIfFailed(mSwapChain->ResizeBuffers(SwapChainBufferSize, mMainWindowWidth, mMainWindowHeight, mBackBufferFormat, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

	mCurrentBackBufferIndex = 0;

	CD3DX12_CPU_DESCRIPTOR_HANDLE handle(mRTVHeap->GetCPUDescriptorHandleForHeapStart());

	for (UINT i = 0; i < SwapChainBufferSize; ++i)
	{
		ThrowIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mSwapChainBuffer[i])));
		mDevice->CreateRenderTargetView(mSwapChainBuffer[i].Get(), nullptr, handle);
		handle.Offset(1, mRTVDescriptorSize);
	}

	D3D12_RESOURCE_DESC desc;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Alignment = 0;
	desc.Width = mMainWindowWidth;
	desc.Height = mMainWindowHeight;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	desc.SampleDesc.Count = m4xMSAAState ? 4 : 1;
	desc.SampleDesc.Quality = m4xMSAAState ? (m4xMSAAQuality - 1) : 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE clear;
	clear.Format = mDepthStencilFormat;
	clear.DepthStencil.Depth = 1;
	clear.DepthStencil.Stencil = 0;

	auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(mDevice->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, &clear, IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf())));

	// depth stencil view
	{
		D3D12_DEPTH_STENCIL_VIEW_DESC desc;
		desc.Flags = D3D12_DSV_FLAG_NONE;
		desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		desc.Format = mDepthStencilFormat;
		desc.Texture2D.MipSlice = 0;

		mDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), &desc, GetDepthStencilView());

		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		mCommandList->ResourceBarrier(1, &barrier);
	}

	ThrowIfFailed(mCommandList->Close());

	ID3D12CommandList* list[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(list), list);

	FlushCommandQueue();

	mScreenViewport.TopLeftX = 0;
	mScreenViewport.TopLeftY = 0;
	mScreenViewport.Width = mMainWindowWidth;
	mScreenViewport.Height = mMainWindowHeight;
	mScreenViewport.MinDepth = 0;
	mScreenViewport.MaxDepth = 1;

	mScissorRect.left = 0;
	mScissorRect.top = 0;
	mScissorRect.right = mMainWindowWidth;
	mScissorRect.bottom = mMainWindowHeight;

	ImGui_ImplDX12_CreateDeviceObjects();
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT ApplicationFramework::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(mMainWindow, msg, wParam, lParam))
	{
		return true;
	}

	switch (msg)
	{
		// WM_ACTIVATE is sent when the window is activated or deactivated  
		// we pause the game when the window is deactivated and unpause it when it becomes active
		case WM_ACTIVATE:
		{
			if (LOWORD(wParam) == WA_INACTIVE)
			{
				mApplicationPaused = true;
				mGameTimer.stop();
			}
			else
			{
				mApplicationPaused = false;
				mGameTimer.start();
			}
			return 0;
		}
		// WM_SIZE is sent when the user resizes the window  
		case WM_SIZE:
		{
			// new client area dimensions
			mMainWindowWidth = LOWORD(lParam);
			mMainWindowHeight = HIWORD(lParam);

			if (mDevice)
			{
				switch (wParam)
				{

					case SIZE_MINIMIZED:
					{
						mApplicationPaused = true;
						mWindowMinimized = true;
						mWindowMaximized = false;
					}
					case SIZE_MAXIMIZED:
					{
						mApplicationPaused = false;
						mWindowMinimized = false;
						mWindowMaximized = true;
						OnResize();
					}
					case SIZE_RESTORED:
					{
						// restoring from minimized state?
						if (mWindowMinimized)
						{
							mApplicationPaused = false;
							mWindowMinimized = false;
							OnResize();
						}
						// restoring from maximized state?
						else if (mWindowMaximized)
						{
							mApplicationPaused = false;
							mWindowMaximized = false;
							OnResize();
						}
						else if (mWindowResizing)
						{
							// If user is dragging the resize bars, we do not resize 
							// the buffers here because as the user continuously 
							// drags the resize bars, a stream of WM_SIZE messages are
							// sent to the window, and it would be pointless (and slow)
							// to resize for each WM_SIZE message received from dragging
							// the resize bars. So instead, we reset after the user is 
							// done resizing the window and releases the resize bars, which 
							// sends a WM_EXITSIZEMOVE message.
						}
						else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
						{
							OnResize();
						}
					}
				}
			}
			return 0;
		}
		// WM_EXITSIZEMOVE is sent when the user grabs the resize bars
		case WM_ENTERSIZEMOVE:
		{
			mApplicationPaused = true;
			mWindowResizing = true;
			mGameTimer.stop();
			return 0;
		}
		// WM_EXITSIZEMOVE is sent when the user releases the resize bars
		case WM_EXITSIZEMOVE:
		{
			mApplicationPaused = false;
			mWindowResizing = false;
			mGameTimer.start();
			OnResize(); // we reset everything based on the new window dimensions
			return 0;
		}
		// WM_DESTROY is sent when the window is being destroyed
		case WM_DESTROY:
		{
			PostQuitMessage(0);
			return 0;
		}
		// WM_MENUCHAR is sent when a menu is active and the user presses a key that does not correspond to any mnemonic or accelerator key
		case WM_MENUCHAR:
		{
			// don't beep when we alt-enter
			return MAKELRESULT(0, MNC_CLOSE);
		}
		// catch this message so to prevent the window from becoming too small
		case WM_GETMINMAXINFO:
		{
			((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
			((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
			return 0;
		}
		case WM_LBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_RBUTTONDOWN:
		{
			OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			return 0;
		}
		case WM_LBUTTONUP:
		case WM_MBUTTONUP:
		case WM_RBUTTONUP:
		{
			OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			return 0;
		}
		case WM_MOUSEMOVE:
		{
			OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			return 0;
		}
		case WM_KEYUP:
		{
			if (wParam == VK_ESCAPE)
			{
				PostQuitMessage(0);
			}
			else if ((int)wParam == VK_F2)
			{
				m4xMSAAState = !m4xMSAAState;
			}
			return 0;
		}
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void ApplicationFramework::OnMouseDown(WPARAM state, int x, int y) {}
void ApplicationFramework::OnMouseUp(WPARAM state, int x, int y) {}
void ApplicationFramework::OnMouseMove(WPARAM state, int x, int y) {}

int ApplicationFramework::run()
{
	MSG msg = {0};

	mGameTimer.reset();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();

	ImGui_ImplWin32_Init(mMainWindow);
	ImGui_ImplDX12_Init(mDevice.Get(),
						SwapChainBufferSize,
						mBackBufferFormat,
						mSRVHeap.Get(),
						mSRVHeap->GetCPUDescriptorHandleForHeapStart(),
						mSRVHeap->GetGPUDescriptorHandleForHeapStart());

	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			mGameTimer.tick();

			if (!mApplicationPaused)
			{
				// frame stats
				update(mGameTimer);
				draw(mGameTimer);
			}
			else
			{
				Sleep(100);
			}
		}
	}

	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	return msg.wParam;
}

ID3D12Resource* ApplicationFramework::GetCurrentBackBuffer() const
{
	return mSwapChainBuffer[mCurrentBackBufferIndex].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE ApplicationFramework::GetCurrentBackBufferView() const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(mRTVHeap->GetCPUDescriptorHandleForHeapStart(), mCurrentBackBufferIndex, mRTVDescriptorSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE ApplicationFramework::GetDepthStencilView() const
{
	return mDSVHeap->GetCPUDescriptorHandleForHeapStart();
}

void ApplicationFramework::FlushCommandQueue()
{
	mCurrentFence++;

	ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence));

	if (mFence->GetCompletedValue() < mCurrentFence)
	{
		HANDLE handle = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);

		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFence, handle));

		WaitForSingleObject(handle, INFINITE);

		CloseHandle(handle);
	}
}
