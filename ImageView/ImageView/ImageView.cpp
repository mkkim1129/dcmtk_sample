// ImageView.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "ImageView.h"

#include <AtlConv.h>
#include <Commdlg.h>

#include <winsock.h>
#pragma comment(lib, "netapi32.lib")
#pragma comment(lib, "wsock32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")

// DCMTK
#include "dcmtk/dcmimgle/dcmimage.h"

#pragma comment(lib, "oflog.lib")
#pragma comment(lib, "ofstd.lib")
#pragma comment(lib, "dcmdata.lib")
#pragma comment(lib, "dcmimgle.lib")
#pragma comment(lib, "dcmimage.lib")

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_IMAGEVIEW, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_IMAGEVIEW));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_IMAGEVIEW));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_IMAGEVIEW);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

DicomImage* pImage = 0;
//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_FILE_OPEN:
            {
              OPENFILENAME ofn;
              char lpstrFile[MAX_PATH] = "";
              memset(&ofn, 0, sizeof(ofn));
              ofn.lStructSize = sizeof(ofn);
              ofn.hwndOwner = hWnd;
              ofn.lpstrFile = lpstrFile;
              ofn.nMaxFile = MAX_PATH;

              if (GetOpenFileName(&ofn) != 0)
              {
                USES_CONVERSION;
                char* filename = ofn.lpstrFile;

                delete pImage;

                // 파일열기
                pImage = new DicomImage(filename);

                _ASSERT(pImage);
                _ASSERT(pImage->getStatus() == EIS_Normal);

                InvalidateRect(hWnd, NULL, FALSE);
              }
            }
            break;
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            
            if (pImage)
            {
              const int width = (int)pImage->getWidth();
              const int height = (int)pImage->getHeight();
              void *data = NULL;

              // 비트맵으로 변환
              if (pImage->createWindowsDIB(data, width*height) && data)
              {
                BITMAPINFO bi;
                bi.bmiHeader.biSize = sizeof(bi);
                bi.bmiHeader.biWidth = width;
                bi.bmiHeader.biHeight = -height;
                bi.bmiHeader.biPlanes = 1;
                bi.bmiHeader.biBitCount = 24;
                bi.bmiHeader.biCompression = BI_RGB;
                bi.bmiHeader.biSizeImage = 0;

                HBITMAP hbmp = CreateDIBitmap(hdc, &bi.bmiHeader,
                  CBM_INIT, data, &bi, DIB_RGB_COLORS);
                if (hbmp)
                {
                  HDC memDC = CreateCompatibleDC(hdc);
                  SelectObject(memDC, hbmp);

                  SetStretchBltMode(hdc, HALFTONE);
                  StretchBlt(hdc, 0, 0, width, height,
                    memDC, 0, 0, width, height, SRCCOPY);

                  DeleteDC(memDC);
                  DeleteObject(hbmp);
                }
              }

              delete data;
            }

            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
