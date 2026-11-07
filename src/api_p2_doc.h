// ============================================================================
//  api_p2_doc.h - module contract: state + interfaces of p2_doc.
//  Included only by translation units that reference this module.
// ============================================================================
#pragma once
#include "core.h"

inline std::vector<std::unique_ptr<Doc>> g_docs;
inline int g_untitledSeq = 1;
void SetStatus(const std::wstring& s);
void SaveSessionState();
bool CopyDirTree(const std::wstring& src, const std::wstring& dst, std::wstring& err);
std::wstring UniqueNameInDir(const std::wstring& dir, const std::wstring& name, bool isFolder);
MediaCopyResult CopyIntoMedia(const std::wstring& mediaDir, const std::wstring& srcAbs,                                      bool isFolder, bool unique);
Doc* AddBlankDoc();
void RebuildAllLayouts();
