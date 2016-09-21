// Part 3 - Builds on part 2, avoiding string copies using raw C-style strings.
//
// Based on:
//
// Loading the dictionary, part 5: Avoiding string copying
// https://blogs.msdn.microsoft.com/oldnewthing/20050518-42/?p=35613//


#include <windows.h>
#include <algorithm>
#include <iostream> // for cin/cout
#include <vector>
#include "Stopwatch.h"

using std::vector;

using std::cout;
using win32::Stopwatch;


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

    bool Parse(const WCHAR* begin, const WCHAR* end);
    void Destruct() {
        delete[] m_pszTrad;
        delete[] m_pszSimp;
        delete[] m_pszPinyin;
        delete[] m_pszEnglish;
    }
    LPWSTR m_pszTrad;
    LPWSTR m_pszSimp;
    LPWSTR m_pszPinyin;
    LPWSTR m_pszEnglish;
};

LPWSTR AllocString(const WCHAR* begin, const WCHAR* end)
{
    int cch = end - begin + 1;
    LPWSTR psz = new WCHAR[cch];
    lstrcpynW(psz, begin, cch);
    return psz;
}


bool DictionaryEntry::Parse(
    const WCHAR* begin, const WCHAR* end)
{
    const WCHAR* pch = std::find(begin, end, L' ');
    if (pch >= end) return false;
    m_pszTrad = AllocString(begin, pch);
    begin = std::find(pch, end, L'[') + 1;
    if (begin >= end) return false;
    pch = std::find(begin, end, L']');
    if (pch >= end) return false;
    m_pszPinyin = AllocString(begin, pch);
    begin = std::find(pch, end, L'/') + 1;
    if (begin >= end) return false;
    for (pch = end; *--pch != L'/'; ) {}
    if (begin >= pch) return false;
    m_pszEnglish = AllocString(begin, pch);
    return true;
}

class Dictionary
{
public:
    Dictionary();
    ~Dictionary();
    int Length() { return v.size(); }
    const DictionaryEntry& Item(int i) { return v[i]; }
private:
    vector<DictionaryEntry> v;
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
                if (de.Parse(buf, buf + cchResult)) {
                    v.push_back(de);
                }
            }
            delete[] buf;
        }
        pchBuf = pchEOL + 1;
    }
}

Dictionary::~Dictionary()
{
    for (vector<DictionaryEntry>::iterator i = v.begin();
        i != v.end(); ++i) {
        i->Destruct();
    }
}

int main()
{
    cout << "Loading Chinese English Dictionary\n";
    cout << "V.3: Uses memory mapped files, MultiByteToWideChar and raw C-style strings.\n\n";

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

