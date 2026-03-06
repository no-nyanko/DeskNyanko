#include <windows.h>
#include <d2d1.h>
#include <wincodec.h>

#pragma comment(lib,"d2d1.lib")
#pragma comment(lib,"windowscodecs.lib")

ID2D1Factory* factory = nullptr;
ID2D1DCRenderTarget* renderTarget = nullptr;

IWICImagingFactory* wicFactory = nullptr;
IWICBitmapDecoder* decoder = nullptr;

ID2D1Bitmap* bitmap = nullptr;

UINT frameCount = 0;
UINT currentFrame = 0;
UINT* delays = nullptr;

UINT width = 0;
UINT height = 0;

GUID dimension;

HBITMAP dibBitmap;
void* bits;

void ReleaseGif()
{
    if (bitmap) { bitmap->Release(); bitmap = nullptr; }
    if (decoder) { decoder->Release(); decoder = nullptr; }
    if (delays) { free(delays); delays = nullptr; }
}

void LoadFrame()
{
    IWICBitmapFrameDecode* frame = nullptr;
    decoder->GetFrame(currentFrame, &frame);

    IWICFormatConverter* converter = nullptr;
    wicFactory->CreateFormatConverter(&converter);

    converter->Initialize(
        frame,
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone,
        NULL,
        0,
        WICBitmapPaletteTypeCustom
    );

    if (bitmap) bitmap->Release();

    renderTarget->CreateBitmapFromWicBitmap(
        converter,
        NULL,
        &bitmap
    );

    converter->Release();
    frame->Release();
}

void LoadGif(HWND hwnd, const wchar_t* path)
{
    ReleaseGif();

    wicFactory->CreateDecoderFromFilename(
        path,
        NULL,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        &decoder
    );

    decoder->GetFrameCount(&frameCount);

    IWICBitmapFrameDecode* frame;
    decoder->GetFrame(0, &frame);
    frame->GetSize(&width, &height);
    frame->Release();

    delays = (UINT*)malloc(sizeof(UINT) * frameCount);

    for (UINT i = 0; i < frameCount; i++)
    {
        IWICBitmapFrameDecode* f;
        decoder->GetFrame(i, &f);

        IWICMetadataQueryReader* reader;
        f->GetMetadataQueryReader(&reader);

        PROPVARIANT var;
        PropVariantInit(&var);

        reader->GetMetadataByName(
            L"/grctlext/Delay",
            &var
        );

        delays[i] = var.uiVal * 10;

        PropVariantClear(&var);

        reader->Release();
        f->Release();
    }

    LoadFrame();

    SetWindowPos(
        hwnd,
        HWND_TOPMOST,
        200,
        200,
        width,
        height,
        SWP_SHOWWINDOW
    );

    SetTimer(hwnd, 1, delays[0], NULL);
}

void Render(HWND hwnd)
{
    HDC screen = GetDC(NULL);
    HDC mem = CreateCompatibleDC(screen);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    dibBitmap = CreateDIBSection(
        screen,
        &bmi,
        DIB_RGB_COLORS,
        &bits,
        NULL,
        0
    );

    SelectObject(mem, dibBitmap);

    RECT rc = { 0,0,(LONG)width,(LONG)height };

    renderTarget->BindDC(mem, &rc);

    renderTarget->BeginDraw();

    renderTarget->Clear(
        D2D1::ColorF(0, 0)
    );

    if (bitmap)
        renderTarget->DrawBitmap(bitmap);

    renderTarget->EndDraw();

    POINT ptPos;
    RECT wr;
    GetWindowRect(hwnd, &wr);

    ptPos.x = wr.left;
    ptPos.y = wr.top;

    SIZE sizeWindow = { (LONG)width,(LONG)height };
    POINT ptSrc = { 0,0 };

    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(
        hwnd,
        screen,
        &ptPos,
        &sizeWindow,
        mem,
        &ptSrc,
        0,
        &blend,
        ULW_ALPHA
    );

    DeleteObject(dibBitmap);
    DeleteDC(mem);
    ReleaseDC(NULL, screen);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {

    case WM_CREATE:
    {
        DragAcceptFiles(hwnd, TRUE);

        D2D1CreateFactory(
            D2D1_FACTORY_TYPE_SINGLE_THREADED,
            &factory
        );

        D2D1_RENDER_TARGET_PROPERTIES props =
            D2D1::RenderTargetProperties(
                D2D1_RENDER_TARGET_TYPE_DEFAULT,
                D2D1::PixelFormat(
                    DXGI_FORMAT_B8G8R8A8_UNORM,
                    D2D1_ALPHA_MODE_PREMULTIPLIED
                )
            );

        factory->CreateDCRenderTarget(
            &props,
            &renderTarget
        );

        CoInitialize(NULL);

        CoCreateInstance(
            CLSID_WICImagingFactory,
            NULL,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&wicFactory)
        );

        wchar_t path[MAX_PATH];
        GetModuleFileName(NULL, path, MAX_PATH);

        wchar_t* p = wcsrchr(path, L'\\');
        if (p) wcscpy_s(p + 1, MAX_PATH, L"sample.gif");

        LoadGif(hwnd, path);
    }
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

        currentFrame++;
        if (currentFrame >= frameCount)
            currentFrame = 0;

        LoadFrame();
        Render(hwnd);

        SetTimer(hwnd, 1, delays[currentFrame], NULL);

        break;

    case WM_LBUTTONDOWN:

        SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);

        break;

    case WM_RBUTTONDOWN:

        DestroyWindow(hwnd);

        break;

    case WM_DESTROY:

        ReleaseGif();

        if (renderTarget) renderTarget->Release();
        if (factory) factory->Release();
        if (wicFactory) wicFactory->Release();

        PostQuitMessage(0);

        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
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
        300,
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

//no-nyanko