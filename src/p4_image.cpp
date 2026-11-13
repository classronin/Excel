// ============================================================================
//  p4_image.cpp
//  Responsibility: image pipeline
//  WIC decoding, frame animation and the image cache.
//
//  Shared contract: core.h + only the api_*.h headers this module references.
//  This translation unit contains no type or state definitions of its own (state.cpp does).
// ============================================================================
#include "core.h"
#include "api_p4_image.h"
// ---- src\p4_image.inc ----
// ============================================================================
//  Excel - part 4: WIC image loading, frame animation, cache
// ============================================================================



IWICImagingFactory* WicFactory()
{
    if (g_wic) return g_wic;
    IWICImagingFactory* f = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&f));
    if (SUCCEEDED(hr)) g_wic = f;
    return g_wic;
}


int PropDelay(IWICBitmapFrameDecode* frame)
{
    int delay = 100;
    IWICMetadataQueryReader* reader = nullptr;
    if (SUCCEEDED(frame->GetMetadataQueryReader(&reader)) && reader) {
        PROPVARIANT pv;
        PropVariantInit(&pv);
        if (SUCCEEDED(reader->GetMetadataByName(L"/grctlext/Delay", &pv))) {
            if (pv.vt == VT_UI2 && pv.uiVal > 0) {
                delay = (int)pv.uiVal * 10;
                if (delay < 20) delay = 20;
                if (delay > 1000) delay = 1000;
            }
        }
        PropVariantClear(&pv);
        reader->Release();
    }
    return delay;
}


std::shared_ptr<ImageData> LoadImageWIC(const std::wstring& path, int boxW, int boxH, bool& failed)
{
    failed = false;
    if (boxW < 4) boxW = 4;
    if (boxH < 4) boxH = 4;
    IWICImagingFactory* fac = WicFactory();
    if (!fac) { failed = true; return nullptr; }

    IWICBitmapDecoder* decoder = nullptr;
    HRESULT hr = fac->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                WICDecodeMetadataCacheOnDemand, &decoder);
    if (FAILED(hr) || !decoder) { failed = true; return nullptr; }

    UINT count = 0;
    decoder->GetFrameCount(&count);
    if (count == 0) { decoder->Release(); failed = true; return nullptr; }
    UINT take = std::min<UINT>(count, 64);

    auto data = std::make_shared<ImageData>();
    data->startTick = GetTickCount64();

    for (UINT i = 0; i < take; ++i) {
        IWICBitmapFrameDecode* frame = nullptr;
        if (FAILED(decoder->GetFrame(i, &frame)) || !frame) break;
        UINT sw = 0, sh = 0;
        frame->GetSize(&sw, &sh);
        if (sw == 0 || sh == 0) { frame->Release(); continue; }

        double scale = std::min((double)boxW / (double)sw, (double)boxH / (double)sh);
        if (scale > 4.0) scale = 4.0;
        int tw = std::max(1, (int)floor(sw * scale + 0.5));
        int th = std::max(1, (int)floor(sh * scale + 0.5));

        IWICBitmapScaler* scaler = nullptr;
        if (FAILED(fac->CreateBitmapScaler(&scaler)) || !scaler) { frame->Release(); continue; }
        hr = scaler->Initialize(frame, tw, th, WICBitmapInterpolationModeFant);
        if (FAILED(hr)) { scaler->Release(); frame->Release(); continue; }

        IWICFormatConverter* conv = nullptr;
        if (FAILED(fac->CreateFormatConverter(&conv)) || !conv) { scaler->Release(); frame->Release(); continue; }
        hr = conv->Initialize(scaler, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone,
                              nullptr, 0.0, WICBitmapPaletteTypeCustom);
        if (FAILED(hr)) { conv->Release(); scaler->Release(); frame->Release(); continue; }

        UINT stride = (UINT)tw * 4;
        UINT size = stride * (UINT)th;
        std::vector<BYTE> pixels(size);
        hr = conv->CopyPixels(nullptr, stride, size, pixels.data());
        if (FAILED(hr)) { conv->Release(); scaler->Release(); frame->Release(); continue; }

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = tw;
        bmi.bmiHeader.biHeight = -th;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        HBITMAP bmp = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (bmp && bits) {
            memcpy(bits, pixels.data(), size);
            ImageFrame f;
            f.bmp = bmp;
            f.w = tw;
            f.h = th;
            f.delay = PropDelay(frame);
            data->frames.push_back(f);
            data->totalMs += f.delay;
        } else if (bmp) {
            DeleteObject(bmp);
        }

        conv->Release();
        scaler->Release();
        frame->Release();
    }
    decoder->Release();

    if (data->frames.empty()) { failed = true; return nullptr; }
    if (data->totalMs <= 0) data->totalMs = 100;
    return data;
}


