// NativeScrollingScreenshot.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "NativeScrollingScreenshot.h"
#include "MainWindow.h"

#include <cstdio>
#include <Windows.h>

#define MAX_LOADSTRING 100

#ifndef PRINTF_OVERRIDE
#define printf printf2
#define PRINTF_OVERRIDE 1
#endif

namespace
{
    int __cdecl printf2(const char* format, ...)
    {
        char str[1024];

        va_list argptr;
        va_start(argptr, format);
        int ret = vsnprintf(str, sizeof(str), format, argptr);
        va_end(argptr);

        OutputDebugStringA(str);

        return ret;
    }
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    MainWindow mw;
    mw.handleWinMain(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
}

