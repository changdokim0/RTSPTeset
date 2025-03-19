#include "CD3dVideoWnd.h"

CMyVideoWnd::CMyVideoWnd()
{
}

CMyVideoWnd::~CMyVideoWnd()
{
    CleanupD3D();
}

BEGIN_MESSAGE_MAP(CMyVideoWnd, CStatic)
END_MESSAGE_MAP()


void CMyVideoWnd::InitD3D(HWND hWnd)
{
    // Create device and swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 800; // Set your width
    sd.BufferDesc.Height = 600; // Set your height
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &sd, &m_pSwapChain, &m_pd3dDevice, nullptr, &m_pImmediateContext);

    // Create render target view
    ID3D11Texture2D* pBackBuffer = nullptr;
    m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    m_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &m_pRenderTargetView);
    pBackBuffer->Release();

    m_pImmediateContext->OMSetRenderTargets(1, &m_pRenderTargetView, nullptr);

    // Setup viewport
    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)800; // Set your width
    vp.Height = (FLOAT)600; // Set your height
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    m_pImmediateContext->RSSetViewports(1, &vp);

    // Load shaders and create input layout
    // Load your shaders here and create input layout

    // Create texture and sampler state
    // Create your texture and sampler state here




        // Compile the vertex shader
    ID3DBlob* pVSBlob = nullptr;
    D3DCompileFromFile(L"YUVShader.hlsl", nullptr, nullptr, "VS", "vs_5_0", 0, 0, &pVSBlob, nullptr);
    m_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &m_pVertexShader);

    // Define the input layout
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    m_pd3dDevice->CreateInputLayout(layout, ARRAYSIZE(layout), pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &m_pVertexLayout);
    pVSBlob->Release();

    // Compile the pixel shader
    ID3DBlob* pPSBlob = nullptr;
    D3DCompileFromFile(L"YUVShader.hlsl", nullptr, nullptr, "PS", "ps_5_0", 0, 0, &pPSBlob, nullptr);
    m_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &m_pPixelShader);
    pPSBlob->Release();

    // Create texture and sampler state
    // Create your texture and sampler state here
}

void CMyVideoWnd::CleanupD3D()
{
    if (m_pImmediateContext) m_pImmediateContext->ClearState();
    if (m_pRenderTargetView) m_pRenderTargetView->Release();
    if (m_pSwapChain) m_pSwapChain->Release();
    if (m_pImmediateContext) m_pImmediateContext->Release();
    if (m_pd3dDevice) m_pd3dDevice->Release();
}

void CMyVideoWnd::Render()
{
    if (m_pImmediateContext == nullptr)
        return;

    // Clear the back buffer
    float ClearColor[4] = { 0.0f, 0.125f, 0.3f, 1.0f };
    m_pImmediateContext->ClearRenderTargetView(m_pRenderTargetView, ClearColor);

    // Render the scene
    // Set your shaders, input layout, and draw your texture here

    // Present the information rendered to the back buffer to the front buffer (the screen)
    m_pSwapChain->Present(0, 0);
}

void CMyVideoWnd::LoadYUV420Texture(const char* yuvFilePath, int width, int height)
{
    FILE* file = fopen(yuvFilePath, "rb");
    if (!file) return;

    int imageSize = width * height * 3 / 2;
    unsigned char* buffer = new unsigned char[imageSize];
    fread(buffer, 1, imageSize, file);
    fclose(file);

    // Create Y, U, V textures
    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData;
    ZeroMemory(&initData, sizeof(initData));
    initData.pSysMem = buffer;
    initData.SysMemPitch = width;

    m_pd3dDevice->CreateTexture2D(&desc, &initData, &m_pYUVTexture);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    m_pd3dDevice->CreateShaderResourceView(m_pYUVTexture, &srvDesc, &m_pYUVTextureView);

    delete[] buffer;
}