// 非 WIC 可解码的 BMP 兜底
std::shared_ptr<ImageData> LoadImageFallback(const std::wstring& path, int boxW, int boxH)
{
    HBITMAP hb = (HBITMAP)LoadImageW(nullptr, path.c_str(), IMAGE_BITMAP, 0, 0,
                                     LR_LOADFROMFILE | LR_CREATEDIBSECTION);
    if (!hb) return nullptr;
    BITMAP bm = {};
    GetObjectW(hb, sizeof(bm), &bm);

    auto data = std::make_shared<ImageData>();
    data->startTick = GetTickCount64();

    double scale = std::min((double)boxW / (double)std::max(1, (int)bm.bmWidth),
                            (double)boxH / (double)std::max(1, (int)bm.bmHeight));
    if (scale > 4.0) scale = 4.0;
    int tw = std::max(1, (int)floor(bm.bmWidth * scale + 0.5));
    int th = std::max(1, (int)floor(bm.bmHeight * scale + 0.5));

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = tw;
    bmi.bmiHeader.biHeight = -th;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (dib && bits) {
        HDC dc = CreateCompatibleDC(nullptr);
        HGDIOBJ old = SelectObject(dc, hb);
        BITMAPINFO srcBmi = {};
        srcBmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        srcBmi.bmiHeader.biWidth = bm.bmWidth;
        srcBmi.bmiHeader.biHeight = bm.bmHeight;
        srcBmi.bmiHeader.biPlanes = 1;
        srcBmi.bmiHeader.biBitCount = 32;
        srcBmi.bmiHeader.biCompression = BI_RGB;
        HDC dst = CreateCompatibleDC(nullptr);
        HGDIOBJ oldD = SelectObject(dst, dib);
        SetStretchBltMode(dst, HALFTONE);
        StretchBlt(dst, 0, 0, tw, th, dc, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
        SelectObject(dst, oldD);
        DeleteDC(dst);
        SelectObject(dc, old);
        DeleteDC(dc);
        // 强制 alpha = 255
        DWORD* p = (DWORD*)bits;
        for (int i = 0; i < tw * th; ++i) p[i] |= 0xFF000000;
        ImageFrame f;
        f.bmp = dib;
        f.w = tw;
        f.h = th;
        f.delay = 100;
        data->frames.push_back(f);
        data->totalMs = 100;
    }
    DeleteObject(hb);
    if (data->frames.empty()) return nullptr;
    return data;
}



void TrimImageCache()
{
    if (g_imageCache.size() <= 300 && g_imageCacheBytes <= 160u * 1024 * 1024) return;
    g_imageCache.clear();
    g_imageCacheBytes = 0;
}


std::shared_ptr<ImageData> GetImage(const std::wstring& absPath, int boxW, int boxH)
{
    if (absPath.empty() || boxW < 4 || boxH < 4) return nullptr;
    std::wstring key = absPath + L"|" + std::to_wstring(boxW) + L"x" + std::to_wstring(boxH);
    auto it = g_imageCache.find(key);
    if (it != g_imageCache.end()) return it->second;

    bool failed = false;
    std::shared_ptr<ImageData> data = LoadImageWIC(absPath, boxW, boxH, failed);
    if (!data) data = LoadImageFallback(absPath, boxW, boxH);
    if (data) {
        for (auto& f : data->frames)
            g_imageCacheBytes += (size_t)f.w * f.h * 4 + 256;
        TrimImageCache();
        g_imageCache[key] = data;
    } else {
        ImageData* dummy = nullptr;
        (void)dummy;
    }
    return data;
}


HBITMAP LoadEmbeddedImage(UINT resId, int& w, int& h)
{
    w = h = 0;
    HRSRC rc = FindResourceW(g_inst, MAKEINTRESOURCEW(resId), RT_RCDATA);
    if (!rc) return nullptr;
    HGLOBAL res = LoadResource(g_inst, rc);
    if (!res) return nullptr;
    const void* data = LockResource(res);
    DWORD size = SizeofResource(g_inst, rc);
    if (!data || !size) return nullptr;

    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!mem) return nullptr;
    void* dst = GlobalLock(mem);
    if (!dst) { GlobalFree(mem); return nullptr; }
    memcpy(dst, data, size);
    GlobalUnlock(mem);
    IStream* stream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(mem, TRUE, &stream)) || !stream) {
        GlobalFree(mem);
        return nullptr;
    }

    IWICImagingFactory* fac = WicFactory();
    HBITMAP out = nullptr;
    if (fac) {
        IWICBitmapDecoder* decoder = nullptr;
        if (SUCCEEDED(fac->CreateDecoderFromStream(stream, nullptr,
                        WICDecodeMetadataCacheOnDemand, &decoder)) && decoder) {
            IWICBitmapFrameDecode* frame = nullptr;
            if (SUCCEEDED(decoder->GetFrame(0, &frame)) && frame) {
                UINT sw = 0, sh = 0;
                frame->GetSize(&sw, &sh);
                IWICFormatConverter* conv = nullptr;
                if (sw && sh &&
                    SUCCEEDED(fac->CreateFormatConverter(&conv)) && conv &&
                    SUCCEEDED(conv->Initialize(frame, GUID_WICPixelFormat32bppPBGRA,
                        WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
                    UINT stride = sw * 4;
                    std::vector<BYTE> pixels((size_t)stride * sh);
                    if (SUCCEEDED(conv->CopyPixels(nullptr, stride,
                                    (UINT)pixels.size(), pixels.data()))) {
                        BITMAPINFO bmi = {};
                        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                        bmi.bmiHeader.biWidth = (LONG)sw;
                        bmi.bmiHeader.biHeight = -(LONG)sh;
                        bmi.bmiHeader.biPlanes = 1;
                        bmi.bmiHeader.biBitCount = 32;
                        bmi.bmiHeader.biCompression = BI_RGB;
                        void* bits = nullptr;
                        HBITMAP bmp = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS,
                                                       &bits, nullptr, 0);
                        if (bmp && bits) {
                            memcpy(bits, pixels.data(), pixels.size());
                            out = bmp;
                            w = (int)sw;
                            h = (int)sh;
                        } else if (bmp) {
                            DeleteObject(bmp);
                        }
                    }
                }
                if (conv) conv->Release();
                frame->Release();
            }
            decoder->Release();
        }
    }
    stream->Release();
    return out;
}


// 绘制图像（保持比例，居中）
void DrawImageCell(HDC dc, const std::wstring& absPath, const RECT& rc, bool& animated)
{
    animated = false;
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w < 8 || h < 8) return;
    auto img = GetImage(absPath, w - 4, h - 4);
    if (!img) {
        TextIn(dc, L"[图像无法解码]", rc, g_th.faint, UiFont(8), DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS);
        return;
    }
    if (img->Animated()) animated = true;
    int idx = img->FrameIndex(GetTickCount64());
    const ImageFrame& f = img->frames[idx];
    double scale = std::min((double)(w - 4) / f.w, (double)(h - 4) / f.h);
    if (scale > 4.0) scale = 4.0;
    int dw = std::max(1, (int)floor(f.w * scale + 0.5));
    int dh = std::max(1, (int)floor(f.h * scale + 0.5));
    int x = rc.left + (w - dw) / 2;
    int y = rc.top + (h - dh) / 2;

    HDC mem = CreateCompatibleDC(dc);
    HGDIOBJ old = SelectObject(mem, f.bmp);
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    AlphaBlend(dc, x, y, dw, dh, mem, 0, 0, f.w, f.h, bf);
    SelectObject(mem, old);
    DeleteDC(mem);
}


