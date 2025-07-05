#include "ImageStitcher.h"
#include <Windows.h>
#include <algorithm> // For std::min

// OpenCV 4 headers
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/xfeatures2d.hpp>

HBITMAP ImageStitcher::StitchImagesWithFeatureMatching(const std::vector<HBITMAP>& bitmaps) {
    if (bitmaps.empty())
        return NULL;
    
    if (bitmaps.size() == 1) {
        // Create a copy of the bitmap
        BITMAP bmp;
        GetObject(bitmaps[0], sizeof(BITMAP), &bmp);
        
        HDC hdcScreen = GetDC(NULL);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, bmp.bmWidth, bmp.bmHeight);
        
        if (hBitmap) {
            HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, bitmaps[0]);
            HDC hdcMemDest = CreateCompatibleDC(hdcScreen);
            HBITMAP hOldBitmapDest = (HBITMAP)SelectObject(hdcMemDest, hBitmap);
            
            BitBlt(hdcMemDest, 0, 0, bmp.bmWidth, bmp.bmHeight, hdcMem, 0, 0, SRCCOPY);
            
            SelectObject(hdcMemDest, hOldBitmapDest);
            SelectObject(hdcMem, hOldBitmap);
            DeleteDC(hdcMemDest);
            DeleteDC(hdcMem);
            ReleaseDC(NULL, hdcScreen);
            
            return hBitmap;
        }
        ReleaseDC(NULL, hdcScreen);
        return NULL;
    }
    
    try {
        // Convert HBITMAPs to OpenCV Mats
        std::vector<cv::Mat> images;
        for (const auto& bitmap : bitmaps) {
            cv::Mat img = HBitmapToMat(bitmap);
            if (!img.empty()) {
                images.push_back(img);
                OutputDebugStringA("ImageStitcher: Successfully converted bitmap to Mat\n");
            } else {
                OutputDebugStringA("ImageStitcher: Failed to convert bitmap to Mat\n");
            }
        }
        
        if (images.empty()) {
            OutputDebugStringA("ImageStitcher: No images to stitch\n");
            return NULL;
        }
        
        char debugBuf[256];
        sprintf_s(debugBuf, "ImageStitcher: Processing %d images for feature matching\n", (int)images.size());
        OutputDebugStringA(debugBuf);
        
        // Result will be a vertically stacked image
        int totalHeight = 0;
        int maxWidth = 0;
        
        // Find total dimensions
        for (const auto& img : images) {
            totalHeight += img.rows;
            if (img.cols > maxWidth)
                maxWidth = img.cols;
        }
        
        // Create result image
        cv::Mat result(totalHeight, maxWidth, CV_8UC4, cv::Scalar(255, 255, 255, 255));
        
        // First image is the reference
        cv::Mat previousImage = images[0];
        int yOffset = 0;
        
        // Copy first image to the result
        cv::Mat roi = result(cv::Rect(0, 0, previousImage.cols, previousImage.rows));
        previousImage.copyTo(roi);
        yOffset += previousImage.rows;
        
        // For each subsequent image
        for (size_t i = 1; i < images.size(); i++) {
            cv::Mat currentImage = images[i];
            
            // Try feature matching if both images have sufficient size
            if (previousImage.rows > 20 && previousImage.cols > 20 && 
                currentImage.rows > 20 && currentImage.cols > 20) {
                
                OutputDebugStringA("ImageStitcher: Attempting feature matching\n");
                
                try {
                    // Convert to grayscale for feature detection
                    cv::Mat prevGray, currGray;
                    cv::cvtColor(previousImage, prevGray, cv::COLOR_BGRA2GRAY);
                    cv::cvtColor(currentImage, currGray, cv::COLOR_BGRA2GRAY);
                    
                    OutputDebugStringA("ImageStitcher: Converted to grayscale\n");
                    
                    // Try SURF first, fallback to ORB if SURF fails
                    cv::Ptr<cv::Feature2D> detector;
                    bool usingSURF = false;
                    try {
                        // In OpenCV 4, SURF is in the xfeatures2d namespace
                        detector = cv::xfeatures2d::SURF::create(400);
                        usingSURF = true;
                        OutputDebugStringA("ImageStitcher: Created SURF detector\n");
                    } catch (const cv::Exception& e) {
                        char errBuf[512];
                        sprintf_s(errBuf, "ImageStitcher: SURF not available (%s), using ORB detector\n", e.what());
                        OutputDebugStringA(errBuf);
                        detector = cv::ORB::create(1000);
                        usingSURF = false;
                    } catch (...) {
                        OutputDebugStringA("ImageStitcher: SURF not available (unknown error), using ORB detector\n");
                        detector = cv::ORB::create(1000);
                        usingSURF = false;
                    }
                    
                    std::vector<cv::KeyPoint> keypointsPrev, keypointsCurr;
                    cv::Mat descriptorsPrev, descriptorsCurr;
                    
                    detector->detect(prevGray, keypointsPrev);
                    detector->detect(currGray, keypointsCurr);
                    
                    char kpBuf[256];
                    sprintf_s(kpBuf, "ImageStitcher: Found %d keypoints in prev, %d in curr\n", 
                             (int)keypointsPrev.size(), (int)keypointsCurr.size());
                    OutputDebugStringA(kpBuf);
                    
                    // Check if we have enough keypoints
                    if (keypointsPrev.size() > 4 && keypointsCurr.size() > 4) {
                        OutputDebugStringA("ImageStitcher: Sufficient keypoints found, computing descriptors\n");
                        
                        detector->compute(prevGray, keypointsPrev, descriptorsPrev);
                        detector->compute(currGray, keypointsCurr, descriptorsCurr);
                        
                        // Match features
                        std::vector<cv::DMatch> matches;
                        if (!descriptorsPrev.empty() && !descriptorsCurr.empty()) {
                            OutputDebugStringA("ImageStitcher: Computing matches\n");
                            
                            // Use different matchers based on detector type
                            cv::Ptr<cv::DescriptorMatcher> matcher;
                            if (usingSURF) {
                                // SURF uses float descriptors, use FLANN
                                matcher = cv::DescriptorMatcher::create(cv::DescriptorMatcher::FLANNBASED);
                                OutputDebugStringA("ImageStitcher: Using FLANN matcher for SURF\n");
                            } else {
                                // ORB uses binary descriptors, use brute force with Hamming distance
                                matcher = cv::DescriptorMatcher::create(cv::DescriptorMatcher::BRUTEFORCE_HAMMING);
                                OutputDebugStringA("ImageStitcher: Using Brute Force Hamming matcher for ORB\n");
                            }
                            
                            try {
                                matcher->match(descriptorsCurr, descriptorsPrev, matches);
                                
                                char matchBuf[256];
                                sprintf_s(matchBuf, "ImageStitcher: Found %d raw matches\n", (int)matches.size());
                                OutputDebugStringA(matchBuf);
                            } catch (const std::exception& e) {
                                char matchErrBuf[512];
                                sprintf_s(matchErrBuf, "ImageStitcher: Matching failed: %s\n", e.what());
                                OutputDebugStringA(matchErrBuf);
                                matches.clear();
                            }
                            
                            // Find good matches - adjust based on detector type
                            if (!matches.empty()) {
                                double maxDist = 0, minDist = 100;
                                for (const auto& match : matches) {
                                    double dist = match.distance;
                                    if (dist < minDist) minDist = dist;
                                    if (dist > maxDist) maxDist = dist;
                                }
                                
                                char distBuf[256];
                                sprintf_s(distBuf, "ImageStitcher: Distance range: min=%.2f, max=%.2f\n", minDist, maxDist);
                                OutputDebugStringA(distBuf);
                                
                                std::vector<cv::DMatch> goodMatches;
                                double threshold;
                                
                                if (usingSURF) {
                                    // SURF has different distance characteristics
                                    threshold = std::max(minDist * 4.0, 50.0);
                                } else {
                                    // ORB binary descriptors have different distance range (0-256)
                                    threshold = std::max(minDist * 2.0, 30.0);
                                    if (threshold > 100) threshold = 100; // Cap at reasonable value for ORB
                                }
                                
                                for (const auto& match : matches) {
                                    if (match.distance < threshold) {
                                        goodMatches.push_back(match);
                                    }
                                }
                                
                                char goodMatchBuf[256];
                                sprintf_s(goodMatchBuf, "ImageStitcher: Found %d good matches (threshold: %.2f, detector: %s)\n", 
                                         (int)goodMatches.size(), threshold, usingSURF ? "SURF" : "ORB");
                                OutputDebugStringA(goodMatchBuf);
                                
                                // If we have enough good matches, find homography
                                if (goodMatches.size() >= 4) { // Reduced from > 4 to >= 4
                                char goodMatchBuf[256];
                                sprintf_s(goodMatchBuf, "ImageStitcher: Found %d good matches, computing homography\n", (int)goodMatches.size());
                                OutputDebugStringA(goodMatchBuf);
                                
                                std::vector<cv::Point2f> pointsCurr, pointsPrev;
                                for (const auto& match : goodMatches) {
                                    pointsCurr.push_back(keypointsCurr[match.queryIdx].pt);
                                    pointsPrev.push_back(keypointsPrev[match.trainIdx].pt);
                                }
                                
                                // Find homography matrix
                                cv::Mat H = cv::findHomography(pointsCurr, pointsPrev, cv::RANSAC);
                                
                                if (!H.empty()) {
                                    OutputDebugStringA("ImageStitcher: Successfully computed homography, applying transformation\n");
                                    // Calculate offset for proper alignment
                                    int alignedYOffset = yOffset - 10; // Slight overlap for smoother transition
                                    if (alignedYOffset < 0) alignedYOffset = 0;
                                    
                                    // Apply perspective transformation to align images
                                    cv::Mat warpedImage;
                                    cv::warpPerspective(currentImage, warpedImage, H, 
                                                      cv::Size(maxWidth, currentImage.rows));
                                    
                                    // Blend the images where they overlap
                                    cv::Rect overlapRect(0, alignedYOffset, 
                                                         std::min(warpedImage.cols, result.cols),
                                                         std::min(warpedImage.rows, result.rows - alignedYOffset));
                                    
                                    if (overlapRect.width > 0 && overlapRect.height > 0) {
                                        cv::Mat overlapRoi = result(overlapRect);
                                        cv::Mat warpedRoi = warpedImage(cv::Rect(0, 0, overlapRect.width, overlapRect.height));
                                        
                                        // Use alpha blending for smoother transition
                                        double alpha = 0.7; // Weight for the warped image
                                        
                                        for (int y = 0; y < overlapRoi.rows; y++) {
                                            for (int x = 0; x < overlapRoi.cols; x++) {
                                                cv::Vec4b& pixelOverlap = overlapRoi.at<cv::Vec4b>(y, x);
                                                cv::Vec4b& pixelWarped = warpedRoi.at<cv::Vec4b>(y, x);
                                                
                                                if (pixelWarped[3] > 0) {  // If not transparent
                                                    pixelOverlap = alpha * pixelWarped + (1 - alpha) * pixelOverlap;
                                                }
                                            }
                                        }
                                    }
                                    
                                    // Adjust yOffset for the next image
                                    yOffset += currentImage.rows - 10; // Account for overlap
                                    
                                    // Update previous image for next iteration
                                    currentImage.copyTo(previousImage);
                                    OutputDebugStringA("ImageStitcher: Feature matching successful for this image\n");
                                    continue;
                                } else {
                                    OutputDebugStringA("ImageStitcher: Homography is empty, falling back to simple stacking\n");
                                }
                            } else {
                                char insufficientBuf[256];
                                sprintf_s(insufficientBuf, "ImageStitcher: Insufficient good matches (%d), falling back to simple stacking\n", (int)goodMatches.size());
                                OutputDebugStringA(insufficientBuf);
                            }
                            } else {
                                OutputDebugStringA("ImageStitcher: No matches found, falling back to simple stacking\n");
                            }
                        } else {
                            OutputDebugStringA("ImageStitcher: Empty descriptors, falling back to simple stacking\n");
                        }
                    } else {
                        char insufficientKpBuf[256];
                        sprintf_s(insufficientKpBuf, "ImageStitcher: Insufficient keypoints (prev: %d, curr: %d), falling back to simple stacking\n", 
                                 (int)keypointsPrev.size(), (int)keypointsCurr.size());
                        OutputDebugStringA(insufficientKpBuf);
                    }
                }
                catch (const std::exception& e) {
                    char exBuf[512];
                    sprintf_s(exBuf, "ImageStitcher: Exception during feature matching: %s\n", e.what());
                    OutputDebugStringA(exBuf);
                    // If feature matching fails, fall back to simple stacking
                }
                catch (...) {
                    OutputDebugStringA("ImageStitcher: Unknown exception during feature matching, falling back to simple stacking\n");
                    // If feature matching fails, fall back to simple stacking
                }
            } else {
                OutputDebugStringA("ImageStitcher: Images too small for feature matching, using simple stacking\n");
            }
            
            // If feature matching failed or wasn't attempted, just stack vertically
            OutputDebugStringA("ImageStitcher: Using simple vertical stacking for this image\n");
            cv::Rect roi_rect(0, yOffset, 
                              std::min(currentImage.cols, result.cols), 
                              std::min(currentImage.rows, result.rows - yOffset));
            
            if (roi_rect.width > 0 && roi_rect.height > 0) {
                cv::Mat roi = result(roi_rect);
                cv::Mat src_roi = currentImage(cv::Rect(0, 0, roi_rect.width, roi_rect.height));
                src_roi.copyTo(roi);
                
                // Add a separator line
                cv::line(result, 
                        cv::Point(0, yOffset), 
                        cv::Point(maxWidth, yOffset), 
                        cv::Scalar(200, 200, 200, 255), 1);
            }
            
            // Update position and reference image
            yOffset += currentImage.rows;
            currentImage.copyTo(previousImage);
        }
        
        // Convert result back to HBITMAP
        OutputDebugStringA("ImageStitcher: Converting result back to HBITMAP\n");
        return MatToHBitmap(result);
        
    } catch (const std::exception& e) {
        char exBuf[512];
        sprintf_s(exBuf, "ImageStitcher: Exception in StitchImagesWithFeatureMatching: %s\n", e.what());
        OutputDebugStringA(exBuf);
        // Fall back to simple vertical stacking in case of any exception
        return StitchImagesVertically(bitmaps);
    } catch (...) {
        OutputDebugStringA("ImageStitcher: Unknown exception in StitchImagesWithFeatureMatching, falling back to simple stacking\n");
        // Fall back to simple vertical stacking in case of any exception
        return StitchImagesVertically(bitmaps);
    }
}

