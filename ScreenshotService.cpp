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
                int height = abs(_endPoint.y - _startPoint.y);
                
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
            HBITMAP initialScreenshot = CaptureAreaToHBitmap(area);
            screenshots.push_back(initialScreenshot);
            
            // Start time for 5-second capture
            auto startTime = std::chrono::steady_clock::now();
            auto endTime = startTime + std::chrono::seconds(5);
            
            // Find window to scroll - use the center of the selected area
            POINT pt;
            pt.x = area.left + area.width / 2;
            pt.y = area.top + area.height / 2;
            
            // Get the best scrollable window at that point
            HWND targetWindow = FindScrollableWindow(pt);
            
            if (targetWindow) {
                // Main capture loop
                int captureCount = 0;
                int similarFrames = 0;  // Count of consecutive similar frames
                const int MAX_SIMILAR_FRAMES = 3;  // Stop after this many similar frames
                
                while (std::chrono::steady_clock::now() < endTime && similarFrames < MAX_SIMILAR_FRAMES) {
                    // Try multiple approaches to scrolling
                    
                    // Approach 1: Direct message to the window
                    SendMessage(targetWindow, WM_MOUSEWHEEL, MAKEWPARAM(0, -WHEEL_DELTA), MAKELPARAM(pt.x, pt.y));
                    
                    // Approach 2: Try posting the message
                    PostMessage(targetWindow, WM_MOUSEWHEEL, MAKEWPARAM(0, -WHEEL_DELTA), MAKELPARAM(pt.x, pt.y));
                    
                    // Approach 3: SendInput for more reliable scrolling
                    // First, bring the window to the foreground
                    SetForegroundWindow(targetWindow);
                    
                    // Move mouse to the center of the target area
                    SetCursorPos(pt.x, pt.y);
                    
                    // Wait a bit to ensure the cursor has moved
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    
                    // Simulate a mouse wheel scroll
                    INPUT input = {0};
                    input.type = INPUT_MOUSE;
                    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
                    input.mi.mouseData = -WHEEL_DELTA;
                    SendInput(1, &input, sizeof(INPUT));
                    
                    // Wait for scroll animation to finish
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    
                    // Capture another screenshot
                    HBITMAP newScreenshot = CaptureAreaToHBitmap(area);
                    
                    // Compare with the previous screenshot to see if scrolling is still happening
                    if (screenshots.size() > 0 && AreBitmapsSimilar(screenshots.back(), newScreenshot)) {
                        // Screenshots are too similar - scrolling may have stopped
                        similarFrames++;
                        OutputDebugString(L"Similar frame detected\n");
                        
                        // Delete the duplicate screenshot
                        DeleteObject(newScreenshot);
                    } else {
                        // Screenshots are different - scrolling is still happening
                        screenshots.push_back(newScreenshot);
                        captureCount++;
                        similarFrames = 0; // Reset the similar frame counter
                        OutputDebugString(L"New content detected - continuing to scroll\n");
                    }
                }
                
                if (screenshots.size() > 1) {
                    // Combine all screenshots vertically
                    wchar_t buffer[256];
                    swprintf_s(buffer, L"Combining %d screenshots\n", (int)screenshots.size());
                    OutputDebugString(buffer);
                    
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
                    // If we didn't scroll successfully, use the single screenshot
                    OutputDebugString(L"No scrolling detected - using single screenshot\n");
                    
                    // Save the first screenshot to clipboard
                    bool success = SaveToClipboard(screenshots[0]);
                    
                    // Clean up screenshots
                    for (auto& bmp : screenshots) {
                        DeleteObject(bmp);
                    }
                    
                    // Restore main window
                    ::ShowWindow(_mainWindow, SW_RESTORE);
                    
                    // Notify about result
                    if (_callback) {
                        _callback->OnScreenshotCaptured(success);
                    }
                }
            } else {
                OutputDebugString(L"Could not find window to scroll\n");
                
                // If we couldn't find a window to scroll, use the first screenshot
                if (!screenshots.empty()) {
                    // Save the first screenshot to clipboard
                    bool success = SaveToClipboard(screenshots[0]);
                    
                    // Clean up screenshots
                    for (auto& bmp : screenshots) {
                        DeleteObject(bmp);
                    }
                    
                    // Notify about result (partial success - we got one screenshot at least)
                    if (_callback) {
                        _callback->OnScreenshotCaptured(success);
                    }
                } else {
                    // No screenshots at all
                    if (_callback) {
                        _callback->OnScreenshotCaptured(false);
                    }
                }
                
                // Restore main window
                ::ShowWindow(_mainWindow, SW_RESTORE);
            }
        } catch (const std::exception& e) {
            // Log the exception
            char exceptionBuf[1024];
            sprintf_s(exceptionBuf, "Exception during scrolling screenshot: %s\n", e.what());
            OutputDebugStringA(exceptionBuf);
            
            // Clean up screenshots
            for (auto& bmp : screenshots) {
                DeleteObject(bmp);
            }
            
            // Restore main window
            ::ShowWindow(_mainWindow, SW_RESTORE);
            
            if (_callback) {
                _callback->OnScreenshotCaptured(false);
            }
        } catch (...) {
            OutputDebugString(L"Unknown exception during scrolling screenshot\n");
            
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
        
        // Calculate total height and find largest width
        for (auto& hBitmap : bitmaps) {
            BITMAP bInfo;
            GetObject(hBitmap, sizeof(BITMAP), &bInfo);
            totalHeight += bInfo.bmHeight;
            
            // Use the largest width we find
            if (bInfo.bmWidth > width) {
                width = bInfo.bmWidth;
            }
        }
        
        // Debug output
        wchar_t buffer[256];
        swprintf_s(buffer, L"Creating combined bitmap with dimensions: %dx%d\n", width, totalHeight);
        OutputDebugString(buffer);
        
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
        for (size_t i = 0; i < bitmaps.size(); i++) {
            BITMAP bInfo;
            GetObject(bitmaps[i], sizeof(BITMAP), &bInfo);
            
            HDC hdcBitmap = CreateCompatibleDC(hdcScreen);
            HGDIOBJ hOldBmp = SelectObject(hdcBitmap, bitmaps[i]);
            
            // Copy bitmap centered horizontally if widths differ
            int xOffset = (width - bInfo.bmWidth) / 2;
            if (xOffset < 0) xOffset = 0;
            
            // Use StretchBlt if bitmaps are different sizes
            if (bInfo.bmWidth != width) {
                // Scale while maintaining aspect ratio
                StretchBlt(
                    hdcMem, xOffset, yPos, bInfo.bmWidth, bInfo.bmHeight,
                    hdcBitmap, 0, 0, bInfo.bmWidth, bInfo.bmHeight, 
                    SRCCOPY
                );
            } else {
                // Use regular BitBlt if no scaling needed
                BitBlt(
                    hdcMem, xOffset, yPos, bInfo.bmWidth, bInfo.bmHeight,
                    hdcBitmap, 0, 0, SRCCOPY
                );
            }
            
            // Add a subtle separator line between screenshots (except for the last one)
            if (i < bitmaps.size() - 1) {
                HPEN separatorPen = CreatePen(PS_DOT, 1, RGB(200, 200, 200));
                HGDIOBJ oldPen = SelectObject(hdcMem, separatorPen);
                
                MoveToEx(hdcMem, 0, yPos + bInfo.bmHeight - 1, NULL);
                LineTo(hdcMem, width, yPos + bInfo.bmHeight - 1);
                
                SelectObject(hdcMem, oldPen);
                DeleteObject(separatorPen);
            }
            
            // Clean up
            SelectObject(hdcBitmap, hOldBmp);
            DeleteDC(hdcBitmap);
            
            // Move down for the next bitmap
            yPos += bInfo.bmHeight;
        }
        
        // Clean up
        SelectObject(hdcMem, hOldBitmap);
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        
        return hCombined;
    }
    
    // Helper function to find a scrollable window under a point
    HWND FindScrollableWindow(POINT pt) {
        // Get the window at the point
        HWND hwnd = WindowFromPoint(pt);
        
        // Debug the window we found
        wchar_t className[256] = {0};
        wchar_t windowTitle[256] = {0};
        
        if (hwnd) {
            GetClassName(hwnd, className, 256);
            GetWindowText(hwnd, windowTitle, 256);
            
            wchar_t buffer[512];
            swprintf_s(buffer, L"Window at (%d,%d): HWND=0x%p, Class='%s', Title='%s'\n", 
                      pt.x, pt.y, hwnd, className, windowTitle);
            OutputDebugString(buffer);
        } else {
            OutputDebugString(L"No window found at the specified point\n");
        }
        
        // Try to find a more appropriate scrollable window by walking up the parent chain
        HWND scrollableWindow = hwnd;
        int maxAttempts = 5; // Prevent infinite loops
        
        while (scrollableWindow && maxAttempts > 0) {
            // Check window style to see if it has a vertical scrollbar
            LONG style = GetWindowLong(scrollableWindow, GWL_STYLE);
            if (style & WS_VSCROLL) {
                // Found a window with a vertical scrollbar
                GetClassName(scrollableWindow, className, 256);
                GetWindowText(scrollableWindow, windowTitle, 256);
                
                wchar_t buffer[512];
                swprintf_s(buffer, L"Found scrollable window: HWND=0x%p, Class='%s', Title='%s'\n", 
                          scrollableWindow, className, windowTitle);
                OutputDebugString(buffer);
                return scrollableWindow;
            }
            
            // Check some common scrollable control classes
            GetClassName(scrollableWindow, className, 256);
            if (wcscmp(className, L"ScrollBar") == 0 ||
                wcscmp(className, L"SCROLLBAR") == 0 ||
                wcscmp(className, L"ListBox") == 0 ||
                wcscmp(className, L"LISTBOX") == 0 ||
                wcscmp(className, L"Edit") == 0 ||
                wcscmp(className, L"EDIT") == 0 ||
                wcscmp(className, L"RichEdit") == 0 ||
                wcscmp(className, L"RICHEDIT") == 0 ||
                wcscmp(className, L"SysListView32") == 0 ||
                wcscmp(className, L"WebViewHost") == 0 ||
                wcscmp(className, L"Chrome_RenderWidgetHostHWND") == 0) {
                // These are commonly scrollable controls
                return scrollableWindow;
            }
            
            // Try parent window
            HWND parentWindow = GetParent(scrollableWindow);
            if (parentWindow == scrollableWindow || parentWindow == NULL) {
                // No more parents or parent is same as current (shouldn't happen)
                break;
            }
            
            scrollableWindow = parentWindow;
            maxAttempts--;
        }
        
        // If we couldn't find a better window, return the original one
        return hwnd;
    }

    // Helper function to compare two bitmaps and check if they're similar (indicating scrolling has stopped)
    bool AreBitmapsSimilar(HBITMAP bmp1, HBITMAP bmp2) {
        if (!bmp1 || !bmp2)
            return false;
            
        // Get bitmap info
        BITMAP bm1 = {0}, bm2 = {0};
        GetObject(bmp1, sizeof(BITMAP), &bm1);
        GetObject(bmp2, sizeof(BITMAP), &bm2);
        
        // If sizes differ significantly, they're not similar
        if (bm1.bmWidth != bm2.bmWidth || abs(bm1.bmHeight - bm2.bmHeight) > 5)
            return false;
            
        // Create DCs for comparison
        HDC hdcScreen = GetDC(NULL);
        HDC hdc1 = CreateCompatibleDC(hdcScreen);
        HDC hdc2 = CreateCompatibleDC(hdcScreen);
        
        HGDIOBJ oldBmp1 = SelectObject(hdc1, bmp1);
        HGDIOBJ oldBmp2 = SelectObject(hdc2, bmp2);
        
        // We'll sample a few rows of pixels for comparison
        const int sampleRows = 5;
        const int rowHeight = bm1.bmHeight / (sampleRows + 1);
        
        int matchingPixels = 0;
        int totalPixels = 0;
        
        // Sample pixels at specific rows
        for (int row = 1; row <= sampleRows; row++) {
            int y = row * rowHeight;
            
            // Sample pixels across this row
            for (int x = 0; x < bm1.bmWidth; x += 10) { // Sample every 10th pixel
                COLORREF color1 = GetPixel(hdc1, x, y);
                COLORREF color2 = GetPixel(hdc2, x, y);
                
                // Count as matching if colors are close enough
                if (abs(GetRValue(color1) - GetRValue(color2)) < 10 &&
                    abs(GetGValue(color1) - GetGValue(color2)) < 10 &&
                    abs(GetBValue(color1) - GetBValue(color2)) < 10) {
                    matchingPixels++;
                }
                
                totalPixels++;
            }
        }
        
        // Clean up
        SelectObject(hdc1, oldBmp1);
        SelectObject(hdc2, oldBmp2);
        DeleteDC(hdc1);
        DeleteDC(hdc2);
        ReleaseDC(NULL, hdcScreen);
        
        // Calculate similarity percentage
        float similarityPercent = (float)matchingPixels / totalPixels * 100;
        
        // Log the similarity for debugging
        wchar_t buffer[256];
        swprintf_s(buffer, L"Bitmap similarity: %.1f%%\n", similarityPercent);
        OutputDebugString(buffer);
        
        // Consider similar if more than 95% of pixels match
        return similarityPercent > 95;
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