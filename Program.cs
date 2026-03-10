using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;

delegate IntPtr WndProcDelegate(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);

class DeskNyanko
{

    const int WS_POPUP = unchecked((int)0x80000000);
    const int WS_VISIBLE = 0x10000000;

    const int WS_EX_LAYERED = 0x80000;
    const int WS_EX_TOPMOST = 0x8;
    const int WS_EX_TRANSPARENT = 0x20;

    const int WM_TIMER = 0x113;
    const int WM_LBUTTONDOWN = 0x201;
    const int WM_LBUTTONUP = 0x202;
    const int WM_MOUSEMOVE = 0x200;
    const int WM_RBUTTONDOWN = 0x204;
    const int WM_DROPFILES = 0x233;

    const int ULW_ALPHA = 2;

    static IntPtr hwnd;

    static bool dragging;
    static int dragX;
    static int dragY;
    static Image gif;
    static FrameDimension dimension;

    static int frame;
static int frameCount = 1;

static int[] delays = Array.Empty<int>();

    static WndProcDelegate wndProcDelegate = WndProc;

    static void Main()
    {
        string gifPath =
            Path.Combine(AppContext.BaseDirectory, "nyanko.gif");

        if (!File.Exists(gifPath))
            return;

        LoadGif(gifPath);

        WNDCLASS wc = new();
        wc.lpfnWndProc = Marshal.GetFunctionPointerForDelegate(wndProcDelegate);
        wc.lpszClassName = "DeskNyanko";

        RegisterClass(ref wc);

        hwnd = CreateWindowEx(
            WS_EX_LAYERED | WS_EX_TOPMOST,
            wc.lpszClassName,
            "",
            WS_POPUP | WS_VISIBLE,
            200, 200,
            gif.Width,
            gif.Height,
            IntPtr.Zero,
            IntPtr.Zero,
            IntPtr.Zero,
            IntPtr.Zero);

        DragAcceptFiles(hwnd, true);

        SetTimer(hwnd, 1, delays[0], IntPtr.Zero);

        MSG msg;

        while (GetMessage(out msg, IntPtr.Zero, 0, 0))
        {
            TranslateMessage(ref msg);
            DispatchMessage(ref msg);
        }
    }

    static void LoadGif(string path)
    {
        if (!File.Exists(path)) return;

        gif?.Dispose();

        gif = Image.FromFile(path);

        dimension =
            new FrameDimension(gif.FrameDimensionsList[0]);

        frameCount = gif.GetFrameCount(dimension);

        frame = 0;

        delays = new int[frameCount];

        try
        {
            PropertyItem item = gif.GetPropertyItem(0x5100);

            for (int i = 0; i < frameCount; i++)
            {
                int delay =
                    BitConverter.ToInt32(item.Value, i * 4);

                if (delay <= 1) delay = 10;

                delays[i] = delay * 10;
            }
        }
        catch
        {
            for (int i = 0; i < frameCount; i++)
                delays[i] = 100;
        }
    }

    static void Draw()
    {
        gif.SelectActiveFrame(dimension, frame);

        using Bitmap bmp = new(gif);

        IntPtr screen = GetDC(IntPtr.Zero);
        IntPtr mem = CreateCompatibleDC(screen);

        IntPtr hBitmap = bmp.GetHbitmap(Color.FromArgb(0));
        IntPtr old = SelectObject(mem, hBitmap);

        SIZE size = new(bmp.Width, bmp.Height);
        POINT src = new(0, 0);

        GetWindowRect(hwnd, out RECT rect);

        POINT pos = new(rect.left, rect.top);

        BLENDFUNCTION blend = new();
        blend.BlendOp = 0;
        blend.SourceConstantAlpha = 255;
        blend.AlphaFormat = 1;

        UpdateLayeredWindow(
            hwnd,
            screen,
            ref pos,
            ref size,
            mem,
            ref src,
            0,
            ref blend,
            ULW_ALPHA);

        SelectObject(mem, old);
        DeleteObject(hBitmap);

        DeleteDC(mem);
        ReleaseDC(IntPtr.Zero, screen);
    }

