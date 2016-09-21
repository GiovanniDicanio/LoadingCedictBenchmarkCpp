// Part 4 - Builds on part 3, uses custom memory pool allocator
//
// Based on:
//
// Loading the dictionary, part 6: Taking advantage of our memory allocation pattern
//
// https://blogs.msdn.microsoft.com/oldnewthing/20050519-00/?p=35603

#include <windows.h>
#include <algorithm>
#include <iostream> // for cin/cout
#include <vector>
#include "Stopwatch.h"

using std::vector;

using std::cout;
using win32::Stopwatch;


class StringPool
{
public:
    StringPool();
    ~StringPool();
    LPWSTR AllocString(const WCHAR* pszBegin, const WCHAR* pszEnd);

private:
    union HEADER {
        struct {
            HEADER* m_phdrPrev;
            SIZE_T  m_cb;
        };
        WCHAR alignment;
    };
    enum {
        MIN_CBCHUNK = 32000,
        MAX_CHARALLOC = 1024 * 1024
    };

private:
    WCHAR*  m_pchNext;   // first available byte
    WCHAR*  m_pchLimit;  // one past last available byte
    HEADER* m_phdrCur;   // current block
    DWORD   m_dwGranularity;
};

inline DWORD RoundUp(DWORD cb, DWORD units)
{
    return ((cb + units - 1) / units) * units;
}

StringPool::StringPool()
    : m_pchNext(NULL), m_pchLimit(NULL), m_phdrCur(NULL)
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    m_dwGranularity = RoundUp(sizeof(HEADER) + MIN_CBCHUNK,
        si.dwAllocationGranularity);
}

LPWSTR StringPool::AllocString(const WCHAR* pszBegin, const WCHAR* pszEnd)
{
    size_t cch = pszEnd - pszBegin + 1;
    LPWSTR psz = m_pchNext;
    if (m_pchNext + cch <= m_pchLimit) {
        m_pchNext += cch;
        lstrcpynW(psz, pszBegin, cch);
        return psz;
    }

    if (cch > MAX_CHARALLOC) goto OOM;
    DWORD cbAlloc = RoundUp(cch * sizeof(WCHAR) + sizeof(HEADER),
        m_dwGranularity);
    BYTE* pbNext = reinterpret_cast<BYTE*>(
        VirtualAlloc(NULL, cbAlloc, MEM_COMMIT, PAGE_READWRITE));
    if (!pbNext) {
    OOM:
        static std::bad_alloc OOM;
        throw(OOM);
    }

    m_pchLimit = reinterpret_cast<WCHAR*>(pbNext + cbAlloc);
    HEADER* phdrCur = reinterpret_cast<HEADER*>(pbNext);
    phdrCur->m_phdrPrev = m_phdrCur;
    phdrCur->m_cb = cbAlloc;
    m_phdrCur = phdrCur;
    m_pchNext = reinterpret_cast<WCHAR*>(phdrCur + 1);

    return AllocString(pszBegin, pszEnd);
}

StringPool::~StringPool()
{
    HEADER* phdr = m_phdrCur;
    while (phdr) {
        HEADER hdr = *phdr;
        VirtualFree(hdr.m_phdrPrev, hdr.m_cb, MEM_RELEASE);
        phdr = hdr.m_phdrPrev;
    }
}



class MappedTextFile
{
public:
    MappedTextFile(LPCTSTR pszFile);
    ~MappedTextFile();

    const CHAR *Buffer() { return m_p; }
    DWORD Length() const { return m_cb; }

private:
    PCHAR   m_p;
    DWORD   m_cb;
    HANDLE  m_hf;
    HANDLE  m_hfm;
};

MappedTextFile::MappedTextFile(LPCTSTR pszFile)
    : m_hfm(NULL), m_p(NULL), m_cb(0)
{
    m_hf = CreateFile(pszFile, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (m_hf != INVALID_HANDLE_VALUE) {
        DWORD cb = GetFileSize(m_hf, NULL);
        m_hfm = CreateFileMapping(m_hf, NULL, PAGE_READONLY, 0, 0, NULL);
        if (m_hfm != NULL) {
            m_p = reinterpret_cast<PCHAR>
                (MapViewOfFile(m_hfm, FILE_MAP_READ, 0, 0, cb));
            if (m_p) {
                m_cb = cb;
            }
        }
    }
}

MappedTextFile::~MappedTextFile()
{
    if (m_p) UnmapViewOfFile(m_p);
    if (m_hfm) CloseHandle(m_hfm);
    if (m_hf != INVALID_HANDLE_VALUE) CloseHandle(m_hf);
}


struct DictionaryEntry
{
    DictionaryEntry() 
        : m_pszTrad(nullptr)
        , m_pszSimp(nullptr)
        , m_pszPinyin(nullptr)
        , m_pszEnglish(nullptr)
    {}

    bool Parse(const WCHAR* begin, const WCHAR* end, StringPool& pool);

    LPWSTR m_pszTrad;
    LPWSTR m_pszSimp;
    LPWSTR m_pszPinyin;
    LPWSTR m_pszEnglish;
};


bool DictionaryEntry::Parse(
    const WCHAR* begin, const WCHAR* end,
    StringPool& pool)
{
    const WCHAR* pch = std::find(begin, end, L' ');
    if (pch >= end) return false;
    m_pszTrad = pool.AllocString(begin, pch);
    begin = std::find(pch, end, L'[') + 1;
    if (begin >= end) return false;
    pch = std::find(begin, end, L']');
    if (pch >= end) return false;
    m_pszPinyin = pool.AllocString(begin, pch);
    begin = std::find(pch, end, L'/') + 1;
    if (begin >= end) return false;
    for (pch = end; *--pch != L'/'; ) {}
    if (begin >= pch) return false;
    m_pszEnglish = pool.AllocString(begin, pch);
    return true;
}

class Dictionary
{
public:
    Dictionary();
    int Length() { return v.size(); }
    const DictionaryEntry& Item(int i) { return v[i]; }
private:
    vector<DictionaryEntry> v;
    StringPool m_pool;
};

Dictionary::Dictionary()
{
    MappedTextFile mtf(TEXT("cedict.u8"));
    const CHAR* pchBuf = mtf.Buffer();
    const CHAR* pchEnd = pchBuf + mtf.Length();
    while (pchBuf < pchEnd) {
        const CHAR* pchEOL = std::find(pchBuf, pchEnd, '\n');
        if (*pchBuf != '#') {
            size_t cchBuf = pchEOL - pchBuf;
            wchar_t* buf = new wchar_t[cchBuf];

            DWORD cchResult = MultiByteToWideChar(CP_UTF8, 0,
                pchBuf, cchBuf, buf, cchBuf);
            if (cchResult) {
                DictionaryEntry de;
                if (de.Parse(buf, buf + cchResult, m_pool)) {
                    v.push_back(de);
                }
            }
            delete[] buf;
        }
        pchBuf = pchEOL + 1;
    }
}


int main()
{
    cout << "Loading Chinese English Dictionary\n";
    cout << "V.4: Uses memory mapped files, MultiByteToWideChar and custom pool string allocator.\n\n";

    Stopwatch sw;
    double timeWithoutDtors = 0;

    sw.Start();
    {
        Dictionary dict;
        cout << dict.Length() << '\n';
        timeWithoutDtors = sw.ElapsedMilliseconds();
    }
    sw.Stop();
    double timeTotal = sw.ElapsedMilliseconds();
    
    cout << "Total time:         " << timeTotal << " ms\n";
    cout << "Time without dtors: " << timeWithoutDtors << " ms\n";
}

