// Cricket IRC Client - the Wallpaper tab's engine (Haiku)
// Distributed under the terms of the MIT license.
//
// Picks one wallpaper picture from local folders or from an online source (no key needed) and keeps it in a
// cache folder. Blocking: call it from a thread of its own.
#ifndef CRICKET_WALLPAPER_H
#define CRICKET_WALLPAPER_H

#include <string>
#include <vector>

namespace CricketWallpaper {

struct Settings {
    std::vector<std::string> sources;   // any of SourceKeys()
    std::vector<std::string> folders;
    bool random;
    int index;                          // folders, in order: the next picture
    Settings() : random(true), index(0) {}
};

struct Result {
    bool ok;
    std::string path;                   // the picture (a file)
    std::string title;
    std::string credit;
    std::string error;
    int nextIndex;
    Result() : ok(false), nextIndex(0) {}
};

// "folders", "bing", "wikimedia", "wallhaven", "picsum", "artic"
const std::vector<std::string>& SourceKeys();
std::string SourceTitle(const std::string& key);
// The minutes between two pictures for each entry of the interval menu
const std::vector<int>& IntervalMinutes();
std::string IntervalTitle(int minutes);
// ~/config/cache/cricket/wallpapers
std::string CacheDir();
// The title and credit of a picture of the cache (empty for any other file)
void Describe(const std::string& path, std::string* title, std::string* credit);

Result PickOne(const Settings& settings, const std::string& currentPath);

}   // namespace CricketWallpaper

#endif
