// ============================================================================
//  api_p4_image.h - module contract: state + interfaces of p4_image.
//  Included only by translation units that reference this module.
// ============================================================================
#pragma once
#include "core.h"

inline IWICImagingFactory* g_wic = nullptr;
inline std::unordered_map<std::wstring, std::shared_ptr<ImageData>> g_imageCache;
inline size_t g_imageCacheBytes = 0;
IWICImagingFactory* WicFactory();
int PropDelay(IWICBitmapFrameDecode* frame);
std::shared_ptr<ImageData> LoadImageWIC(const std::wstring& path, int boxW, int boxH, bool& failed);
std::shared_ptr<ImageData> LoadImageFallback(const std::wstring& path, int boxW, int boxH);
void TrimImageCache();
std::shared_ptr<ImageData> GetImage(const std::wstring& absPath, int boxW, int boxH);
void DrawImageCell(HDC dc, const std::wstring& absPath, const RECT& rc, bool& animated);
