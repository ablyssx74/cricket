// Cricket IRC Client - the Wallpaper tab's engine (Haiku)
// Distributed under the terms of the MIT license.
#include "wallpaper.h"

#include <curl/curl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <random>

#include "nlohmann/json.hpp"

namespace CricketWallpaper {

using json = nlohmann::json;

namespace {

struct Picture {
    std::string id;         // the name of the cached file, without its extension
    std::string url;
    std::string fallback;   // another address, if the first one is refused
    std::string title;
    std::string credit;
};

const size_t kMaxDownload = 80u * 1024 * 1024;
const size_t kKeepPictures = 15;

std::mt19937& Rng()
{
    static std::mt19937 rng(static_cast<unsigned>(time(nullptr)) ^ static_cast<unsigned>(getpid()));
    return rng;
}

int RandomBelow(int n)
{
    if (n <= 1)
        return 0;
    return static_cast<int>(Rng()() % static_cast<unsigned>(n));
}

// --- downloading ---
size_t Append(char* data, size_t size, size_t count, void* user)
{
    std::string* text = static_cast<std::string*>(user);
    if (text->size() + size * count > kMaxDownload)
        return 0;
    text->append(data, size * count);
    return size * count;
}

bool HttpGet(const std::string& url, std::string* body, long timeout = 30)
{
    CURL* curl = curl_easy_init();
    if (curl == nullptr)
        return false;
    body->clear();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Append);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, body);
    // (Wikimedia asks for a User-Agent that says who is asking; the Art Institute turns one that has an address in it away)
    curl_easy_setopt(curl, CURLOPT_USERAGENT,
        url.find("wikimedia.org") != std::string::npos ? "cricket-irc-client (https://github.com/ablyssx74/cricket)" : "cricket");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    const CURLcode result = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);
    return result == CURLE_OK && status == 200 && !body->empty();
}

json GetJson(const std::string& url)
{
    std::string body;
    if (!HttpGet(url, &body))
        return json();
    return json::parse(body, nullptr, false);
}

std::string DateString(time_t when, const char* format)
{
    char text[32];
    struct tm local;
    localtime_r(&when, &local);
    strftime(text, sizeof(text), format, &local);
    return text;
}

std::string Str(const json& object, const char* key)
{
    if (!object.is_object())
        return std::string();
    json::const_iterator it = object.find(key);
    if (it == object.end() || !it->is_string())
        return std::string();
    return it->get<std::string>();
}

long Num(const json& object, const char* key)
{
    if (!object.is_object())
        return 0;
    json::const_iterator it = object.find(key);
    if (it == object.end() || !it->is_number())
        return 0;
    return it->get<long>();
}

const json& Member(const json& object, const char* key)
{
    static const json none;
    if (!object.is_object())
        return none;
    json::const_iterator it = object.find(key);
    return it == object.end() ? none : *it;
}

// --- the online sources (the ones that need no key) ---
std::vector<Picture> ListBing()
{
    std::vector<Picture> list;
    const json j = GetJson("https://www.bing.com/HPImageArchive.aspx?format=js&idx=0&n=4&mkt=en-US");
    const json& images = Member(j, "images");
    if (!images.is_array())
        return list;
    for (size_t i = 0; i < images.size(); i++) {
        const json& o = images[i];
        const std::string base = Str(o, "urlbase");
        if (base.empty())
            continue;
        Picture p;
        p.id = "bing-" + Str(o, "startdate");
        p.url = "https://www.bing.com" + base + "_1920x1080.jpg";
        p.title = Str(o, "title");
        const std::string copyright = Str(o, "copyright");
        p.credit = copyright.empty() ? std::string("Bing") : copyright + " (Bing)";
        list.push_back(p);
    }
    return list;
}

