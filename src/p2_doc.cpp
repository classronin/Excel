// ============================================================================
//  p2_doc.cpp
//  Responsibility: document model
//  Cell / Doc state, snapshots, SQLite persistence, media copy.
//
//  Shared contract: core.h + only the api_*.h headers this module references.
//  This translation unit contains no type or state definitions of its own (state.cpp does).
// ============================================================================
#include "core.h"
#include "api_p1_core.h"
#include "api_p2_doc.h"
// ---- src\p2_doc.inc ----
// ============================================================================
//  Excel - part 2: document model, SQLite persistence, media
// ============================================================================





long long CellKey(int r, int c)
{
    return ((long long)r << 32) | (unsigned int)c;
}

int KeyRow(long long k)
{ return (int)(k >> 32); }

int KeyCol(long long k)
{ return (int)(k & 0xFFFFFFFF); }





// ---------------------------------------------------------------------------
//  media copy helpers
// ---------------------------------------------------------------------------

bool CopyDirTree(const std::wstring& src, const std::wstring& dst, std::wstring& err)
{
    if (!EnsureDirectory(dst)) { err = L"无法创建目录: " + dst; return false; }
    std::wstring pattern = PathJoin(src, L"*");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        DWORD e = GetLastError();
        if (e == ERROR_FILE_NOT_FOUND) return true;
        err = L"无法读取目录: " + src;
        return false;
    }
    bool ok = true;
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        std::wstring s = PathJoin(src, fd.cFileName);
        std::wstring d = PathJoin(dst, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (!CopyDirTree(s, d, err)) { ok = false; break; }
        } else {
            if (!CopyFileW(s.c_str(), d.c_str(), FALSE)) {
                err = L"复制失败: " + s;
                ok = false;
                break;
            }
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return ok;
}


std::wstring UniqueNameInDir(const std::wstring& dir, const std::wstring& name, bool isFolder)
{
    std::wstring candidate = PathJoin(dir, name);
    if (GetFileAttributesW(candidate.c_str()) == INVALID_FILE_ATTRIBUTES) return name;
    std::wstring stem = name, ext;
    size_t dot = name.find_last_of(L'.');
    if (!isFolder && dot != std::wstring::npos && dot > 0) {
        stem = name.substr(0, dot);
        ext = name.substr(dot);
    }
    for (int i = 1; i < 10000; ++i) {
        std::wstring n = stem + L" (" + std::to_wstring(i) + L")" + ext;
        if (GetFileAttributesW(PathJoin(dir, n).c_str()) == INVALID_FILE_ATTRIBUTES)
            return n;
    }
    return stem + L"_" + std::to_wstring(GetTickCount64()) + ext;
}


MediaCopyResult CopyIntoMedia(const std::wstring& mediaDir, const std::wstring& srcAbs, bool isFolder, bool unique)
{
    MediaCopyResult res;
    res.abs = srcAbs;
    if (mediaDir.empty()) { res.err = L"无法定位 exe 同级的 media 目录"; return res; }
    if (!EnsureDirectory(mediaDir)) { res.err = L"无法创建 media 目录"; return res; }

    std::wstring name = PathBaseName(srcAbs);
    std::wstring target = PathJoin(mediaDir, name);
    bool exists = GetFileAttributesW(target.c_str()) != INVALID_FILE_ATTRIBUTES;
    if (exists && unique) {
        name = UniqueNameInDir(mediaDir, name, isFolder);
        target = PathJoin(mediaDir, name);
        exists = false;
    }
    if (exists) {
        res.existed = true;
        res.rel = L"media\\" + name;
        res.abs = target;
        res.err = L"已经存在";
        return res;
    }
    if (isFolder) {
        if (!CopyDirTree(srcAbs, target, res.err)) return res;
    } else {
        if (!CopyFileW(srcAbs.c_str(), target.c_str(), FALSE)) {
            res.err = L"复制失败: " + srcAbs;
            return res;
        }
    }
    res.ok = true;
    res.rel = L"media\\" + name;
    res.abs = target;
    return res;
}


void Doc::AutoCopyPending()
{
    if (!g_autoCopy) return;
    std::wstring media = MediaDirAbs();
    if (media.empty()) return;
    bool changed = false;
    for (auto& kv : cells) {
        Cell& cell = kv.second;
        if (cell.path.empty() || cell.kind == CK_URL || cell.kind == CK_TEXT) continue;
        if (!IsAbsPath(cell.path)) continue;
        if (IsUnderDir(cell.path, media)) continue;  // 已在 media 内
        if (!FileExistsW(cell.path) && !DirExistsW(cell.path)) continue;
        MediaCopyResult r = CopyIntoMedia(media, cell.path, cell.kind == CK_FOLDER, true);
        if (r.ok) {
            cell.path = StoragePathOf(r.abs);
            changed = true;
        }
    }
    if (changed) { dirty = true; ++g_contentRev; }
}


// ---------------------------------------------------------------------------
//  document list
// ---------------------------------------------------------------------------

Doc* ActiveDoc()
{
    if (g_active >= 0 && g_active < (int)g_docs.size())
        return g_docs[g_active].get();
    return nullptr;
}


Doc* AddBlankDoc()
{
    auto doc = std::make_unique<Doc>();
    doc->title = L"未命名-" + std::to_wstring(g_untitledSeq++);
    g_docs.push_back(std::move(doc));
    g_active = (int)g_docs.size() - 1;
    UpdateWindowTitle();
    InvalidateAll();
    return ActiveDoc();
}


void RebuildAllLayouts()
{
    for (auto& d : g_docs) d->InvalidateLayout();
}


// Capacity is declared by Doc::ReconcileCapacity, called from the frame commit
// and from the single view-request entry point. There is deliberately no second
// capacity RULE and no second place that resizes documents.