HBITMAP ImageStitcher::StitchImagesVertically(const std::vector<HBITMAP>& bitmaps) {
    if (bitmaps.empty())
        return NULL;
    
    if (bitmaps.size() == 1) {
        // Create a copy of the bitmap
        BITMAP bmp;
        GetObject(bitmaps[0], sizeof(BITMAP), &bmp);
        
        HDC hdcScreen = GetDC(NULL);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, bmp.bmWidth, bmp.bmHeight);
        
        if (hBitmap) {
            HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, bitmaps[0]);
            HDC hdcMemDest = CreateCompatibleDC(hdcScreen);
            HBITMAP hOldBitmapDest = (HBITMAP)SelectObject(hdcMemDest, hBitmap);
            
            BitBlt(hdcMemDest, 0, 0, bmp.bmWidth, bmp.bmHeight, hdcMem, 0, 0, SRCCOPY);
            
            SelectObject(hdcMemDest, hOldBitmapDest);
            SelectObject(hdcMem, hOldBitmap);
            DeleteDC(hdcMemDest);
            DeleteDC(hdcMem);
            ReleaseDC(NULL, hdcScreen);
            
            return hBitmap;
        }
        ReleaseDC(NULL, hdcScreen);
        return NULL;
    }
    
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
        
        // Use the largest width
        if (bInfo.bmWidth > width) {
            width = bInfo.bmWidth;
        }
    }
    
    // Create a large empty bitmap
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hCombined = CreateCompatibleBitmap(hdcScreen, width, totalHeight);
    
    if (hCombined) {
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
            
            // Use regular BitBlt
            BitBlt(
                hdcMem, xOffset, yPos, bInfo.bmWidth, bInfo.bmHeight,
                hdcBitmap, 0, 0, SRCCOPY
            );
            
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
    }
    
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
    
    return hCombined;
}

