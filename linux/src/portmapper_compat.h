/*
 * Cricket IRC Client - Linux port
 * Distributed under the terms of the MIT license.
 *
 * Just enough of the Haiku API for portmapper.cpp (adapted from the Haiku
 * build's portmapper/PortMapper.cpp) to compile on Linux unchanged in
 * spirit: a BString work-alike on std::string, BLocker/BAutolock on
 * std::mutex, and the kernel time/sleep calls.
 */
#ifndef CRICKET_PORTMAPPER_COMPAT_H
#define CRICKET_PORTMAPPER_COMPAT_H

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <strings.h>
#include <thread>

namespace cricket {

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef int32_t int32;
typedef uint32_t uint32;
typedef int64_t int64;
typedef uint64_t uint64;
typedef int64_t bigtime_t;
typedef int32_t status_t;

static const status_t B_OK = 0;
static const status_t B_NO_ERROR = 0;

inline bigtime_t system_time()
{
	using namespace std::chrono;
	return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}

inline void snooze(bigtime_t micros)
{
	std::this_thread::sleep_for(std::chrono::microseconds(micros));
}

// The subset of Haiku's BString that PortMapper uses.
class BString {
public:
	BString() {}
	BString(const char* s) : fStr(s ? s : "") {}
	BString(const std::string& s) : fStr(s) {}

	const char* String() const { return fStr.c_str(); }
	int32 Length() const { return (int32)fStr.size(); }
	char operator[](int32 i) const { return fStr[i]; }

	int32 FindFirst(const char* s, int32 from = 0) const { return Pos(fStr.find(s, Clamp(from))); }
	int32 FindFirst(const BString& s, int32 from = 0) const { return FindFirst(s.String(), from); }
	int32 FindFirst(char c, int32 from = 0) const { return Pos(fStr.find(c, Clamp(from))); }
	int32 IFindFirst(const char* s, int32 from = 0) const
	{
		std::string hay = Lowered(fStr), needle = Lowered(s);
		return Pos(hay.find(needle, Clamp(from)));
	}

	BString& CopyInto(BString& into, int32 from, int32 length) const
	{
		into.fStr = fStr.substr(Clamp(from), length < 0 ? 0 : (size_t)length);
		return into;
	}
	BString& ToLower()
	{
		fStr = Lowered(fStr);
		return *this;
	}
	BString& Trim()
	{
		size_t a = fStr.find_first_not_of(" \t\r\n");
		if (a == std::string::npos) {
			fStr.clear();
			return *this;
		}
		size_t b = fStr.find_last_not_of(" \t\r\n");
		fStr = fStr.substr(a, b - a + 1);
		return *this;
	}
	bool StartsWith(const char* prefix) const { return fStr.rfind(prefix, 0) == 0; }

	BString& operator=(const char* s) { fStr = s ? s : ""; return *this; }
	BString& operator+=(const char* s) { fStr += s; return *this; }
	BString& operator+=(const BString& s) { fStr += s.fStr; return *this; }
	BString& operator<<(const char* s) { fStr += s; return *this; }
	BString& operator<<(const BString& s) { fStr += s.fStr; return *this; }
	bool operator==(const BString& o) const { return fStr == o.fStr; }
	bool operator!=(const BString& o) const { return fStr != o.fStr; }

	const std::string& Std() const { return fStr; }

private:
	static std::string Lowered(std::string s)
	{
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)tolower(c); });
		return s;
	}
	size_t Clamp(int32 from) const { return from < 0 ? 0 : (size_t)from; }
	static int32 Pos(size_t p) { return p == std::string::npos ? -1 : (int32)p; }

	std::string fStr;
};

class BLocker {
public:
	explicit BLocker(const char* = nullptr) {}
	void Lock() { fMutex.lock(); }
	void Unlock() { fMutex.unlock(); }

private:
	std::recursive_mutex fMutex;
};

class BAutolock {
public:
	explicit BAutolock(BLocker& locker) : fLocker(locker) { fLocker.Lock(); }
	~BAutolock() { fLocker.Unlock(); }

private:
	BLocker& fLocker;
};

}  // namespace cricket

#endif
