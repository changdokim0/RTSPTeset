#include "pch.h"
#include "CD3dVideoWnd.h"
#include <algorithm> // std::clamp

CustomPictureControl::CustomPictureControl()
    : m_width(0), m_height(0)
{
}

CustomPictureControl::~CustomPictureControl()
{
}

BEGIN_MESSAGE_MAP(CustomPictureControl, CStatic)
    ON_WM_PAINT()
END_MESSAGE_MAP()

void CustomPictureControl::LoadVideoFrame(const uint8_t* yData, const uint8_t* uData, const uint8_t* vData, int width, int height)
{
    m_width = width;
    m_height = height;

    m_yData.assign(yData, yData + width * height);
    m_uData.assign(uData, uData + width * height / 4);
    m_vData.assign(vData, vData + width * height / 4);

    Invalidate(); // Force the window to repaint
}

void CustomPictureControl::OnPaint()
{
    CPaintDC dc(this);
    DrawYUV(&dc);
}

void CustomPictureControl::DrawYUV(CDC* pDC)
{
    if (m_width == 0 || m_height == 0)
        return;

    CRect rect;
    GetClientRect(&rect);

    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = m_width;
    bmi.bmiHeader.biHeight = -m_height; // Negative to indicate top-down bitmap
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    std::vector<uint8_t> rgbData(m_width * m_height * 3);

    for (int y = 0; y < m_height; ++y)
    {
        for (int x = 0; x < m_width; ++x)
        {
            int yIndex = y * m_width + x;
            int uIndex = (y / 2) * (m_width / 2) + (x / 2);
            int vIndex = (y / 2) * (m_width / 2) + (x / 2);

            uint8_t Y = m_yData[yIndex];
            uint8_t U = m_uData[uIndex];
            uint8_t V = m_vData[vIndex];

            int C = Y - 16;
            int D = U - 128;
            int E = V - 128;

            uint8_t R = std::clamp((298 * C + 409 * E + 128) >> 8, 0, 255);
            uint8_t G = std::clamp((298 * C - 100 * D - 208 * E + 128) >> 8, 0, 255);
            uint8_t B = std::clamp((298 * C + 516 * D + 128) >> 8, 0, 255);

            int rgbIndex = (y * m_width + x) * 3;
            rgbData[rgbIndex] = B;
            rgbData[rgbIndex + 1] = G;
            rgbData[rgbIndex + 2] = R;
        }
    }

    //StretchDIBits(pDC->GetSafeHdc(),
    //    rect.left, rect.top, rect.Width(), rect.Height(),
    //    0, 0, m_width, m_height,
    //    rgbData.data(), &bmi, DIB_RGB_COLORS, SRCCOPY);
    // 
    // 
    // 
    // 
    //SetDIBitsToDevice(pDC->GetSafeHdc(),
    //    rect.left, rect.top, m_width, m_height,
    //    0, 0, 0, m_height,
    //    rgbData.data(), &bmi, DIB_RGB_COLORS);
    // 
    // 
    // 
    int oldStretchMode = pDC->SetStretchBltMode(HALFTONE);
    pDC->SetBrushOrg(0, 0); // Required for HALFTONE mode

    // Calculate aspect ratio
    float aspectRatio = static_cast<float>(m_width) / m_height;
    int newWidth = rect.Width();
    int newHeight = static_cast<int>(newWidth / aspectRatio);

    if (newHeight > rect.Height())
    {
        newHeight = rect.Height();
        newWidth = static_cast<int>(newHeight * aspectRatio);
    }

    int offsetX = (rect.Width() - newWidth) / 2;
    int offsetY = (rect.Height() - newHeight) / 2;

    StretchDIBits(pDC->GetSafeHdc(),
        rect.left + offsetX, rect.top + offsetY, newWidth, newHeight,
        0, 0, m_width, m_height,
        rgbData.data(), &bmi, DIB_RGB_COLORS, SRCCOPY);

    // Restore the old stretching mode
    pDC->SetStretchBltMode(oldStretchMode);
}