#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <iostream>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#define HR_CHECK(hr) \
    if (FAILED(hr)) { \
        _com_error err(hr); \
        LPCTSTR errMsg = err.ErrorMessage(); \
        std::wcerr << L"Error: " << errMsg << L" in " \
                   << __FILE__ << L" at line " << __LINE__ \
                   << std::endl; \
    }

// -----------------------------------------------------------------------------
const char* vertexShaderSrc = R"(
struct VS_INPUT {
    float3 position : POSITION;
    float4 color : COLOR;
};

struct PS_INPUT {
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

cbuffer ConstantBuffer : register(b0) {
    matrix FinalMatrix;
};

PS_INPUT VSMain(VS_INPUT input) {
    PS_INPUT output;
    output.position = float4(input.position, 1.0f);
    output.position = mul(FinalMatrix, output.position);
    output.color = input.color;
    return output;
}
)";

const char* pixelShaderSrc = R"(
struct PS_INPUT {
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

float4 PSMain(PS_INPUT input) : SV_TARGET {
    return input.color;
}
)";

// -----------------------------------------------------------------------------
struct Vertex {
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT4 color;
};

// A constant buffer for passing the world-view-projection matrix
struct CBUFFER {
    DirectX::XMMATRIX FinalMatrix;
};

// -----------------------------------------------------------------------------
class MainWindow {
 public:
    MainWindow();

    ~MainWindow();

    bool init();

    void mainloop();

    HRESULT resizeSwapChain(uint32_t width, uint32_t height);

 private:
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message,
                                       WPARAM wParam, LPARAM lParam);

 private:
    bool is_fullscreen_;
    HWND hWnd_;
    ID3D11RenderTargetView* pRenderTargetView_;
    ID3D11Device* pDevice_;
    ID3D11DeviceContext* pDeviceContext_;
    IDXGISwapChain1* pSwapChain_;
    ID3D11VertexShader* pVertexShader_;
    ID3D11PixelShader* pPixelShader_;
    ID3D11InputLayout* pInputLayout_;
    ID3D11Buffer* pVertexBuffer_;
    ID3D11Buffer* pConstBuffer_;

    HWND createWindow();
    HRESULT initD3D();
    HRESULT initPipeline();
    HRESULT initGraphics();
    HRESULT renderFrame();
    void toggleFullscreen();
    void releaseResources(ID3D11RenderTargetView* renderTargetView);

    // Get
    const bool isFullscreen() const { return is_fullscreen_; }
    HWND hWnd() const { return hWnd_; }
    ID3D11Device* device() const { return pDevice_; }
    ID3D11DeviceContext* deviceCtx() const { return pDeviceContext_; }
    IDXGISwapChain1* swapChain() const { return pSwapChain_; }
    ID3D11VertexShader* vertexShader() const { return pVertexShader_; }
    ID3D11PixelShader* pixelShader() const { return pPixelShader_; }
    ID3D11Buffer* vertexBuffer() const { return pVertexBuffer_; }
};

// -----------------------------------------------------------------------------
MainWindow::MainWindow()
    : is_fullscreen_(false) {
}

MainWindow::~MainWindow() {
}

void MainWindow::toggleFullscreen() {
    is_fullscreen_ = !is_fullscreen_;

    HRESULT hr = swapChain()->SetFullscreenState(isFullscreen(), nullptr);
    if (FAILED(hr)) {
        std::cerr << "Failed to toggle fullscreen mode." << std::endl;
        return;
    }
}

void MainWindow::releaseResources(ID3D11RenderTargetView* render_target_view) {
    if (render_target_view != nullptr) {
        render_target_view->Release();
    }
}

HRESULT MainWindow::resizeSwapChain(uint32_t width, uint32_t height) {
    HRESULT hr;

    // Release existing resources
    releaseResources(pRenderTargetView_);
    pRenderTargetView_ = nullptr;

    // Resize buffers in swapchain
    hr = swapChain()->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) return hr;

    // Recreate the render target view
    ID3D11Texture2D* backBuffer = nullptr;
    hr = swapChain()->GetBuffer(0,
                                __uuidof(ID3D11Texture2D),
                                reinterpret_cast<void**>(&backBuffer));
    if (FAILED(hr)) return hr;

    hr = device()->CreateRenderTargetView(backBuffer,
                                          nullptr,
                                          &pRenderTargetView_);
    if (FAILED(hr)) {
        std::cerr << "Failed to create render target view" << std::endl;
        return hr;
    }

    deviceCtx()->OMSetRenderTargets(1, &pRenderTargetView_, nullptr);
    backBuffer->Release();

    // Set up the viewport
    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(width);
    viewport.Height = static_cast<float>(height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    deviceCtx()->RSSetViewports(1, &viewport);

    return S_OK;
}

