#include "pch.h"

#define TIMER_ID 101
#define TIMER_INTERVAL 300000 // Set Img Display Duration (1000 == 1s)

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "Shlwapi.lib")

HWND g_hWnd;
HINSTANCE g_hInstance;
ID2D1Factory7* g_d2dFactory;
ID2D1Device6* g_d2dDevice;
ID2D1DeviceContext5* g_d2dContext;
ID2D1Bitmap1* g_targetBitmap;
IDXGISwapChain1* g_swapChain;
ID2D1SvgDocument* g_svgDocument;

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
HRESULT InitD2D(HWND hwnd);
HRESULT SVGAttibuteValue(HWND hwnd);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    g_hInstance = hInstance;

    WNDCLASSEX wcex;
    ZeroMemory(&wcex, sizeof(WNDCLASSEX));
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = L"MainWindow";

    RegisterClassEx(&wcex);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    DWORD const dwStyleEx = WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TRANSPARENT /*| WS_EX_NOACTIVATE*/;
    DWORD const dwStyle = WS_POPUP;

    g_hWnd = CreateWindowEx(dwStyleEx,
        L"MainWindow", L"SVG OSD", dwStyle,
        0, 0, screenWidth, screenHeight, nullptr, nullptr, hInstance, nullptr);

    if (!g_hWnd)
        return -1;

    if (FAILED(InitD2D(g_hWnd)))
    {
        LRESULT res = SendMessage(g_hWnd, WM_DESTROY, NULL, NULL);
    }

    bool res = SetLayeredWindowAttributes(g_hWnd, RGB(0, 0, 0), 0, LWA_COLORKEY);

    if (FAILED(SVGAttibuteValue(g_hWnd)))
    {
        OutputDebugString(L"Set SVG attibute failed");
    }

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    SetTimer(g_hWnd, TIMER_ID, TIMER_INTERVAL, nullptr);

    MSG msg{};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        if (g_d2dContext)
        {
            g_d2dContext->BeginDraw();

            g_d2dContext->Clear();

            if (g_svgDocument)
            {
                g_d2dContext->DrawSvgDocument(g_svgDocument);
            }

            g_d2dContext->EndDraw();
            g_swapChain->Present(1, 0);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        SafeRelease(&g_d2dFactory);
        SafeRelease(&g_d2dDevice);
        SafeRelease(&g_d2dContext);
        SafeRelease(&g_targetBitmap);
        SafeRelease(&g_swapChain);
        SafeRelease(&g_svgDocument);
        PostQuitMessage(0);
        return 0;

    case WM_TIMER:
        if (wParam == TIMER_ID)
        {
            KillTimer(hwnd, TIMER_ID);
            DestroyWindow(hwnd);
        }
        return 0;
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

HRESULT InitD2D(HWND hwnd)
{
    HRESULT hr = S_OK;
    ID3D11Device* d3dDevice{};
    ID3D11DeviceContext* d3dContext{};
    D3D_FEATURE_LEVEL featureLevel{};
    IDXGIDevice* dxgiDevice{};
    IDXGIAdapter* pAdapter{};
    IDXGIFactory7* dxgiFactory{};
    IDXGISurface* dxgiBackBuffer{};
    IStream* svgStream{};
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD; // Loss transparent with FLIP param
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE;

    D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));

    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
        D3D11_SDK_VERSION, &d3dDevice, &featureLevel, &d3dContext);
    if (FAILED(hr))
        goto Exit;

    hr = d3dDevice->QueryInterface(&dxgiDevice);
    if (FAILED(hr))
        goto Exit;

    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_d2dFactory);
    if (FAILED(hr))
        goto Exit;

    hr = g_d2dFactory->CreateDevice(dxgiDevice, &g_d2dDevice);
    if (FAILED(hr))
        goto Exit;

    hr = g_d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &g_d2dContext);
    if (FAILED(hr))
        goto Exit;

    hr = dxgiDevice->GetAdapter(&pAdapter);
    if (FAILED(hr))
        goto Exit;

    hr = pAdapter->GetParent(__uuidof(IDXGIFactory7), reinterpret_cast<void**>(&dxgiFactory));
    if (FAILED(hr))
        goto Exit;

    hr = dxgiFactory->CreateSwapChainForHwnd(d3dDevice, hwnd, &swapChainDesc, nullptr, nullptr, &g_swapChain);
    if (FAILED(hr))
        goto Exit;

    hr = g_swapChain->GetBuffer(0, IID_PPV_ARGS(&dxgiBackBuffer));
    if (FAILED(hr))
        goto Exit;

    hr = g_d2dContext->CreateBitmapFromDxgiSurface(dxgiBackBuffer, &bitmapProperties, &g_targetBitmap);
    if (FAILED(hr))
        goto Exit;

    g_d2dContext->SetTarget(g_targetBitmap);

    hr = SHCreateStreamOnFileEx(L"cat-svgrepo-com.svg", STGM_READ | STGM_SHARE_DENY_WRITE, 0, FALSE, nullptr, &svgStream);
    if (FAILED(hr))
        goto Exit;

    hr = g_d2dContext->CreateSvgDocument(svgStream, D2D1::SizeF(screenWidth, screenHeight), &g_svgDocument);
    if (FAILED(hr))
        goto Exit;

Exit:
    SafeRelease(&d3dDevice);
    SafeRelease(&d3dContext);
    SafeRelease(&dxgiDevice);
    SafeRelease(&pAdapter);
    SafeRelease(&dxgiFactory);
    SafeRelease(&dxgiBackBuffer);
    SafeRelease(&svgStream);
    return hr;
}

HRESULT SVGAttibuteValue(HWND hwnd)
{
    HRESULT hr = S_OK;
    ID2D1SvgElement* svgElement{};

    float width{};
    float height{};

    float centerX = GetSystemMetrics(SM_CXSCREEN) / 2.f;
    float centerY = GetSystemMetrics(SM_CYSCREEN) / 2.f;

    g_svgDocument->GetRoot(&svgElement);

    if (svgElement)
    {
        hr = svgElement->SetAttributeValue(L"width", 600.0f);
        hr = svgElement->SetAttributeValue(L"height", 600.0f);

        hr = svgElement->GetAttributeValue(L"width", &width);
        hr = svgElement->GetAttributeValue(L"height", &height);

        if (FAILED(hr))
            goto Exit;

        float offsetX = width / 2;
        float offsetY = height / 2;

        D2D1_POINT_2F translation = D2D1::Point2F(centerX - offsetX, centerY - offsetY);

        g_d2dContext->SetTransform(
            D2D1::Matrix3x2F::Translation(translation.x, translation.y)
        );

    }
    else
    {
        hr = S_FALSE;
    }

Exit:
    SafeRelease(&svgElement);
    return hr;
}
