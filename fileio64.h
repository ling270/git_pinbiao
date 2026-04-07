#pragma once

#include <cstdio>
#include <QtGlobal>

inline bool seekFile64(FILE* f, qint64 offset, int origin)
{
    if (!f) return false;
#ifdef _WIN32
    return _fseeki64(f, static_cast<__int64>(offset), origin) == 0;
#else
    return fseeko(f, static_cast<off_t>(offset), origin) == 0;
#endif
}

inline qint64 tellFile64(FILE* f)
{
    if (!f) return -1;
#ifdef _WIN32
    return static_cast<qint64>(_ftelli64(f));
#else
    return static_cast<qint64>(ftello(f));
#endif
}