std::vector<Picture> ListWikimedia()
{
    std::vector<Picture> list;
    for (int back = 0; back < 3; back++) {
        const time_t when = time(nullptr) - back * 86400;
        const json j = GetJson("https://api.wikimedia.org/feed/v1/wikipedia/en/featured/" + DateString(when, "%Y/%m/%d"));
        const json& image = Member(j, "image");
        const std::string thumb = Str(Member(image, "thumbnail"), "source");
        const std::string original = Str(Member(image, "image"), "source");
        if (thumb.empty() && original.empty())
            continue;
        Picture p;
        p.id = "wikimedia-" + DateString(when, "%Y-%m-%d");
        // (the thumbnail's address names its width: ask for 1920)
        const size_t px = thumb.find("/960px-");
        p.url = px != std::string::npos ? thumb.substr(0, px) + "/1920px-" + thumb.substr(px + 7) : thumb;
        p.fallback = original;
        const std::string title = Str(image, "title");
        p.title = title.compare(0, 5, "File:") == 0 ? title.substr(5) : title;
        p.credit = "Wikimedia Commons, Picture of the Day";
        list.push_back(p);
    }
    return list;
}

std::vector<Picture> ListWallhaven()
{
    std::vector<Picture> list;
    const json j = GetJson("https://wallhaven.cc/api/v1/search?categories=100&purity=100&sorting=toplist&topRange=1d"
                           "&ratios=16x9,16x10&atleast=1920x1080");
    const json& data = Member(j, "data");
    if (!data.is_array())
        return list;
    const std::string today = DateString(time(nullptr), "%Y%m%d");
    for (size_t i = 0; i < data.size() && list.size() < 6; i++) {
        const std::string id = Str(data[i], "id"), path = Str(data[i], "path");
        if (id.empty() || path.empty())
            continue;
        Picture p;
        p.id = "wallhaven-" + today + "-" + id;
        p.url = path;
        p.title = "Wallhaven " + id;
        const std::string page = Str(data[i], "url");
        p.credit = "wallhaven.cc" + (page.empty() ? std::string() : " - " + page);
        list.push_back(p);
    }
    return list;
}

std::vector<Picture> ListPicsum()
{
    std::vector<Picture> list;
    char query[64];
    snprintf(query, sizeof(query), "page=%d&limit=40", 1 + RandomBelow(20));
    const json j = GetJson(std::string("https://picsum.photos/v2/list?") + query);
    if (!j.is_array())
        return list;
    const std::string today = DateString(time(nullptr), "%Y%m%d");
    std::vector<size_t> good;
    for (size_t i = 0; i < j.size(); i++) {
        const long width = Num(j[i], "width"), height = Num(j[i], "height");
        if (width >= 3000 && height > 0 && width > height * 13 / 10)
            good.push_back(i);
    }
    while (list.size() < 4 && !good.empty()) {
        const size_t pick = static_cast<size_t>(RandomBelow(static_cast<int>(good.size())));
        const json& o = j[good[pick]];
        good.erase(good.begin() + pick);
        const std::string id = Str(o, "id"), author = Str(o, "author");
        if (id.empty())
            continue;
        Picture p;
        p.id = "picsum-" + today + "-" + id;
        p.url = "https://picsum.photos/id/" + id + "/1920/1080.jpg";
        p.title = "Picsum Photos";
        p.credit = "Photo by " + (author.empty() ? std::string("a photographer") : author) + " (Picsum Photos)";
        list.push_back(p);
    }
    return list;
}

