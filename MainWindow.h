#pragma once
#include <Windows.h>
class MainWindow
{
public:
    int APIENTRY handleWinMain(_In_ HINSTANCE hInstance,
        _In_opt_ HINSTANCE hPrevInstance,
        _In_ LPWSTR    lpCmdLine,
        _In_ int       nCmdShow);
    ATOM myRegisterClass(HINSTANCE hInstance);
    BOOL initInstance(HINSTANCE hInstance, int nCmdShow);
    int runMe(HWND parentHwnd);
};