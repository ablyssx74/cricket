/*
 * Cricket IRC Client - Linux port
 * Distributed under the terms of the MIT license.
 */
#ifndef CRICKET_SERVICES_H
#define CRICKET_SERVICES_H

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

// Gemini live translation (inbound chat lines and outbound typed text).
class Translator : public QObject {
    Q_OBJECT
public:
    explicit Translator(QObject* parent = nullptr);

    // Calls done(ok, translatedText) on the UI thread when the request finishes.
    void translate(const QString& text, const QString& targetLanguage, const QString& model,
        const QString& apiKey, std::function<void(bool, const QString&)> done);

private:
    QNetworkAccessManager fNam;
};

// Checks the VERSION file on GitHub and notifies if a newer release exists.
class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(QObject* parent = nullptr);
    void checkLater(int delayMs = 5000);

signals:
    void updateAvailable(const QString& remoteVersion);

private:
    void check();
    QNetworkAccessManager fNam;
};

// Desktop notifications through org.freedesktop.Notifications (KDE Plasma,
// GNOME, etc.). Falls back to nothing if no notification server is running.
namespace Notifier {
void notify(const QString& title, const QString& body);
}

// Optional aspell-backed spell checker. Compiles to a no-op when aspell
// headers are not available at build time.
class SpellChecker {
public:
    static SpellChecker& instance();
    bool isAvailable() const;
    bool setLanguage(const QString& lang);
    bool check(const QString& word);
    QStringList suggest(const QString& word);
    void addToPersonal(const QString& word);

private:
    SpellChecker();
    ~SpellChecker();
    void* fSpeller = nullptr;
    QString fLanguage;
};

#endif