std::vector<Picture> ListArtic()
{
    std::vector<Picture> list;
    char query[96];
    snprintf(query, sizeof(query), "&limit=40&page=%d", 1 + RandomBelow(20));
    const json j = GetJson(std::string("https://api.artic.edu/api/v1/artworks/search?q=landscape"
                                       "&query%5Bterm%5D%5Bis_public_domain%5D=true"
                                       "&fields=id,title,artist_display,image_id,thumbnail") + query);
    const json& data = Member(j, "data");
    if (!data.is_array())
        return list;
    const std::string today = DateString(time(nullptr), "%Y%m%d");
    std::vector<size_t> good;
    for (size_t i = 0; i < data.size(); i++) {
        const json& thumb = Member(data[i], "thumbnail");
        const long width = Num(thumb, "width"), height = Num(thumb, "height");
        if (!Str(data[i], "image_id").empty() && width >= 2500 && height > 0 && width > height * 13 / 10)
            good.push_back(i);
    }
    while (list.size() < 4 && !good.empty()) {
        const size_t pick = static_cast<size_t>(RandomBelow(static_cast<int>(good.size())));
        const json& o = data[good[pick]];
        good.erase(good.begin() + pick);
        const std::string image = Str(o, "image_id");
        std::string artist = Str(o, "artist_display");
        const size_t newline = artist.find('\n');
        if (newline != std::string::npos)
            artist = artist.substr(0, newline);
        Picture p;
        p.id = "artic-" + today + "-" + image.substr(0, 8);
        p.url = "https://www.artic.edu/iiif/2/" + image + "/full/1686,/0/default.jpg";
        p.title = Str(o, "title").empty() ? std::string("Art Institute of Chicago") : Str(o, "title");
        p.credit = (artist.empty() ? std::string("Unknown artist") : artist) + " - Art Institute of Chicago (public domain)";
        list.push_back(p);
    }
    return list;
}

// --- files ---
bool FileExists(const std::string& path, off_t* size = nullptr, time_t* mtime = nullptr)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
        return false;
    if (size != nullptr) *size = st.st_size;
    if (mtime != nullptr) *mtime = st.st_mtime;
    return true;
}

void MakeDirectories(const std::string& path)
{
    for (size_t i = 1; i <= path.size(); i++) {
        if (i == path.size() || path[i] == '/')
            mkdir(path.substr(0, i).c_str(), 0755);
    }
}

std::string Extension(const std::string& data)
{
    if (data.size() < 12)
        return std::string();
    const unsigned char* b = reinterpret_cast<const unsigned char*>(data.data());
    if (b[0] == 0xFF && b[1] == 0xD8) return "jpg";
    if (b[0] == 0x89 && b[1] == 'P') return "png";
    if (b[0] == 'R' && b[1] == 'I' && b[2] == 'F') return "webp";
    return std::string();
}

std::string ExistingCached(const std::string& id)
{
    static const char* const kExtensions[] = {"jpg", "png", "webp"};
    for (size_t i = 0; i < 3; i++) {
        const std::string file = CacheDir() + "/" + id + "." + kExtensions[i];
        off_t size = 0;
        if (FileExists(file, &size) && size > 0)
            return file;
    }
    return std::string();
}

void WriteNote(const std::string& file, const Picture& p)
{
    if (p.title.empty() && p.credit.empty())
        return;
    if (FILE* f = fopen((file + ".txt").c_str(), "w")) {
        fprintf(f, "%s\n%s\n", p.title.c_str(), p.credit.c_str());
        fclose(f);
    }
}

// The picture as a file in the cache (downloaded unless it is there already); empty: it could not be had.
std::string Download(const Picture& p)
{
    MakeDirectories(CacheDir());
    std::string file = ExistingCached(p.id);
    if (!file.empty()) {
        WriteNote(file, p);
        return file;
    }
    const std::string urls[2] = {p.url, p.fallback};
    for (size_t i = 0; i < 2; i++) {
        if (urls[i].empty())
            continue;
        std::string data;
        if (!HttpGet(urls[i], &data, 60))
            continue;
        const std::string ext = Extension(data);
        if (ext.empty())
            continue;
        file = CacheDir() + "/" + p.id + "." + ext;
        FILE* out = fopen(file.c_str(), "wb");
        if (out == nullptr)
            continue;
        const bool written = fwrite(data.data(), 1, data.size(), out) == data.size();
        fclose(out);
        if (!written) {
            remove(file.c_str());
            continue;
        }
        WriteNote(file, p);
        return file;
    }
    return std::string();
}

