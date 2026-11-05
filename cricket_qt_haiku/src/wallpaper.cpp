/*
 * Cricket IRC Client - Haiku Qt port
 * Distributed under the terms of the MIT license.
 */
#include "wallpaper.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QThread>
#include <QUrl>

#include <algorithm>

namespace Wallpaper {

namespace {

struct Picture {
    QString id;         // the name of the cached file, without its extension
    QString url;
    QString fallback;   // another address, if the first one is refused
    QString title;
    QString credit;
};

const int kMaxDownload = 80 * 1024 * 1024;
const int kKeepPictures = 15;

bool httpGet(const QUrl& url, QByteArray* body, int timeoutMs)
{
    QNetworkAccessManager nam;
    QNetworkRequest request(url);
    // (Wikimedia asks for a User-Agent that says who is asking; the Art Institute turns one that has an address in it away)
    request.setHeader(QNetworkRequest::UserAgentHeader,
        url.host().endsWith("wikimedia.org") ? "cricket-irc-client (https://github.com/ablyssx74/cricket)" : "cricket");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(timeoutMs);
    QNetworkReply* reply = nam.get(request);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    const bool ok = reply->error() == QNetworkReply::NoError
        && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200;
    if (ok) {
        *body = reply->readAll();
        if (body->size() > kMaxDownload)
            body->clear();
    }
    reply->deleteLater();
    return ok && !body->isEmpty();
}

QJsonObject getObject(const QString& url, int timeoutMs = 30000)
{
    QByteArray body;
    if (!httpGet(QUrl(url), &body, timeoutMs))
        return {};
    return QJsonDocument::fromJson(body).object();
}

QJsonArray getArray(const QString& url, int timeoutMs = 30000)
{
    QByteArray body;
    if (!httpGet(QUrl(url), &body, timeoutMs))
        return {};
    return QJsonDocument::fromJson(body).array();
}

int randomBelow(int n) { return n <= 0 ? 0 : static_cast<int>(QRandomGenerator::global()->bounded(n)); }

QString day(int back, const QString& format)
{
    return QDateTime::currentDateTime().addDays(-back).toString(format);
}

// --- the online sources (the ones that need no key) ---

QVector<Picture> listBing()
{
    QVector<Picture> list;
    const QJsonObject json = getObject("https://www.bing.com/HPImageArchive.aspx?format=js&idx=0&n=4&mkt=en-US");
    for (const QJsonValue& v : json.value("images").toArray()) {
        const QJsonObject o = v.toObject();
        if (o.value("urlbase").toString().isEmpty())
            continue;
        Picture p;
        p.id = "bing-" + o.value("startdate").toString();
        p.url = "https://www.bing.com" + o.value("urlbase").toString() + "_1920x1080.jpg";
        p.title = o.value("title").toString();
        const QString copyright = o.value("copyright").toString();
        p.credit = copyright.isEmpty() ? QString("Bing") : copyright + " (Bing)";
        list << p;
    }
    return list;
}

QVector<Picture> listWikimedia()
{
    QVector<Picture> list;
    for (int back = 0; back < 3; back++) {
        const QJsonObject json = getObject("https://api.wikimedia.org/feed/v1/wikipedia/en/featured/" + day(back, "yyyy/MM/dd"));
        const QJsonObject image = json.value("image").toObject();
        QString thumb = image.value("thumbnail").toObject().value("source").toString();
        const QString original = image.value("image").toObject().value("source").toString();
        if (thumb.isEmpty() && original.isEmpty())
            continue;
        Picture p;
        p.id = "wikimedia-" + day(back, "yyyy-MM-dd");
        // (the thumbnail's address names its width: ask for 1920)
        const int px = thumb.indexOf("/960px-");
        p.url = px >= 0 ? thumb.left(px) + "/1920px-" + thumb.mid(px + 7) : thumb;
        p.fallback = original;
        QString title = image.value("title").toString();
        p.title = title.startsWith("File:") ? title.mid(5) : title;
        p.credit = "Wikimedia Commons, Picture of the Day";
        list << p;
    }
    return list;
}

QVector<Picture> listWallhaven()
{
    QVector<Picture> list;
    const QJsonObject json = getObject("https://wallhaven.cc/api/v1/search?categories=100&purity=100&sorting=toplist&topRange=1d"
                                       "&ratios=16x9,16x10&atleast=1920x1080");
    const QString today = day(0, "yyyyMMdd");
    for (const QJsonValue& v : json.value("data").toArray()) {
        if (list.size() >= 6)
            break;
        const QJsonObject o = v.toObject();
        const QString id = o.value("id").toString(), path = o.value("path").toString();
        if (id.isEmpty() || path.isEmpty())
            continue;
        Picture p;
        p.id = "wallhaven-" + today + "-" + id;
        p.url = path;
        p.title = "Wallhaven " + id;
        const QString page = o.value("url").toString();
        p.credit = "wallhaven.cc" + (page.isEmpty() ? QString() : " - " + page);
        list << p;
    }
    return list;
}

QVector<Picture> listPicsum()
{
    QVector<Picture> list;
    const QJsonArray json = getArray(QString("https://picsum.photos/v2/list?page=%1&limit=40").arg(1 + randomBelow(20)));
    const QString today = day(0, "yyyyMMdd");
    QVector<QJsonObject> good;
    for (const QJsonValue& v : json) {
        const QJsonObject o = v.toObject();
        const int width = o.value("width").toInt(), height = o.value("height").toInt();
        if (width >= 3000 && height > 0 && width > height * 13 / 10)
            good << o;
    }
    while (list.size() < 4 && !good.isEmpty()) {
        const QJsonObject o = good.takeAt(randomBelow(good.size()));
        const QString id = o.value("id").toString(), author = o.value("author").toString();
        if (id.isEmpty())
            continue;
        Picture p;
        p.id = "picsum-" + today + "-" + id;
        p.url = "https://picsum.photos/id/" + id + "/1920/1080.jpg";
        p.title = "Picsum Photos";
        p.credit = "Photo by " + (author.isEmpty() ? QString("a photographer") : author) + " (Picsum Photos)";
        list << p;
    }
    return list;
}

QVector<Picture> listArtic()
{
    QVector<Picture> list;
    const QJsonObject json = getObject(QString("https://api.artic.edu/api/v1/artworks/search?q=landscape"
                                               "&query%5Bterm%5D%5Bis_public_domain%5D=true"
                                               "&fields=id,title,artist_display,image_id,thumbnail&limit=40&page=%1")
                                           .arg(1 + randomBelow(20)));
    const QString today = day(0, "yyyyMMdd");
    QVector<QJsonObject> good;
    for (const QJsonValue& v : json.value("data").toArray()) {
        const QJsonObject o = v.toObject();
        const QJsonObject thumb = o.value("thumbnail").toObject();
        const int width = thumb.value("width").toInt(), height = thumb.value("height").toInt();
        if (!o.value("image_id").toString().isEmpty() && width >= 2500 && height > 0 && width > height * 13 / 10)
            good << o;
    }
    while (list.size() < 4 && !good.isEmpty()) {
        const QJsonObject o = good.takeAt(randomBelow(good.size()));
        const QString image = o.value("image_id").toString();
        QString artist = o.value("artist_display").toString();
        const int newline = artist.indexOf('\n');
        if (newline >= 0)
            artist = artist.left(newline);
        Picture p;
        p.id = "artic-" + today + "-" + image.left(8);
        p.url = "https://www.artic.edu/iiif/2/" + image + "/full/1686,/0/default.jpg";
        p.title = o.value("title").toString().isEmpty() ? QString("Art Institute of Chicago") : o.value("title").toString();
        p.credit = (artist.isEmpty() ? QString("Unknown artist") : artist) + " - Art Institute of Chicago (public domain)";
        list << p;
    }
    return list;
}

// The extension of a picture's data (empty: it is not a picture)
QString pictureExtension(const QByteArray& data)
{
    if (data.size() < 12)
        return QString();
    const uchar* b = reinterpret_cast<const uchar*>(data.constData());
    if (b[0] == 0xFF && b[1] == 0xD8)
        return "jpg";
    if (b[0] == 0x89 && b[1] == 'P')
        return "png";
    if (b[0] == 'R' && b[1] == 'I' && b[2] == 'F')
        return "webp";
    return QString();
}

QString existingCached(const QString& id)
{
    for (const char* ext : {"jpg", "png", "webp"}) {
        const QString file = cacheDir() + "/" + id + "." + ext;
        if (QFileInfo(file).size() > 0)
            return file;
    }
    return QString();
}

void writeNote(const QString& file, const Picture& p)
{
    if (p.title.isEmpty() && p.credit.isEmpty())
        return;
    QFile note(file + ".txt");
    if (note.open(QIODevice::WriteOnly | QIODevice::Truncate))
        note.write((p.title + "\n" + p.credit + "\n").toUtf8());
}

// The picture as a file in the cache (downloaded unless it is there already); empty: it could not be had.
QString download(const Picture& p)
{
    QDir().mkpath(cacheDir());
    QString file = existingCached(p.id);
    if (!file.isEmpty()) {
        writeNote(file, p);
        return file;
    }
    for (const QString& url : {p.url, p.fallback}) {
        if (url.isEmpty())
            continue;
        QByteArray data;
        if (!httpGet(QUrl(url), &data, 60000))
            continue;
        const QString ext = pictureExtension(data);
        if (ext.isEmpty())
            continue;
        file = cacheDir() + "/" + p.id + "." + ext;
        QFile out(file);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate) || out.write(data) != data.size()) {
            out.remove();
            continue;
        }
        out.close();
        writeNote(file, p);
        return file;
    }
    return QString();
}