HRESULT MainWindow::initD3D() {
    HRESULT hr = S_OK;

    // Create device and device context
    D3D_FEATURE_LEVEL featureLevel;
    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                           nullptr, 0, D3D11_SDK_VERSION,
                           &pDevice_, &featureLevel, &pDeviceContext_);
    if (FAILED(hr)) {
        std::cerr << "Failed to create device" << std::endl;
        return hr;
    }

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = 0;
    swapChainDesc.Height = 0;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Scaling = DXGI_SCALING_NONE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.Flags = 0;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    IDXGIDevice* dxgiDevice = nullptr;
    hr = device()->QueryInterface(__uuidof(IDXGIDevice),
                                  reinterpret_cast<void**>(&dxgiDevice));
    if (FAILED(hr)) {
        std::cerr << "Failed to query IDXGIDevice interface" << std::endl;
        return hr;
    }

    IDXGIAdapter* dxgiAdapter = nullptr;
    hr = dxgiDevice->GetAdapter(&dxgiAdapter);
    if (FAILED(hr)) {
        std::cerr << "Failed to GetAdapter from device" << std::endl;
        return hr;
    }
    dxgiDevice->Release();

    IDXGIFactory2* dxgiFactory = nullptr;
    hr = dxgiAdapter->GetParent(__uuidof(IDXGIFactory2),
                                reinterpret_cast<void**>(&dxgiFactory));
    dxgiAdapter->Release();
    if (FAILED(hr)) {
        std::cerr << "Failed to IDXGIFactory2 from adapter" << std::endl;
        return hr;
    }

    // Create swapchain
    hr = dxgiFactory->CreateSwapChainForHwnd(device(), hWnd(), &swapChainDesc,
                                             nullptr, nullptr, &pSwapChain_);
    dxgiFactory->Release();
    if (FAILED(hr)) {
        std::cerr << "Failed to create swapchain" << std::endl;
        return hr;
    }


    ID3D11Texture2D* backBuffer = nullptr;
    swapChain()->GetBuffer(0, __uuidof(ID3D11Texture2D),
                           reinterpret_cast<void**>(&backBuffer));
    device()->CreateRenderTargetView(backBuffer, nullptr, &pRenderTargetView_);
    backBuffer->Release();

    deviceCtx()->OMSetRenderTargets(1, &pRenderTargetView_, nullptr);

    D3D11_VIEWPORT viewport = {};
    viewport.Width = 1920.0f;
    viewport.Height = 1080.0f;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;

    deviceCtx()->RSSetViewports(1, &viewport);

    return hr;
}

HRESULT MainWindow::initPipeline() {
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;

    HRESULT hr;
    hr = D3DCompile(vertexShaderSrc, strlen(vertexShaderSrc), nullptr, nullptr,
                    nullptr, "VSMain", "vs_5_0", 0, 0, &vsBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            std::cerr << reinterpret_cast<char*>(errorBlob->GetBufferPointer())
                      << std::endl;
            errorBlob->Release();
        }
        return hr;
    }

    hr = D3DCompile(pixelShaderSrc, strlen(pixelShaderSrc), nullptr, nullptr,
                    nullptr, "PSMain", "ps_5_0", 0, 0, &psBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            std::cerr << reinterpret_cast<char*>(errorBlob->GetBufferPointer())
                      << std::endl;
            errorBlob->Release();
        }
        return hr;
    }

    device()->CreateVertexShader(vsBlob->GetBufferPointer(),
                                 vsBlob->GetBufferSize(),
                                 nullptr, &pVertexShader_);
    device()->CreatePixelShader(psBlob->GetBufferPointer(),
                                psBlob->GetBufferSize(),
                                nullptr, &pPixelShader_);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    device()->CreateInputLayout(layout, 2,
                                vsBlob->GetBufferPointer(),
                                vsBlob->GetBufferSize(),
                                &pInputLayout_);
    deviceCtx()->IASetInputLayout(pInputLayout_);

    vsBlob->Release();
    psBlob->Release();

    // Create the constant buffer
    D3D11_BUFFER_DESC cbd;
    ZeroMemory(&cbd, sizeof(D3D11_BUFFER_DESC));
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.ByteWidth = sizeof(CBUFFER);
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    device()->CreateBuffer(&cbd, NULL, &pConstBuffer_);

    // Set the constant buffer
    deviceCtx()->VSSetConstantBuffers(0, 1, &pConstBuffer_);

    return hr;
}