bool HasPictureExtension(const std::string& name)
{
    const size_t dot = name.rfind('.');
    if (dot == std::string::npos)
        return false;
    std::string ext = name.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(tolower(c)); });
    return ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "webp" || ext == "bmp";
}

// Keeps the newest pictures of the cache (and the one on show), removes the others
void Prune(const std::string& keep)
{
    DIR* dir = opendir(CacheDir().c_str());
    if (dir == nullptr)
        return;
    std::vector<std::pair<time_t, std::string> > files;
    while (struct dirent* entry = readdir(dir)) {
        const std::string name = entry->d_name;
        if (!HasPictureExtension(name))
            continue;
        const std::string path = CacheDir() + "/" + name;
        time_t mtime = 0;
        if (FileExists(path, nullptr, &mtime))
            files.push_back(std::make_pair(mtime, path));
    }
    closedir(dir);
    std::sort(files.begin(), files.end(), [](const std::pair<time_t, std::string>& a, const std::pair<time_t, std::string>& b) {
        return a.first > b.first;
    });
    for (size_t i = kKeepPictures; i < files.size(); i++) {
        if (files[i].second == keep)
            continue;
        remove(files[i].second.c_str());
        remove((files[i].second + ".txt").c_str());
    }
}

void CollectPictures(const std::string& folder, int depth, std::vector<std::string>* files)
{
    DIR* dir = opendir(folder.c_str());
    if (dir == nullptr)
        return;
    while (struct dirent* entry = readdir(dir)) {
        const std::string name = entry->d_name;
        if (name == "." || name == "..")
            continue;
        const std::string path = folder + "/" + name;
        struct stat st;
        if (stat(path.c_str(), &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode)) {
            if (depth < 6)
                CollectPictures(path, depth + 1, files);
        } else if (S_ISREG(st.st_mode) && HasPictureExtension(name)) {
            files->push_back(path);
        }
    }
    closedir(dir);
}

bool FromFolders(const Settings& s, const std::string& current, Result* r)
{
    std::vector<std::string> files;
    for (size_t i = 0; i < s.folders.size(); i++)
        CollectPictures(s.folders[i], 0, &files);
    std::sort(files.begin(), files.end());
    files.erase(std::unique(files.begin(), files.end()), files.end());
    if (files.empty()) {
        r->error = "There are no pictures in the folders";
        return false;
    }
    std::string pick;
    const int count = static_cast<int>(files.size());
    if (s.random) {
        for (int tries = 0; tries < 8; tries++) {
            pick = files[RandomBelow(count)];
            if (count < 2 || pick != current)
                break;
        }
    } else {
        const int at = ((s.index % count) + count) % count;
        pick = files[at];
        r->nextIndex = (at + 1) % count;
    }
    r->path = pick;
    const size_t slash = pick.rfind('/');
    r->title = slash == std::string::npos ? pick : pick.substr(slash + 1);
    r->credit.clear();
    return true;
}

std::string BaseName(const std::string& path)
{
    const size_t slash = path.rfind('/');
    std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
    const size_t dot = name.rfind('.');
    return dot == std::string::npos ? name : name.substr(0, dot);
}

bool FromOnline(const std::string& key, const std::string& current, Result* r)
{
    std::vector<Picture> list;
    if (key == "bing") list = ListBing();
    else if (key == "wikimedia") list = ListWikimedia();
    else if (key == "wallhaven") list = ListWallhaven();
    else if (key == "picsum") list = ListPicsum();
    else if (key == "artic") list = ListArtic();
    // (not the one on show, if there is another)
    const std::string shown = BaseName(current);
    std::vector<Picture> others;
    for (size_t i = 0; i < list.size(); i++)
        if (list[i].id != shown)
            others.push_back(list[i]);
    if (!others.empty())
        list = others;
    while (!list.empty()) {
        const size_t pick = static_cast<size_t>(RandomBelow(static_cast<int>(list.size())));
        const Picture p = list[pick];
        list.erase(list.begin() + pick);
        const std::string file = Download(p);
        if (!file.empty()) {
            r->path = file;
            r->title = p.title;
            r->credit = p.credit;
            return true;
        }
    }
    r->error = SourceTitle(key) + ": no picture could be had";
    return false;
}

}   // namespace