// Keeps the newest pictures of the cache (and the one on show), removes the others
void prune(const QString& keep)
{
    QFileInfoList files = QDir(cacheDir()).entryInfoList({"*.jpg", "*.png", "*.webp"}, QDir::Files, QDir::Time);
    int n = 0;
    for (const QFileInfo& f : files) {
        if (++n <= kKeepPictures || f.absoluteFilePath() == keep)
            continue;
        QFile::remove(f.absoluteFilePath());
        QFile::remove(f.absoluteFilePath() + ".txt");
    }
}

QStringList collectPictures(const QStringList& folders)
{
    QStringList files;
    for (const QString& folder : folders) {
        QDirIterator it(folder, {"*.jpg", "*.jpeg", "*.png", "*.webp", "*.bmp"}, QDir::Files,
            QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
        while (it.hasNext())
            files << it.next();
    }
    files.removeDuplicates();
    files.sort();
    return files;
}

bool fromFolders(const Settings& s, const QString& current, Result* r)
{
    const QStringList files = collectPictures(s.folders);
    if (files.isEmpty()) {
        r->error = "There are no pictures in the folders";
        return false;
    }
    QString pick;
    if (s.random) {
        for (int tries = 0; tries < 8; tries++) {
            pick = files.at(randomBelow(files.size()));
            if (files.size() < 2 || pick != current)
                break;
        }
    } else {
        const int at = ((s.index % files.size()) + files.size()) % files.size();
        pick = files.at(at);
        r->nextIndex = (at + 1) % files.size();
    }
    r->path = pick;
    r->title = QFileInfo(pick).completeBaseName();
    r->credit.clear();
    return true;
}

bool fromOnline(const QString& key, const QString& current, Result* r)
{
    QVector<Picture> list;
    if (key == "bing") list = listBing();
    else if (key == "wikimedia") list = listWikimedia();
    else if (key == "wallhaven") list = listWallhaven();
    else if (key == "picsum") list = listPicsum();
    else if (key == "artic") list = listArtic();
    // (not the one on show, if there is another)
    const QString shown = QFileInfo(current).completeBaseName();
    QVector<Picture> others;
    for (const Picture& p : list)
        if (p.id != shown)
            others << p;
    if (!others.isEmpty())
        list = others;
    while (!list.isEmpty()) {
        const Picture p = list.takeAt(randomBelow(list.size()));
        const QString file = download(p);
        if (!file.isEmpty()) {
            r->path = file;
            r->title = p.title;
            r->credit = p.credit;
            return true;
        }
    }
    r->error = sourceTitle(key) + ": no picture could be had";
    return false;
}

Result pick(const Settings& s, const QString& current)
{
    Result result;
    QStringList order = s.sources.isEmpty() ? QStringList{"folders"} : s.sources;
    for (int i = order.size() - 1; i > 0; i--)
        order.swapItemsAt(i, randomBelow(i + 1));
    QString errors;
    for (const QString& key : order) {
        Result attempt;
        attempt.nextIndex = s.index;
        const bool ok = key == "folders" ? fromFolders(s, current, &attempt) : fromOnline(key, current, &attempt);
        if (ok) {
            attempt.ok = true;
            prune(attempt.path);
            return attempt;
        }
        if (!attempt.error.isEmpty())
            errors += (errors.isEmpty() ? "" : "; ") + attempt.error;
    }
    result.error = errors.isEmpty() ? QString("No picture could be had") : errors;
    result.nextIndex = s.index;
    return result;
}

}   // namespace

