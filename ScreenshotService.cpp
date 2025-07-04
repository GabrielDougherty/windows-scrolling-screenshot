#include "ScreenshotService.h"
#include <thread>
#include <chrono>

// Implementation of WindowManager interface
class WindowManager : public IWindowManager {
public:
    WindowManager() = default;

    HWND GetMainWindow() const override {
        return _mainWindow;
    }

    void SetMainWindow(HWND hwnd) {
        _mainWindow = hwnd;
    }

    void MinimizeWindow(HWND hwnd) override {
        ::ShowWindow(hwnd, SW_MINIMIZE);
    }

    void RestoreWindow(HWND hwnd) override {
        ::ShowWindow(hwnd, SW_RESTORE);
    }

    ATOM RegisterWindowClass(HINSTANCE hInstance, LPCWSTR className, WNDPROC wndProc) override {
        // Check if the class is already registered
        WNDCLASSEX wcTemp;
        if (GetClassInfoEx(hInstance, className, &wcTemp)) {
            // Class already registered, return its atom
            return (ATOM)wcTemp.style;
        }
        
        // Set up class properties
        WNDCLASSEX wcex = {};
        wcex.cbSize = sizeof(WNDCLASSEX);
        wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS; // Added CS_DBLCLKS to handle double clicks
        wcex.lpfnWndProc = wndProc;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = 0;
        wcex.hInstance = hInstance;
        wcex.hIcon = NULL;
        wcex.hCursor = LoadCursor(nullptr, IDC_CROSS); // Use crosshair cursor by default
        wcex.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH); // Black background
        wcex.lpszMenuName = NULL;
        wcex.lpszClassName = className;
        wcex.hIconSm = NULL;
        
        // Register the window class
        return RegisterClassEx(&wcex);
    }

    HWND CreateOverlayWindow(HINSTANCE hInstance, LPCWSTR className) override {
        // Get the screen dimensions
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        
        // CRITICAL FIX: Make sure window style flags are optimal for an overlay
        // We need WS_EX_LAYERED for transparency, WS_EX_TOPMOST to stay on top
        HWND hwnd = CreateWindowEx(
            WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW, // Added WS_EX_TOOLWINDOW to avoid taskbar icon
            className,
            L"Screenshot Overlay",
            WS_POPUP | WS_VISIBLE, // WS_VISIBLE is crucial to show the window
            0, 0, screenWidth, screenHeight, // Full screen size
            NULL, NULL, hInstance, NULL
        );
        
        if (hwnd) {
            // Set the window to be more visible with better alpha (128/255)
            // LWA_ALPHA makes the whole window semi-transparent
            SetLayeredWindowAttributes(hwnd, 0, 128, LWA_ALPHA);
            
            // Set the cursor to crosshair for better precision when selecting
            HCURSOR crossCursor = LoadCursor(NULL, IDC_CROSS);
            SetCursor(crossCursor);
            SetClassLongPtr(hwnd, GCLP_HCURSOR, (LONG_PTR)crossCursor);
            
            // Make sure this window gets focus and is the active window
            SetForegroundWindow(hwnd);
            SetFocus(hwnd);
            SetActiveWindow(hwnd);
            
            // Debug output
            OutputDebugString(L"Overlay window created with dimensions: ");
            wchar_t buf[100];
            swprintf_s(buf, L"%dx%d\n", screenWidth, screenHeight);
            OutputDebugString(buf);
        } else {
            // Debug output if window creation failed
            DWORD error = GetLastError();
            OutputDebugString(L"Failed to create overlay window. Error code: ");
            wchar_t buf[100];
            swprintf_s(buf, L"%d\n", error);
            OutputDebugString(buf);
        }
        
        return hwnd;
    }

    void ShowWindow(HWND hwnd, int nCmdShow) override {
        ::ShowWindow(hwnd, nCmdShow);
    }

    void UpdateWindow(HWND hwnd) override {
        ::UpdateWindow(hwnd);
    }

    void SetForegroundWindow(HWND hwnd) override {
        ::SetForegroundWindow(hwnd);
    }

    void SetActiveWindow(HWND hwnd) override {
        ::SetActiveWindow(hwnd);
    }

    void DestroyWindow(HWND hwnd) override {
        ::DestroyWindow(hwnd);
    }

