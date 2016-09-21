# The Chinese Dictionary Loading Benchmark Revised

## Loading the CEDICT Chinese/English dictionary with C++, including Windows-specific optimizations.

By Giovanni Dicanio  
2016, September 21st

In 2005, on [“The Old New Thing” blog](https://blogs.msdn.microsoft.com/oldnewthing/), an interesting series on building a Chinese/English dictionary was [published](https://blogs.msdn.microsoft.com/oldnewthing/20050510-55/?p=35673).

I created a Visual Studio 2015 solution containing several iterations of the dictionary loading program that appeared in that series. In addition, out of curiosity, I also added some code to test ATL’s CString (more precisely: CStringW) instead of STL’s wstring.

I built the code with VS2015 with Update 3, in 32-bit release mode, and executed it on an Intel i7-6700 @ 3.40GHz, 32GB of RAM and Windows 10.
The dictionary file I used for the tests can be downloaded from [here](http://www.mdbg.net/chindict/chindict.php?page=cc-cedict). I also included a copy of it in this repository. Note that, when running the tests, the dictionary file must be in the same directory containing the executable files.

The relase date of this CEDICT dictionary file is 2016-09-11; this version of the dictionary has 114,822 entries.

These are the results I got:

| Test  | Time excluding destructors [ms] | Time including destructors [ms] |
| ----- |:-------------------------------:|:-------------------------------:|
| #1: Uses C++ standard **I/O streams**, **codecvt** and **STL wstring**. | 641 | 651 |
| #2: Uses **memory-mapped files, MultiByteToWideChar** and STL wstring. | 105 | 118 |
| #2A: Uses memory-mapped files, MultiByteToWideChar and **ATL CStringW**. | 111 | 127 |
| #3: Uses memory-mapped files, MultiByteToWideChar and **raw C-style strings** (with the default allocator, i.e. new[]). | 58 | 73 |
| #4: Uses memory-mapped files, MultiByteToWideChar and **custom pool string allocator**. | 50 | 50 |

In this table I compare my times with those published on the original blog series, written in parentheses:

| Test  | Time excluding destructors [ms] | Time including destructors [ms] |
| ----- |:----------------------------------------------------:|:----------------------------------------------------:|
| #1: Uses C++ standard **I/O streams**, **codecvt** and **STL wstring**. ([Original](https://blogs.msdn.microsoft.com/oldnewthing/20050510-55/?p=35673)) | 641 (2080) | 651 (2140) |
| #2: Uses **memory-mapped files, MultiByteToWideChar** and STL wstring. ([Original](https://blogs.msdn.microsoft.com/oldnewthing/20050516-30/?p=35633)) | 105 (240) | 118 (300) |
| #2A: Uses memory-mapped files, MultiByteToWideChar and **ATL CStringW**. | 111 (N/A) | 127 (N/A) |
| #3: Uses memory-mapped files, MultiByteToWideChar and **raw C-style strings** (with the default allocator, i.e. new[]). ([Original](https://blogs.msdn.microsoft.com/oldnewthing/20050518-42/?p=35613)) | 58 (120) | 73 (290) |
| #4: Uses memory-mapped files, MultiByteToWideChar and **custom pool string allocator**. ([Original](https://blogs.msdn.microsoft.com/oldnewthing/20050519-00/?p=35603)) | 50 (70) | 50 (80) |

Reading those numbers, I draw the following conclusions:

* The C++ standard I/O streams and locale/codecvt for UTF-8 to UTF-16 conversions are _very slow_ (#1). 

  As soon as the implementation changes to memory-mapped files and Win32 MultiByteToWideChar (#2), the performance jumps from a total time of 650 ms to less than 120 ms. (The original numbers published in The Old New Thing blog are 240 ms, and 300 ms including destructors.) Basically, running this code today, compiled with VS2015 on an Intel i7-6700 CPU, we get less than _half_ the execution times that the author got in 2005, e.g. 105 ms of dictionary loading time today vs. 240 ms back in 2005.
  
  I don’t know what hardware was used for testing the original code in 2005, but I think that C++11 improvements like move semantics play an important role in the increase of STL string performance.
  
* The original author’s target was **100 ms** of dictionary loading time, and our #2 test with 105 ms is just fine to me (note also that there can be small fluctuations at the millisecond level around those measured values).

* STL’s wstring performs slightly better than ATL’s CString (comparing #2 vs. #2A), but the difference is less than 10 ms!

  However, in other tests, when _small strings_ are involved, I measured much more significant improvements of STL’s strings vs. ATL CoW-based CString thanks to STL’s small string optimization (SSO).
  
* There are significant improvements when raw C-style string pointers are used instead of wstring: from 105 ms of test #2 to 58 ms of test #3 (almost half-time). However, with wstring we already almost met the 100 ms target. Unless further optimizations are required, using raw C-style string pointers is not worth the inconvenience.

* Using a custom string pool allocator (#4) reduces the time from the raw string pointer case (#3) by just 8 ms if we exclude destructors, i.e. from 58 ms of loading time to 50 ms. (If we include destructors, the performance increase is slightly better, with execution time reduced from 73 ms of test #3 to 50 ms with a custom pool allocator.) So, in cases like this, I think a custom memory allocator is not worth the code complication.

*	Using raw C-style string pointers and the _default_ memory allocator (I think C++ new[] calls C malloc which calls Win32 HeapAlloc) as in #3 results today in better (shorter) times than those obtained with a custom pool allocator and published in 2005. In fact, the best times in the original 2005 series were 70 ms and 80 ms including destructors, and those times were obtained with a custom pool allocator; instead today I got 58 ms (73 ms with destructors) with the _default_ memory allocator.

All in all, I’d be happy with the optimization level reached in #2: Ditch C++ standard I/O streams and locale/codecvt in favor of memory-mapped files for reading files and MultiByteToWideChar Win32 API for UTF-8 to UTF-16 conversions, but just continue using the STL’s wstring class!