const QStringList& sourceKeys()
{
    static const QStringList keys = {"folders", "bing", "wikimedia", "wallhaven", "picsum", "artic"};
    return keys;
}

QString sourceTitle(const QString& key)
{
    if (key == "folders") return "My folders";
    if (key == "bing") return "Bing picture of the day";
    if (key == "wikimedia") return "Wikimedia Commons picture of the day";
    if (key == "wallhaven") return "Wallhaven (top of the day)";
    if (key == "picsum") return "Picsum Photos";
    if (key == "artic") return "Art Institute of Chicago";
    return key;
}

const QVector<int>& intervalMinutes()
{
    static const QVector<int> minutes = {1, 5, 15, 30, 60, 360, 1440};
    return minutes;
}

QString cacheDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/wallpapers";
}

void describe(const QString& path, QString* title, QString* credit)
{
    title->clear();
    credit->clear();
    if (!path.startsWith(cacheDir() + "/"))
        return;
    QFile note(path + ".txt");
    if (!note.open(QIODevice::ReadOnly))
        return;
    const QList<QByteArray> lines = note.readAll().split('\n');
    if (lines.size() > 0) *title = QString::fromUtf8(lines.at(0));
    if (lines.size() > 1) *credit = QString::fromUtf8(lines.at(1));
}

void next(QObject* context, const Settings& settings, const QString& currentPath, std::function<void(const Result&)> done)
{
    QThread* thread = QThread::create([=]() {
        const Result r = pick(settings, currentPath);
        QMetaObject::invokeMethod(context, [done, r]() { done(r); }, Qt::QueuedConnection);
    });
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

}   // namespace Wallpaper
