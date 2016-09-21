// Part 2a - Uses memory-mapped files for dictionary file reading and Win32 MultiByteToWideChar
//           for converting to UTF-16, and the ATL CStringW class.
//
// The main difference with V2 is that here I tried ATL's CStringW instead of STL's wstring.
//
//
// Based on:
//
// Loading the dictionary, part 3: Breaking the text into lines
// https://blogs.msdn.microsoft.com/oldnewthing/20050513-26/?p=35643
//
// Loading the dictionary, part 4: Character conversion redux
// https://blogs.msdn.microsoft.com/oldnewthing/20050516-30/?p=35633
//

#include <windows.h>
#include <atlstr.h>     // for CStringW
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
    bool Parse(const CStringW& line);
    CStringW trad;
    CStringW simp;
    CStringW pinyin;
    CStringW english;
};

bool DictionaryEntry::Parse(const CStringW& line)
{
    static const int kNotFound = -1;
    int start = 0;
    int end = line.Find(L' ', start);
    if (end == kNotFound) return false;
    trad = CStringW((line.GetString()) + start, end - start);
    
    start = line.Find(L'[', end);
    if (start == kNotFound) return false;
    end = line.Find(L']', ++start);
    if (end == kNotFound) return false;
    pinyin = CStringW((line.GetString()) + start, end - start);
    
    start = line.Find(L'/', end);
    if (start == kNotFound) return false;
    start++;
    end = line.ReverseFind(L'/');
    if (end == kNotFound) return false;
    if (end <= start) return false;
    english = CStringW((line.GetString()) + start, end - start);

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
                CStringW line(buf, cchResult);
                DictionaryEntry de;
                if (de.Parse(line)) {
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
    cout << "V.2A: Uses memory mapped files, MultiByteToWideChar and ATL CStringW.\n\n";

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