private:
    HWND _mainWindow = NULL;
};

// Implementation of ScreenshotCapture interface
class ScreenshotCapture : public IScreenshotCapture {
public:
    bool CaptureArea(const ScreenshotArea& area) override {
        // Create compatible DC, bitmap and other objects needed for the screenshot
        HDC hdcScreen = GetDC(NULL);
        HDC hdcMemDC = CreateCompatibleDC(hdcScreen);
        
        // Create a bitmap that matches the selected area's size
        HBITMAP hbmScreen = CreateCompatibleBitmap(hdcScreen, area.width, area.height);
        HGDIOBJ hOldObject = SelectObject(hdcMemDC, hbmScreen);
        
        // Copy screen to bitmap - this is where the actual screenshot happens
        BitBlt(hdcMemDC, 0, 0, area.width, area.height, 
               hdcScreen, area.left, area.top, SRCCOPY);
        
        // Save the screenshot to clipboard
        bool result = SaveToClipboard(hbmScreen);
        
        // Clean up
        SelectObject(hdcMemDC, hOldObject);
        DeleteDC(hdcMemDC);
        ReleaseDC(NULL, hdcScreen);
        
        return result;
    }

    bool SaveToClipboard(HBITMAP hBitmap) override {
        if (OpenClipboard(NULL)) {
            EmptyClipboard();
            SetClipboardData(CF_BITMAP, hBitmap);
            CloseClipboard();
            return true;
        }
        return false;
    }

    void ShowNotification(const std::wstring& message) override {
        MessageBox(NULL, message.c_str(), L"Screenshot", MB_OK | MB_ICONINFORMATION);
    }
};

// Implementation of OverlayHandler interface
class OverlayHandler : public IOverlayHandler {
public:
    void StartSelection(int x, int y) override {
        _isSelecting = true;
        _startPoint = { x, y };
        _endPoint = _startPoint;
    }

    void UpdateSelection(int x, int y) override {
        if (_isSelecting) {
            _endPoint = { x, y };
        }
    }

    void EndSelection(int x, int y) override {
        if (!_isSelecting) return;
        
        _isSelecting = false;
        _endPoint = { x, y };
        
        // Calculate the selection area
        int left = min(_startPoint.x, _endPoint.x);
        int top = min(_startPoint.y, _endPoint.y);
        int width = abs(_endPoint.x - _startPoint.x);
        int height = abs(_endPoint.y - _startPoint.y);
        
        // Ensure minimum size
        if (width > 5 && height > 5) {
            _currentSelection = ScreenshotArea{ left, top, width, height };
        } else {
            _currentSelection = std::nullopt;
        }
    }

    void CancelSelection() override {
        _isSelecting = false;
        _currentSelection = std::nullopt;
    }

    void DrawSelection(HDC hdc, const RECT& clientRect) override {
        // Fill the entire window with a semi-transparent dark overlay
        // Use a slightly more opaque background for better visibility
        HBRUSH backgroundBrush = CreateSolidBrush(RGB(30, 30, 30));
        FillRect(hdc, &clientRect, backgroundBrush);
        DeleteObject(backgroundBrush);
        
        // If not selecting, draw instructions
        if (!_isSelecting) {
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            
            WCHAR instructionText[] = L"Click and drag to select an area for screenshot. Press ESC to cancel.";
            RECT textRect = clientRect;
            textRect.top = clientRect.bottom / 2 - 20;
            DrawText(hdc, instructionText, -1, &textRect, DT_CENTER);
        }
        
        // Draw the selection rectangle if selecting
        if (_isSelecting) {
            // Calculate the selection rectangle
            RECT selectionRect = {
                min(_startPoint.x, _endPoint.x),
                min(_startPoint.y, _endPoint.y),
                max(_startPoint.x, _endPoint.x),
                max(_startPoint.y, _endPoint.y)
            };
            
            // First save the current DC state
            int savedDC = SaveDC(hdc);
            
            // Draw a brighter "cutout" for the selected area
            HBRUSH cutoutBrush = CreateSolidBrush(RGB(255, 255, 255));
            SelectObject(hdc, cutoutBrush);
            
            // Use BitBlt with DSTINVERT to create a cutout effect
            BitBlt(hdc, selectionRect.left, selectionRect.top,
                selectionRect.right - selectionRect.left,
                selectionRect.bottom - selectionRect.top,
                NULL, 0, 0, PATINVERT);
                
            DeleteObject(cutoutBrush);
            
            // Restore the DC state
            RestoreDC(hdc, savedDC);
            
            // Draw a bright border around selection for better visibility
            // Use a 2-pixel wide pen for better visibility
            HPEN borderPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 0)); // Bright yellow
            HGDIOBJ oldPen = SelectObject(hdc, borderPen);
            