const std::vector<std::string>& SourceKeys()
{
    static std::vector<std::string> keys;
    if (keys.empty()) {
        keys.push_back("folders");
        keys.push_back("bing");
        keys.push_back("wikimedia");
        keys.push_back("wallhaven");
        keys.push_back("picsum");
        keys.push_back("artic");
    }
    return keys;
}

std::string SourceTitle(const std::string& key)
{
    if (key == "folders") return "My folders";
    if (key == "bing") return "Bing picture of the day";
    if (key == "wikimedia") return "Wikimedia Commons picture of the day";
    if (key == "wallhaven") return "Wallhaven (top of the day)";
    if (key == "picsum") return "Picsum Photos";
    if (key == "artic") return "Art Institute of Chicago";
    return key;
}

const std::vector<int>& IntervalMinutes()
{
    static std::vector<int> minutes;
    if (minutes.empty()) {
        const int values[] = {1, 5, 15, 30, 60, 360, 1440};
        minutes.assign(values, values + 7);
    }
    return minutes;
}

std::string IntervalTitle(int minutes)
{
    char text[48];
    if (minutes == 1) return "Every minute";
    if (minutes < 60) { snprintf(text, sizeof(text), "Every %d minutes", minutes); return text; }
    if (minutes == 60) return "Every hour";
    if (minutes < 1440) { snprintf(text, sizeof(text), "Every %d hours", minutes / 60); return text; }
    return "Every day";
}

std::string CacheDir()
{
    const char* home = getenv("HOME");
    return std::string(home != nullptr ? home : "/boot/home") + "/config/cache/cricket/wallpapers";
}

void Describe(const std::string& path, std::string* title, std::string* credit)
{
    title->clear();
    credit->clear();
    const std::string dir = CacheDir() + "/";
    if (path.compare(0, dir.size(), dir) != 0)
        return;
    FILE* f = fopen((path + ".txt").c_str(), "r");
    if (f == nullptr)
        return;
    char line[1024];
    if (fgets(line, sizeof(line), f) != nullptr) {
        *title = line;
        while (!title->empty() && (title->back() == '\n' || title->back() == '\r')) title->pop_back();
    }
    if (fgets(line, sizeof(line), f) != nullptr) {
        *credit = line;
        while (!credit->empty() && (credit->back() == '\n' || credit->back() == '\r')) credit->pop_back();
    }
    fclose(f);
}

Result PickOne(const Settings& s, const std::string& current)
{
    static bool curlReady = (curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK);
    (void)curlReady;
    std::vector<std::string> order = s.sources;
    if (order.empty())
        order.push_back("folders");
    for (size_t i = order.size(); i > 1; i--)
        std::swap(order[i - 1], order[static_cast<size_t>(RandomBelow(static_cast<int>(i)))]);
    std::string errors;
    for (size_t i = 0; i < order.size(); i++) {
        Result attempt;
        attempt.nextIndex = s.index;
        const bool ok = order[i] == "folders" ? FromFolders(s, current, &attempt) : FromOnline(order[i], current, &attempt);
        if (ok) {
            attempt.ok = true;
            Prune(attempt.path);
            return attempt;
        }
        if (!attempt.error.empty())
            errors += (errors.empty() ? "" : "; ") + attempt.error;
    }
    Result failed;
    failed.error = errors.empty() ? std::string("No picture could be had") : errors;
    failed.nextIndex = s.index;
    return failed;
}

}   // namespace CricketWallpaper