    static IntPtr WndProc(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam)
    {
        switch (msg)
        {
            case WM_TIMER:

                frame++;

                if (frame >= frameCount)
                    frame = 0;

                Draw();

                SetTimer(hwnd, 1, delays[frame], IntPtr.Zero);

                break;

            case 0x0201: // WM_LBUTTONDOWN
                ReleaseCapture();
                SendMessage(hwnd, 0xA1, (IntPtr)2, IntPtr.Zero);
                break;

            case WM_RBUTTONDOWN:

                PostQuitMessage(0);

                break;

            case WM_DROPFILES:

                uint len = DragQueryFile(wParam, 0, null, 0);

                if (len > 0)
                {
                    char[] buffer = new char[len + 1];

                    DragQueryFile(wParam, 0, buffer, buffer.Length);

                    string path =
                        new string(buffer).TrimEnd('\0');

                    if (File.Exists(path))
                    {
                        KillTimer(hwnd, 1);

                        LoadGif(path);

                        SetTimer(hwnd, 1, delays[0], IntPtr.Zero);
                    }
                }

                DragFinish(wParam);

                break;
        }

        return DefWindowProc(hWnd, msg, wParam, lParam);
    }

    struct POINT { public int x, y; public POINT(int x, int y) { this.x = x; this.y = y; } }


    struct SIZE { public int cx, cy; public SIZE(int x, int y) { cx = x; cy = y; } }
    struct RECT { public int left, top, right, bottom; }

    struct MSG
    {
        public IntPtr hwnd;
        public uint message;
        public IntPtr wParam;
        public IntPtr lParam;
        public uint time;
        public POINT pt;
    }

    struct WNDCLASS
    {
        public uint style;
        public IntPtr lpfnWndProc;
        public int cbClsExtra;
        public int cbWndExtra;
        public IntPtr hInstance;
        public IntPtr hIcon;
        public IntPtr hCursor;
        public IntPtr hbrBackground;
        public string lpszMenuName;
        public string lpszClassName;
    }

    struct BLENDFUNCTION
    {
        public byte BlendOp;
        public byte BlendFlags;
        public byte SourceConstantAlpha;
        public byte AlphaFormat;
    }

    [DllImport("user32.dll")] static extern ushort RegisterClass(ref WNDCLASS wc);
    [DllImport("user32.dll")] static extern IntPtr CreateWindowEx(int ex, string cls, string name, int style, int x, int y, int w, int h, IntPtr p, IntPtr m, IntPtr i, IntPtr param);
    [DllImport("user32.dll")] static extern bool GetMessage(out MSG msg, IntPtr hwnd, uint min, uint max);
    [DllImport("user32.dll")] static extern bool TranslateMessage(ref MSG msg);
    [DllImport("user32.dll")] static extern IntPtr DispatchMessage(ref MSG msg);
    [DllImport("user32.dll")] static extern IntPtr DefWindowProc(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")] static extern void PostQuitMessage(int code);

    [DllImport("user32.dll")] static extern IntPtr SetTimer(IntPtr hwnd, int id, int elapse, IntPtr func);
    [DllImport("user32.dll")] static extern bool KillTimer(IntPtr hwnd, int id);

    [DllImport("user32.dll")] static extern IntPtr GetDC(IntPtr hwnd);
    [DllImport("user32.dll")] static extern bool ReleaseDC(IntPtr hwnd, IntPtr dc);
    [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr hwnd, out RECT rect);

    [DllImport("gdi32.dll")] static extern IntPtr CreateCompatibleDC(IntPtr dc);
    [DllImport("gdi32.dll")] static extern bool DeleteDC(IntPtr dc);
    [DllImport("gdi32.dll")] static extern IntPtr SelectObject(IntPtr dc, IntPtr obj);
    [DllImport("gdi32.dll")] static extern bool DeleteObject(IntPtr obj);

    [DllImport("user32.dll")]
    static extern bool UpdateLayeredWindow(
        IntPtr hwnd,
        IntPtr dst,
        ref POINT pos,
        ref SIZE size,
        IntPtr src,
        ref POINT srcPt,
        int key,
        ref BLENDFUNCTION blend,
        int flags);

    [DllImport("user32.dll")]
    static extern bool ReleaseCapture();

    [DllImport("user32.dll")]
    static extern IntPtr SendMessage(
        IntPtr hWnd,
        int msg,
        IntPtr wParam,
        IntPtr lParam);

    [DllImport("shell32.dll")] static extern void DragAcceptFiles(IntPtr hwnd, bool accept);

    [DllImport("shell32.dll", CharSet = CharSet.Unicode)]
    static extern uint DragQueryFile(
        IntPtr hDrop,
        uint iFile,
        char[]? file,
        int size);

    [DllImport("shell32.dll")] static extern void DragFinish(IntPtr hDrop);
}