            // Draw outer rectangle
            MoveToEx(hdc, selectionRect.left, selectionRect.top, NULL);
            LineTo(hdc, selectionRect.right, selectionRect.top);
            LineTo(hdc, selectionRect.right, selectionRect.bottom);
            LineTo(hdc, selectionRect.left, selectionRect.bottom);
            LineTo(hdc, selectionRect.left, selectionRect.top);
            
            // Select the old pen back and delete the border pen
            SelectObject(hdc, oldPen);
            DeleteObject(borderPen);
            
            // Draw dimension info
            WCHAR dimensionText[50];
            swprintf_s(dimensionText, L"%dx%d", 
                selectionRect.right - selectionRect.left,
                selectionRect.bottom - selectionRect.top);
            
            // Set up the text to be visible
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 0)); // Bright yellow
            
            // Draw dimension text in a position that won't be cut off at screen edges
            int textX = selectionRect.right + 10;
            int textY = selectionRect.bottom + 10;
            
            // Adjust if too close to screen edge
            if (textX + 100 > clientRect.right) textX = selectionRect.left - 100;
            if (textY + 20 > clientRect.bottom) textY = selectionRect.top - 20;
            
            TextOut(hdc, textX, textY, dimensionText, (int)wcslen(dimensionText));
        }
    }

    std::optional<ScreenshotArea> GetCurrentSelection() const override {
        return _currentSelection;
    }

private:
    bool _isSelecting = false;
    POINT _startPoint = { 0, 0 };
    POINT _endPoint = { 0, 0 };
    std::optional<ScreenshotArea> _currentSelection;
};

// Main screenshot service implementation
class ScreenshotService : public IScreenshotService {
public:
    ScreenshotService(
        std::shared_ptr<IWindowManager> windowManager,
        std::shared_ptr<IScreenshotCapture> screenshotCapture,
        std::shared_ptr<IOverlayHandler> overlayHandler,
        HINSTANCE hInstance
    ) : 
        _windowManager(windowManager),
        _screenshotCapture(screenshotCapture),
        _overlayHandler(overlayHandler),
        _hInstance(hInstance),
        _overlayWnd(NULL)
    {}

