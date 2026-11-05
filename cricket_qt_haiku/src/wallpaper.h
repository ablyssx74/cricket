/*
 * Cricket IRC Client - Haiku Qt port
 * Distributed under the terms of the MIT license.
 */
#ifndef CRICKET_WALLPAPER_H
#define CRICKET_WALLPAPER_H

#include <QObject>
#include <QStringList>
#include <QVector>
#include <functional>

// Picks one wallpaper picture from local folders or from an online source and puts it in a cache folder.
namespace Wallpaper {

struct Settings {
    QStringList sources;    // any of sourceKeys()
    QStringList folders;    // folders with pictures (used when "folders" is one of the sources)
    bool random = true;     // folders: random order (off: in order)
    int index = 0;          // folders, in order: the next picture
};

struct Result {
    bool ok = false;
    QString path;           // the picture (a file)
    QString title;
    QString credit;
    QString error;
    int nextIndex = 0;
};

// "folders", "bing", "wikimedia", "wallhaven", "picsum", "artic"
const QStringList& sourceKeys();
QString sourceTitle(const QString& key);
// The minutes between pictures the interval setting stands for
const QVector<int>& intervalMinutes();
QString cacheDir();
// The title and credit of a picture of the cache (empty for other files)
void describe(const QString& path, QString* title, QString* credit);

// Picks and gets one picture (in a worker thread). done is called in the thread of `context`.
// currentPath: the picture shown now, so that the next one is another one.
void next(QObject* context, const Settings& settings, const QString& currentPath, std::function<void(const Result&)> done);

}   // namespace Wallpaper

#endif
