#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include "afxdialogex.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

class CMyVideoWnd : public CStatic
{
public:
    CMyVideoWnd();
    virtual ~CMyVideoWnd();

    void LoadYUV420Texture(const char* yuvFilePath, int width, int height);
    void InitD3D(HWND hWnd);
protected:
    //virtual void PreSubclassWindow();
    afx_msg void OnPaint();
    DECLARE_MESSAGE_MAP()

private:
    ID3D11Device* m_pd3dDevice;
    ID3D11DeviceContext* m_pImmediateContext;
    IDXGISwapChain* m_pSwapChain;
    ID3D11RenderTargetView* m_pRenderTargetView;
    ID3D11Texture2D* m_pYUVTexture;
    ID3D11ShaderResourceView* m_pYUVTextureView;
    ID3D11SamplerState* m_pSamplerLinear;
    ID3D11VertexShader* m_pVertexShader;
    ID3D11PixelShader* m_pPixelShader;
    ID3D11InputLayout* m_pVertexLayout;

    void CleanupD3D();
    void Render();
};
