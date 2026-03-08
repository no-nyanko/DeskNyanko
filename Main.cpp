/*
Copyright (c) 2026 no-nyanko
Licensed under the MIT License.
See LICENSE file for details.
*/

#include <windows.h>
#include <gdiplus.h>
#include <shellapi.h>

#pragma comment(lib,"gdiplus.lib")

ULONG_PTR gdiplusToken;

Gdiplus::Image* gifImage = nullptr;

UINT frameCount = 0;
UINT currentFrame = 0;

GUID dimensionGuid;

UINT* delays = nullptr;

int gifWidth = 0;
int gifHeight = 0;

void ClearGif()
{
    if (gifImage)
    {
        delete gifImage;
        gifImage = nullptr;
    }

    if (delays)
    {
        free(delays);
        delays = nullptr;
    }

    frameCount = 0;
    currentFrame = 0;
}

bool LoadFrameDelays()
{
    UINT propSize = gifImage->GetPropertyItemSize(PropertyTagFrameDelay);

    if (propSize == 0)
    {
        frameCount = 1;

        delays = (UINT*)malloc(sizeof(UINT));
        delays[0] = 10;

        return true;
    }

    Gdiplus::PropertyItem* propItem = (Gdiplus::PropertyItem*)malloc(propSize);

    if (gifImage->GetPropertyItem(PropertyTagFrameDelay, propSize, propItem) )
    {
        free(propItem);
        return false;
    }

    delays = (UINT*)malloc(frameCount * sizeof(UINT));

    memcpy(delays, propItem->value, frameCount * sizeof(UINT));

    free(propItem);

    for (UINT i = 0; i < frameCount; i++)
    {
        if (delays[i] == 0)
            delays[i] = 10;
    }

    return true;
}

void LoadGifFromResource()
{
    ClearGif();

    HRSRC res = FindResource(NULL, MAKEINTRESOURCE(101), RT_RCDATA);
    if (!res) return;

    DWORD size = SizeofResource(NULL, res);

    HGLOBAL resData = LoadResource(NULL, res);
    if (!resData) return;

    void* pData = LockResource(resData);

    HGLOBAL hBuffer = GlobalAlloc(GMEM_MOVEABLE, size);

    void* pBuffer = GlobalLock(hBuffer);

    memcpy(pBuffer, pData, size);

    GlobalUnlock(hBuffer);

    IStream* stream;

    if (CreateStreamOnHGlobal(hBuffer, TRUE, &stream) != S_OK)
        return;

    gifImage = Gdiplus::Image::FromStream(stream);

    stream->Release();

    if (!gifImage)
        return;

    gifWidth = gifImage->GetWidth();
    gifHeight = gifImage->GetHeight();

    GUID dimensionIDs[1];

    gifImage->GetFrameDimensionsList(dimensionIDs, 1);

    dimensionGuid = dimensionIDs[0];

    frameCount = gifImage->GetFrameCount(&dimensionGuid);

    LoadFrameDelays();
}

void DrawLayered(HWND hwnd)
{
    if (!gifImage) return;

    HDC screenDC = GetDC(NULL);
    HDC memDC = CreateCompatibleDC(screenDC);

    BITMAPINFO bmi = {};

    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = gifWidth;
    bmi.bmiHeader.biHeight = -gifHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;

    void* bits;

    HBITMAP bmp = CreateDIBSection(
        screenDC,
        &bmi,
        DIB_RGB_COLORS,
        &bits,
        NULL,
        0
    );

    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, bmp);

    Gdiplus::Graphics g(memDC);

    g.Clear(Gdiplus::Color(0, 0, 0, 0));

    g.DrawImage(gifImage, 0, 0, gifWidth, gifHeight);

    RECT rc;

    GetWindowRect(hwnd, &rc);

    POINT ptPos = { rc.left, rc.top };

    SIZE sizeWindow = { gifWidth,gifHeight };

    POINT ptSrc = { 0,0 };

    BLENDFUNCTION blend = {};

    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(
        hwnd,
        screenDC,
        &ptPos,
        &sizeWindow,
        memDC,
        &ptSrc,
        0,
        &blend,
        ULW_ALPHA
    );

    SelectObject(memDC, oldBmp);

    DeleteObject(bmp);
    DeleteDC(memDC);

    ReleaseDC(NULL, screenDC);
}

void LoadGif(HWND hwnd, const wchar_t* path)
{
    ClearGif();

    gifImage = Gdiplus::Image::FromFile(path);

    if (!gifImage)
        return;

    gifWidth = gifImage->GetWidth();
    gifHeight = gifImage->GetHeight();

    SetWindowPos(hwnd, HWND_TOPMOST, 100, 100, gifWidth, gifHeight, SWP_SHOWWINDOW);

    GUID dimensionIDs[1];

    gifImage->GetFrameDimensionsList(dimensionIDs, 1);

    dimensionGuid = dimensionIDs[0];

    frameCount = gifImage->GetFrameCount(&dimensionGuid);

    LoadFrameDelays();

    currentFrame = 0;

    SetTimer(hwnd, 1, delays[0] * 10, NULL);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {

    case WM_CREATE:

        DragAcceptFiles(hwnd, TRUE);

        LoadGifFromResource();

        SetWindowPos(hwnd, HWND_TOPMOST, 200, 200, gifWidth, gifHeight, 0);

        if (delays)
            SetTimer(hwnd, 1, delays[0] * 10, NULL);

        break;

    case WM_DROPFILES:
    {
        wchar_t file[MAX_PATH];

        DragQueryFile((HDROP)wParam, 0, file, MAX_PATH);

        LoadGif(hwnd, file);

        DragFinish((HDROP)wParam);
    }
    break;

    case WM_TIMER:

        if (gifImage && frameCount > 0)
        {
            currentFrame = (currentFrame + 1) % frameCount;

            gifImage->SelectActiveFrame(&dimensionGuid, currentFrame);

            DrawLayered(hwnd);

            SetTimer(hwnd, 1, delays[currentFrame] * 10, NULL);
        }

        break;

    case WM_LBUTTONDOWN:

        SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);

        break;

    case WM_RBUTTONDOWN:

        DestroyWindow(hwnd);

        break;

    case WM_DESTROY:

        KillTimer(hwnd, 1);

        ClearGif();

        Gdiplus::GdiplusShutdown(gdiplusToken);

        PostQuitMessage(0);

        break;

    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;

    Gdiplus::GdiplusStartup(
        &gdiplusToken,
        &gdiplusStartupInput,
        NULL
    );

    WNDCLASS wc = {};

    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"GifViewer";

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOPMOST,
        L"GifViewer",
        L"",
        WS_POPUP,
        200,
        200,
        300,
        200,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    ShowWindow(hwnd, nCmdShow);

    MSG msg;

    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}