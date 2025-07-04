#include "ScreenshotService.h"
#include <thread>
#include <chrono>
#include <vector>
#include <algorithm>

// Implementation of the ScreenshotService
class ScreenshotServiceImpl : public ScreenshotService {
public:
    ScreenshotServiceImpl(HWND mainWindow, HINSTANCE hInstance)
        : _mainWindow(mainWindow), _hInstance(hInstance), _overlayWnd(nullptr)
    {}

    ~ScreenshotServiceImpl() {
        // Clean up overlay window if it exists
        if (_overlayWnd) {
            ::DestroyWindow(_overlayWnd);
            _overlayWnd = nullptr;
        }
    }

    void StartScreenshotProcess() override {
        // First minimize the main window
        ::ShowWindow(_mainWindow, SW_MINIMIZE);
        
        // Clean up any existing overlay
        if (_overlayWnd) {
            ::DestroyWindow(_overlayWnd);
            _overlayWnd = nullptr;
        }
        
        // Wait for the window to minimize
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        
        // Register window class for overlay
        RegisterOverlayClass();
        
        // Create overlay window
        _overlayWnd = CreateOverlayWindow();
        
        if (_overlayWnd) {
            // Store this instance in window's user data for the static proc
            SetWindowLongPtr(_overlayWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
            
            // Show and focus the overlay window
            ::ShowWindow(_overlayWnd, SW_SHOW);
            ::UpdateWindow(_overlayWnd);
            ::SetForegroundWindow(_overlayWnd);
            ::SetFocus(_overlayWnd);
            
            OutputDebugString(L"Screenshot overlay created and shown\n");
        } else {
            OutputDebugString(L"Failed to create screenshot overlay\n");
            ::ShowWindow(_mainWindow, SW_RESTORE);
        }
    }
    
    void SetScreenshotCallback(std::shared_ptr<ScreenshotCallback> callback) override {
        _callback = callback;
    }
    
    LRESULT HandleOverlayWindowMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) override {
        switch (message) {
        case WM_CREATE:
            // Set crosshair cursor
            SetCursor(LoadCursor(NULL, IDC_CROSS));
            return 0;
            
        case WM_SETCURSOR:
            // Always use crosshair cursor
            SetCursor(LoadCursor(NULL, IDC_CROSS));
            return TRUE;
            
        case WM_LBUTTONDOWN:
            // Start selection
            _isSelecting = true;
            _startPoint = { LOWORD(lParam), HIWORD(lParam) };
            _endPoint = _startPoint;
            SetCapture(hWnd);
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
            
        case WM_MOUSEMOVE:
            if (GetCapture() == hWnd) {
                // Update selection
                _endPoint = { LOWORD(lParam), HIWORD(lParam) };
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;
            
        case WM_LBUTTONUP:
            if (GetCapture() == hWnd) {
                // End selection
                ReleaseCapture();
                _endPoint = { LOWORD(lParam), HIWORD(lParam) };
                _isSelecting = false;
                
                // Calculate selection area
                int left = min(_startPoint.x, _endPoint.x);
                int top = min(_startPoint.y, _endPoint.y);
                int width = abs(_endPoint.x - _startPoint.x);
                int height = abs(_endPoint.y - _endPoint.y);
                
                // Check if selection is large enough
                if (width > 10 && height > 10) {
                    // Hide overlay before capturing
                    ::ShowWindow(_overlayWnd, SW_HIDE);
                    
                    // Process pending messages
                    MSG msg;
                    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                        TranslateMessage(&msg);
                        DispatchMessage(&msg);
                    }
                    
                    // Wait for overlay to hide
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    
                    // Create selection area
                    ScreenshotArea area = { left, top, width, height };
                    
                    // Take scrolling screenshots
                    CaptureScrollingScreenshot(area);
                } else {
                    // Selection too small, cancel
                    ::DestroyWindow(_overlayWnd);
                    _overlayWnd = nullptr;
                    
                    ::ShowWindow(_mainWindow, SW_RESTORE);
                    
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
            
            // Paint the overlay with selection
            PaintOverlay(hdc);
            
            EndPaint(hWnd, &ps);
            return 0;
        }
            
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                // Cancel on ESC
                ::DestroyWindow(_overlayWnd);
                _overlayWnd = nullptr;
                
                ::ShowWindow(_mainWindow, SW_RESTORE);
                
                if (_callback) {
                    _callback->OnSelectionCancelled();
                }
                return 0;
            }
            break;
            
        case WM_DESTROY:
            _overlayWnd = nullptr;
            return 0;
        }
        
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

private:
    // Register the overlay window class
    void RegisterOverlayClass() {
        WNDCLASSEX wcex = {};
        wcex.cbSize = sizeof(WNDCLASSEX);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = OverlayWndProc;
        wcex.hInstance = _hInstance;
        wcex.hCursor = LoadCursor(NULL, IDC_CROSS);
        wcex.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wcex.lpszClassName = L"ScrollingScreenshotOverlay";
        
        RegisterClassEx(&wcex);
    }
    
    // Create the overlay window
    HWND CreateOverlayWindow() {
        // Get screen dimensions
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        
        // Create a topmost, layered window
        HWND hwnd = CreateWindowEx(
            WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
            L"ScrollingScreenshotOverlay",
            L"Scrolling Screenshot",
            WS_POPUP | WS_VISIBLE,
            0, 0, screenWidth, screenHeight,
            NULL, NULL, _hInstance, NULL
        );
        
        if (hwnd) {
            // Set semi-transparency
            SetLayeredWindowAttributes(hwnd, 0, 128, LWA_ALPHA);
        }
        
        return hwnd;
    }
    
    // Static window procedure that forwards messages to the instance
    static LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
        ScreenshotServiceImpl* service = reinterpret_cast<ScreenshotServiceImpl*>(
            GetWindowLongPtr(hWnd, GWLP_USERDATA));
        
        if (service) {
            return service->HandleOverlayWindowMessage(hWnd, message, wParam, lParam);
        }
        
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    
    // Paint the overlay with selection rectangle
    void PaintOverlay(HDC hdc) {
        // Get client rect
        RECT clientRect;
        GetClientRect(_overlayWnd, &clientRect);
        
        // Fill with semi-transparent dark overlay
        HBRUSH darkBrush = CreateSolidBrush(RGB(30, 30, 30));
        FillRect(hdc, &clientRect, darkBrush);
        DeleteObject(darkBrush);
        
        // Draw instructions if not selecting
        if (!_isSelecting) {
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            
            WCHAR instructionText[] = L"Click and drag to select an area for scrolling screenshot. Press ESC to cancel.";
            RECT textRect = clientRect;
            textRect.top = clientRect.bottom / 2 - 20;
            DrawText(hdc, instructionText, -1, &textRect, DT_CENTER);
        }
        
        // Draw selection rectangle if selecting
        if (_isSelecting) {
            // Calculate selection rect
            RECT selectionRect = {
                min(_startPoint.x, _endPoint.x),
                min(_startPoint.y, _endPoint.y),
                max(_startPoint.x, _endPoint.x),
                max(_startPoint.y, _endPoint.y)
            };
            
            // Draw a bright "cutout" for the selected area
            int savedDC = SaveDC(hdc);
            
            // Draw selection with inverted effect
            BitBlt(hdc, selectionRect.left, selectionRect.top,
                   selectionRect.right - selectionRect.left,
                   selectionRect.bottom - selectionRect.top,
                   NULL, 0, 0, PATINVERT);
            
            RestoreDC(hdc, savedDC);
            
            // Draw bright border
            HPEN yellowPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 0));
            HGDIOBJ oldPen = SelectObject(hdc, yellowPen);
            
            // Draw rectangle border
            MoveToEx(hdc, selectionRect.left, selectionRect.top, NULL);
            LineTo(hdc, selectionRect.right, selectionRect.top);
            LineTo(hdc, selectionRect.right, selectionRect.bottom);
            LineTo(hdc, selectionRect.left, selectionRect.bottom);
            LineTo(hdc, selectionRect.left, selectionRect.top);
            
            SelectObject(hdc, oldPen);
            DeleteObject(yellowPen);
            
            // Draw dimensions
            WCHAR dimText[50];
            swprintf_s(dimText, L"%dx%d", 
                      selectionRect.right - selectionRect.left,
                      selectionRect.bottom - selectionRect.top);
            
            SetTextColor(hdc, RGB(255, 255, 0));
            TextOut(hdc, selectionRect.right + 5, selectionRect.bottom + 5,
                   dimText, (int)wcslen(dimText));
        }
    }
    
    // Capture a series of screenshots while scrolling
    void CaptureScrollingScreenshot(const ScreenshotArea& area) {
        OutputDebugString(L"Starting scrolling screenshot capture\n");
        
        // Vector to store all screenshot bitmaps
        std::vector<HBITMAP> screenshots;
        
        try {
            // Take initial screenshot
            screenshots.push_back(CaptureAreaToHBitmap(area));
            
            // Start time for 5-second capture
            auto startTime = std::chrono::steady_clock::now();
            auto endTime = startTime + std::chrono::seconds(5);
            
            // Find window to scroll
            POINT pt;
            pt.x = area.left + area.width / 2;
            pt.y = area.top + area.height / 2;
            HWND targetWindow = WindowFromPoint(pt);
            
            if (targetWindow) {
                // Main capture loop
                while (std::chrono::steady_clock::now() < endTime) {
                    // Scroll the window down
                    SendMessage(targetWindow, WM_MOUSEWHEEL, MAKEWPARAM(0, -WHEEL_DELTA), 
                               MAKELPARAM(pt.x, pt.y));
                    
                    // Wait for scroll animation
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    
                    // Capture another screenshot
                    screenshots.push_back(CaptureAreaToHBitmap(area));
                }
                
                // Combine all screenshots vertically
                HBITMAP combinedBitmap = CombineVertically(screenshots);
                
                // Save to clipboard
                bool success = SaveToClipboard(combinedBitmap);
                
                // Clean up individual screenshots
                for (auto& bmp : screenshots) {
                    DeleteObject(bmp);
                }
                
                // Restore main window
                ::ShowWindow(_mainWindow, SW_RESTORE);
                
                // Notify about result
                if (_callback) {
                    _callback->OnScreenshotCaptured(success);
                }
            } else {
                OutputDebugString(L"Could not find window to scroll\n");
                ::ShowWindow(_mainWindow, SW_RESTORE);
                
                if (_callback) {
                    _callback->OnScreenshotCaptured(false);
                }
            }
        } catch (...) {
            OutputDebugString(L"Exception during scrolling screenshot\n");
            
            // Clean up screenshots
            for (auto& bmp : screenshots) {
                DeleteObject(bmp);
            }
            
            // Restore main window
            ::ShowWindow(_mainWindow, SW_RESTORE);
            
            if (_callback) {
                _callback->OnScreenshotCaptured(false);
            }
        }
        
        // Clean up overlay
        if (_overlayWnd) {
            ::DestroyWindow(_overlayWnd);
            _overlayWnd = nullptr;
        }
    }
    
    // Capture a screenshot of the specified area
    HBITMAP CaptureAreaToHBitmap(const ScreenshotArea& area) {
        HDC hdcScreen = GetDC(NULL);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        
        HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, area.width, area.height);
        HGDIOBJ hOldBitmap = SelectObject(hdcMem, hBitmap);
        
        BitBlt(hdcMem, 0, 0, area.width, area.height,
               hdcScreen, area.left, area.top, SRCCOPY);
        
        SelectObject(hdcMem, hOldBitmap);
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        
        return hBitmap;
    }
    
    // Save bitmap to clipboard
    bool SaveToClipboard(HBITMAP hBitmap) {
        if (!OpenClipboard(NULL))
            return false;
        
        EmptyClipboard();
        SetClipboardData(CF_BITMAP, hBitmap);
        CloseClipboard();
        
        return true;
    }
    
    // Combine multiple bitmaps vertically into one bitmap
    HBITMAP CombineVertically(const std::vector<HBITMAP>& bitmaps) {
        if (bitmaps.empty())
            return NULL;
            
        // Get dimensions from first bitmap
        BITMAP bmp;
        GetObject(bitmaps[0], sizeof(BITMAP), &bmp);
        
        int width = bmp.bmWidth;
        int totalHeight = 0;
        
        // Calculate total height
        for (auto& hBitmap : bitmaps) {
            BITMAP bInfo;
            GetObject(hBitmap, sizeof(BITMAP), &bInfo);
            totalHeight += bInfo.bmHeight;
        }
        
        // Create DC for combined bitmap
        HDC hdcScreen = GetDC(NULL);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        
        // Create combined bitmap
        HBITMAP hCombined = CreateCompatibleBitmap(hdcScreen, width, totalHeight);
        HGDIOBJ hOldBitmap = SelectObject(hdcMem, hCombined);
        
        // Fill with white background
        RECT rect = { 0, 0, width, totalHeight };
        FillRect(hdcMem, &rect, (HBRUSH)GetStockObject(WHITE_BRUSH));
        
        // Copy each bitmap into combined bitmap
        int yPos = 0;
        for (auto& hBitmap : bitmaps) {
            BITMAP bInfo;
            GetObject(hBitmap, sizeof(BITMAP), &bInfo);
            
            HDC hdcBitmap = CreateCompatibleDC(hdcScreen);
            HGDIOBJ hOldBmp = SelectObject(hdcBitmap, hBitmap);
            
            BitBlt(hdcMem, 0, yPos, bInfo.bmWidth, bInfo.bmHeight,
                  hdcBitmap, 0, 0, SRCCOPY);
            
            SelectObject(hdcBitmap, hOldBmp);
            DeleteDC(hdcBitmap);
            
            yPos += bInfo.bmHeight;
        }
        
        // Clean up
        SelectObject(hdcMem, hOldBitmap);
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        
        return hCombined;
    }
    
private:
    HWND _mainWindow;
    HINSTANCE _hInstance;
    HWND _overlayWnd;
    std::shared_ptr<ScreenshotCallback> _callback;
    
    bool _isSelecting = false;
    POINT _startPoint = { 0, 0 };
    POINT _endPoint = { 0, 0 };
};

// Factory function implementation
std::shared_ptr<ScreenshotService> CreateScreenshotService(HWND mainWindow, HINSTANCE hInstance) {
    return std::make_shared<ScreenshotServiceImpl>(mainWindow, hInstance);
}