    void StartScreenshotProcess() override {
        // First minimize the main window
        HWND mainWindow = _windowManager->GetMainWindow();
        
        // Use direct ShowWindow with SW_MINIMIZE for reliable minimization
        _windowManager->MinimizeWindow(mainWindow);
        
        // Make sure any existing overlay is cleaned up first
        if (_overlayWnd != NULL) {
            _windowManager->DestroyWindow(_overlayWnd);
            _overlayWnd = NULL;
        }
        
        // CRITICAL FIX: Use a synchronous approach instead of a thread for reliable overlay creation
        // Wait for the window to minimize with a longer delay for better reliability
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        
        // Register the overlay window class
        static const wchar_t* overlayClassName = L"ScreenshotOverlayClass";
        _windowManager->RegisterWindowClass(_hInstance, overlayClassName, OverlayWndProc);
        
        // Create the overlay window
        _overlayWnd = _windowManager->CreateOverlayWindow(_hInstance, overlayClassName);
        
        if (_overlayWnd) {
            // Store the service instance in the window's user data for the static window proc
            SetWindowLongPtr(_overlayWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
            
            // Ensure the overlay window is visible and has focus
            _windowManager->ShowWindow(_overlayWnd, SW_SHOW);
            _windowManager->UpdateWindow(_overlayWnd);
            _windowManager->SetForegroundWindow(_overlayWnd);
            _windowManager->SetActiveWindow(_overlayWnd);
            SetFocus(_overlayWnd);
            
            // Set cursor to crosshair for better selection precision
            SetCursor(LoadCursor(NULL, IDC_CROSS));
            
            // Force a message pump to ensure the window processes messages
            MSG msg;
            PeekMessage(&msg, _overlayWnd, 0, 0, PM_REMOVE);
            
            // Debug output
            OutputDebugString(L"Screenshot overlay window created and displayed\n");
        } else {
            // Failed to create overlay, restore the main window
            _windowManager->RestoreWindow(_windowManager->GetMainWindow());
            OutputDebugString(L"Failed to create screenshot overlay window\n");
        }
    }

    void CaptureSelectedArea(const ScreenshotArea& area) override {
        OutputDebugString(L"Starting CaptureSelectedArea\n");
        
        // First hide the overlay window so it doesn't appear in the screenshot
        if (_overlayWnd) {
            // Hide the window
            _windowManager->ShowWindow(_overlayWnd, SW_HIDE);
            
            // Process any pending messages to ensure it's actually hidden
            MSG msg;
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            
            // Allow time for the window to be hidden
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        
        // Debug output the area being captured
        wchar_t areaBuf[100];
        swprintf_s(areaBuf, L"Capturing area: left=%d, top=%d, width=%d, height=%d\n", 
                  area.left, area.top, area.width, area.height);
        OutputDebugString(areaBuf);
        
        // Take the screenshot
        bool success = false;
        try {
            // Capture the screenshot
            success = _screenshotCapture->CaptureArea(area);
            OutputDebugString(success ? L"Screenshot captured successfully\n" : L"Screenshot capture failed\n");
        } catch (const std::exception& e) {
            char buf[512];
            sprintf_s(buf, "Exception during screenshot capture: %s\n", e.what());
            OutputDebugStringA(buf);
            success = false;
        } catch (...) {
            OutputDebugString(L"Unknown exception during screenshot capture\n");
            success = false;
        }
        
        // Clean up the overlay window
        if (_overlayWnd) {
            _windowManager->DestroyWindow(_overlayWnd);
            _overlayWnd = NULL;
            OutputDebugString(L"Overlay window destroyed\n");
        }
        
        // Restore the main window
        _windowManager->RestoreWindow(_windowManager->GetMainWindow());
        OutputDebugString(L"Main window restored\n");
        
        // Allow time for the window to restore
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // Show notification about the result
        if (success) {
            _screenshotCapture->ShowNotification(L"Screenshot captured and saved to clipboard");
            OutputDebugString(L"Showing success notification\n");
        } else {
            _screenshotCapture->ShowNotification(L"Failed to capture screenshot");
            OutputDebugString(L"Showing failure notification\n");
        }
        
        // Notify callback
        if (_callback) {
            OutputDebugString(L"Calling screenshot callback\n");
            _callback->OnScreenshotCaptured(success);
        }
    }

    void SetScreenshotCallback(std::shared_ptr<IScreenshotCallback> callback) override {
        _callback = callback;
    }

    // Static window procedure for the overlay window
    static LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
        // Get the service instance from the window's user data
        ScreenshotService* service = reinterpret_cast<ScreenshotService*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
        
        if (service) {
            return service->HandleOverlayWindowMessage(hWnd, message, wParam, lParam);
        }
        
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    LRESULT HandleOverlayWindowMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) override {
        switch (message) {
        case WM_CREATE:
            // Set the cursor to crosshair immediately when window is created
            SetCursor(LoadCursor(NULL, IDC_CROSS));
            return 0;
            
        case WM_SETCURSOR:
            // Always use crosshair cursor
            SetCursor(LoadCursor(NULL, IDC_CROSS));
            return TRUE;
            
        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE) {
                // If our window is becoming inactive, bring it back to front
                _windowManager->SetForegroundWindow(hWnd);
                _windowManager->SetActiveWindow(hWnd);
                SetFocus(hWnd);
                return 0;
            }
            break;
            
        case WM_LBUTTONDOWN:
            // Start the selection process and capture the mouse
            OutputDebugString(L"WM_LBUTTONDOWN received\n");
            _overlayHandler->StartSelection(LOWORD(lParam), HIWORD(lParam));
            SetCapture(hWnd); // Capture mouse input so we get moves outside window
            return 0;
            
        case WM_MOUSEMOVE:
            // Update selection and force redraw to show the current selection rectangle
            if (GetCapture() == hWnd) {
                _overlayHandler->UpdateSelection(LOWORD(lParam), HIWORD(lParam));
                InvalidateRect(hWnd, NULL, FALSE); // FALSE = don't erase background for smoother drawing
            }
            return 0;
            
        case WM_LBUTTONUP:
            // Complete the selection process
            if (GetCapture() == hWnd) {
                OutputDebugString(L"WM_LBUTTONUP received - ending selection\n");
                
                // Release the mouse capture
                ReleaseCapture();
                
                // End the selection and get selection area if valid
                _overlayHandler->EndSelection(LOWORD(lParam), HIWORD(lParam));
                std::optional<ScreenshotArea> selection = _overlayHandler->GetCurrentSelection();
                
                if (selection) {
                    OutputDebugString(L"Valid selection - capturing screenshot\n");
                    // Valid selection - capture the screenshot
                    CaptureSelectedArea(*selection);
                } else {
                    OutputDebugString(L"Invalid selection - cancelling\n");
                    // Invalid selection (too small) - cancel and restore
                    if (_overlayWnd) {
                        _windowManager->DestroyWindow(_overlayWnd);
                        _overlayWnd = NULL;
                    }
                    
                    // Restore main window
                    _windowManager->RestoreWindow(_windowManager->GetMainWindow());
                    
                    // Notify about cancellation
                    if (_callback) {
                        _callback->OnSelectionCancelled();
                    }
                }
                return 0;
            }
            break;
            
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            
            // Get the client rectangle
            RECT clientRect;
            GetClientRect(hWnd, &clientRect);
            
            // Draw the selection using the handler
            _overlayHandler->DrawSelection(hdc, clientRect);
            
            EndPaint(hWnd, &ps);
            return 0;
        }
            
        case WM_KEYDOWN:
            // Allow ESC to cancel the selection
            if (wParam == VK_ESCAPE) {
                OutputDebugString(L"ESC pressed - cancelling selection\n");
                _overlayHandler->CancelSelection();
                
                if (_overlayWnd) {
                    _windowManager->DestroyWindow(_overlayWnd);
                    _overlayWnd = NULL;
                }
                
                // Restore the main window
                _windowManager->RestoreWindow(_windowManager->GetMainWindow());
                
                // Notify callback
                if (_callback) {
                    _callback->OnSelectionCancelled();
                }
                return 0;
            }
            break;
            
        case WM_DESTROY:
            _overlayWnd = NULL;
            return 0;
        }
        
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

private:
    std::shared_ptr<IWindowManager> _windowManager;
    std::shared_ptr<IScreenshotCapture> _screenshotCapture;
    std::shared_ptr<IOverlayHandler> _overlayHandler;
    std::shared_ptr<IScreenshotCallback> _callback;
    HINSTANCE _hInstance;
    HWND _overlayWnd;
};

// Factory function to create the screenshot service with all dependencies
std::shared_ptr<IScreenshotService> CreateScreenshotService(HWND mainWindow, HINSTANCE hInstance) {
    auto windowManager = std::make_shared<WindowManager>();
    windowManager->SetMainWindow(mainWindow);
    
    auto screenshotCapture = std::make_shared<ScreenshotCapture>();
    auto overlayHandler = std::make_shared<OverlayHandler>();
    
    return std::make_shared<ScreenshotService>(
        windowManager,
        screenshotCapture,
        overlayHandler,
        hInstance
    );
}