HRESULT MainWindow::initGraphics() {
    HRESULT hr = S_OK;
    Vertex vertices[] = {
        { DirectX::XMFLOAT3(0.0f, 0.5f, 0.0f), DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
        { DirectX::XMFLOAT3(0.5f, -0.5f, 0.0f), DirectX::XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
        { DirectX::XMFLOAT3(-0.5f, -0.5f, 0.0f), DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
    };

    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.ByteWidth = sizeof(vertices);
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices;

    hr = device()->CreateBuffer(&bufferDesc, &initData, &pVertexBuffer_);
    if (FAILED(hr)) {
        std::cerr << "Failed to create buffer" << std::endl;
        return hr;
    }

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    deviceCtx()->IASetVertexBuffers(0, 1, &pVertexBuffer_, &stride, &offset);
    deviceCtx()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    return hr;
}

HRESULT MainWindow::renderFrame() {
    // Rebind the Render Target View
    ID3D11Texture2D* backBuffer = nullptr;
    HRESULT hr = S_OK;

    // Create rotation matrix
    static float Time = 0.0f;
    Time += 0.01f;
    DirectX::XMMATRIX RotationMatrix = DirectX::XMMatrixRotationZ(Time);

    // Update the constant buffer
    CBUFFER cb;
    cb.FinalMatrix = XMMatrixTranspose(RotationMatrix);
    deviceCtx()->UpdateSubresource(pConstBuffer_, 0, NULL, &cb, 0, 0);

    // draw
    deviceCtx()->OMSetRenderTargets(1, &pRenderTargetView_, nullptr);

    float clearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };
    deviceCtx()->ClearRenderTargetView(pRenderTargetView_, clearColor);
    deviceCtx()->VSSetShader(vertexShader(), nullptr, 0);
    deviceCtx()->PSSetShader(pixelShader(), nullptr, 0);

    deviceCtx()->Draw(3, 0);

    swapChain()->Present(1, 0);

    return hr;
}

bool MainWindow::init() {
    hWnd_ = this->createWindow();

    do {
        if (FAILED(initD3D())) break;

        if (FAILED(initPipeline())) break;

        if (FAILED(initGraphics())) break;

        return true;
    } while (false);

    return false;
}

void MainWindow::mainloop() {
    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            renderFrame();
        }
    }
}

LRESULT CALLBACK MainWindow::WindowProc(
        HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto winPtr = GetWindowLongPtr(hWnd, GWLP_USERDATA);
    auto* pWindow = reinterpret_cast<MainWindow*>(winPtr);

    switch (message) {
    case WM_KEYDOWN:
        if (wParam == VK_F11) {
            pWindow->toggleFullscreen();

            RECT rect;
            GetClientRect(hWnd, &rect);
            UINT width = rect.right - rect.left;
            UINT height = rect.bottom - rect.top;

            HRESULT hr = pWindow->resizeSwapChain(width, height);
            if (FAILED(hr)) {
                exit(1);
            }
        }
        break;

    case WM_SIZE:
        // TBD
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}


HWND MainWindow::createWindow() {
    // Get screen resolution
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    WNDCLASS wc = {};
    wc.lpfnWndProc = MainWindow::WindowProc;
    wc.hInstance = nullptr;
    wc.lpszClassName = "MainWindow";

    RegisterClass(&wc);

    HWND hWnd = CreateWindowExA(
        0,
        "MainWindow",
        "DirectX 11 Triangle",
        WS_POPUP,
        CW_USEDEFAULT, CW_USEDEFAULT, screenWidth, screenHeight,
        nullptr, nullptr, nullptr, nullptr);

    if (hWnd) {
        ShowWindow(hWnd, true);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    }
    return hWnd;
}

// -----------------------------------------------------------------------------
// Main
int main() {
    MainWindow window;

    window.init();
    window.mainloop();

    return 0;
}