cv::Mat ImageStitcher::HBitmapToMat(HBITMAP hBitmap) {
    // Get bitmap information
    BITMAP bm;
    GetObject(hBitmap, sizeof(BITMAP), &bm);
    
    // Create a device context
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    
    // Select the bitmap into the device context
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);
    
    // Create a temporary bitmap with 32-bit color depth
    BITMAPINFO bi = { 0 };
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = bm.bmWidth;
    bi.bmiHeader.biHeight = -bm.bmHeight;  // Negative for top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;  // 4 channels (RGBA)
    bi.bmiHeader.biCompression = BI_RGB;
    
    // Create a buffer for pixel data
    std::vector<BYTE> buffer(bm.bmWidth * bm.bmHeight * 4);
    
    // Get the bitmap bits
    GetDIBits(hdcMem, hBitmap, 0, bm.bmHeight, buffer.data(), &bi, DIB_RGB_COLORS);
    
    // Clean up
    SelectObject(hdcMem, hOldBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
    
    // Create cv::Mat from the buffer (BGRA format)
    cv::Mat mat(bm.bmHeight, bm.bmWidth, CV_8UC4, buffer.data());
    
    // Make a deep copy of the data
    cv::Mat result;
    mat.copyTo(result);
    
    return result;
}

HBITMAP ImageStitcher::MatToHBitmap(const cv::Mat& mat) {
    // Convert to BGR (24-bit) for better Paint compatibility
    cv::Mat bgr;
    
    if (mat.type() == CV_8UC4) {
        // Convert BGRA to BGR (remove alpha channel)
        cv::cvtColor(mat, bgr, cv::COLOR_BGRA2BGR);
    } 
    else if (mat.type() == CV_8UC3) {
        bgr = mat;
    }
    else if (mat.channels() == 1) {
        cv::cvtColor(mat, bgr, cv::COLOR_GRAY2BGR);
    }
    else {
        // Unsupported format, return NULL
        return NULL;
    }
    
    // Create the bitmap info header for 24-bit BGR
    BITMAPINFO bi = { 0 };
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = bgr.cols;
    bi.bmiHeader.biHeight = -bgr.rows;  // Negative for top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 24;  // 3 channels (BGR), no alpha
    bi.bmiHeader.biCompression = BI_RGB;
    
    // Create a device context
    HDC hdcScreen = GetDC(NULL);
    
    // Create a DIB section
    void* pBits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(hdcScreen, &bi, DIB_RGB_COLORS, &pBits, NULL, 0);
    
    if (hBitmap && pBits) {
        // Calculate the stride for 24-bit bitmap (must be 4-byte aligned)
        int stride = ((bgr.cols * 3 + 3) / 4) * 4;
        
        // Copy the pixel data line by line, handling stride alignment
        for (int y = 0; y < bgr.rows; y++) {
            memcpy((BYTE*)pBits + y * stride, bgr.ptr<BYTE>(y), bgr.cols * 3);
        }
    }
    
    ReleaseDC(NULL, hdcScreen);
    
    return hBitmap;
}