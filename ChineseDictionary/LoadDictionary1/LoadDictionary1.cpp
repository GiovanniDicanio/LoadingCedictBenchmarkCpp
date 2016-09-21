// Part 1 - Uses iostream for dictionary file reading and codecvt for converting to UTF-16
//
// Based on:
//
// Loading the dictionary, part 1: Starting point
// https://blogs.msdn.microsoft.com/oldnewthing/20050510-55/?p=35673

#include <windows.h>
#include <codecvt>
#include <locale>
#include <string>
#include <fstream>
#include <iostream> // for cin/cout
#include <vector>
#include "Stopwatch.h"

using std::string;
using std::wstring;
using std::vector;

using std::cout;
using win32::Stopwatch;


struct DictionaryEntry
{
    bool Parse(const wstring& line);
    wstring trad;
    wstring simp;
    wstring pinyin;
    wstring english;
};

bool DictionaryEntry::Parse(const wstring& line)
{
    wstring::size_type start = 0;
    wstring::size_type end = line.find(L' ', start);
    if (end == wstring::npos) return false;
    trad.assign(line, start, end);
    start = line.find(L'[', end);
    if (start == wstring::npos) return false;
    end = line.find(L']', ++start);
    if (end == wstring::npos) return false;
    pinyin.assign(line, start, end - start);
    start = line.find(L'/', end);
    if (start == wstring::npos) return false;
    start++;
    end = line.rfind(L'/');
    if (end == wstring::npos) return false;
    if (end <= start) return false;
    english.assign(line, start, end - start);
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
    std::wifstream src("cedict.u8");
    src.imbue(std::locale(src.getloc(), new std::codecvt_utf8_utf16<wchar_t>));

    wstring s;
    while (getline(src, s)) {
        if (s.length() > 0 && s[0] != L'#') {
            DictionaryEntry de;
            if (de.Parse(s)) {
                v.push_back(de);
            }
        }
    }
}


int main()
{
    cout << "Loading Chinese English Dictionary\n";
    cout << "V.1: Uses C++ standard I/O streams, codecvt and STL wstring.\n\n";

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

