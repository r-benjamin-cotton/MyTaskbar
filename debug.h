#pragma once

#include "framework.h"

#if defined(_DEBUG)

void DebugPrintf(const wchar_t* frm, ...)
{
    const size_t count = 1024;
    wchar_t work[count];
    va_list list;
    va_start(list, frm);
    _vsnwprintf_s(work, count, _TRUNCATE, frm, list);
    OutputDebugString(work);
}

#else

#define DebugPrintf(...)

#endif

std::wstring FormatString(const wchar_t* frm, ...)
{
    const size_t count = 1024;
    wchar_t work[count];
    va_list list;
    va_start(list, frm);
    _vsnwprintf_s(work, count, _TRUNCATE, frm, list);
    return std::wstring(work);
}
