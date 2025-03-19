#pragma once

#include <memory>
#include <vector>
#include <afxwin.h>

class CustomPictureControl : public CStatic
{
public:
    CustomPictureControl();
    virtual ~CustomPictureControl();

    void LoadVideoFrame(const uint8_t* yData, const uint8_t* uData, const uint8_t* vData, int width, int height);

protected:
    DECLARE_MESSAGE_MAP()
    afx_msg void OnPaint();

private:
    void DrawYUV(CDC* pDC);

    std::vector<uint8_t> m_yData;
    std::vector<uint8_t> m_uData;
    std::vector<uint8_t> m_vData;
    int m_width;
    int m_height;
};