/*
 * Cricket IRC Client - Linux port
 * Distributed under the terms of the MIT license.
 */
#include "services.h"

#include "config.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>

#ifdef CRICKET_HAVE_DBUS
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#endif

#ifdef CRICKET_HAVE_ASPELL
#include <aspell.h>
#endif

// ---------------------------------------------------------------------------
// Translator
// ---------------------------------------------------------------------------

Translator::Translator(QObject* parent) : QObject(parent) {}

void Translator::translate(const QString& text, const QString& targetLanguage, const QString& model,
    const QString& apiKey, std::function<void(bool, const QString&)> done)
{
    QUrl url(QString("https://generativelanguage.googleapis.com/v1beta/models/%1:generateContent")
        .arg(QString(QUrl::toPercentEncoding(model))));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("x-goog-api-key", apiKey.toUtf8());
    req.setTransferTimeout(20000);

    QJsonObject body {
        {"contents", QJsonArray {QJsonObject {{"parts", QJsonArray {QJsonObject {{"text", text}}}}}}},
        {"systemInstruction", QJsonObject {{"parts", QJsonArray {QJsonObject {{"text",
            QString("Translate the input chat message to %1. Keep the tone identical. "
                    "Respond ONLY with the translation.").arg(targetLanguage)}}}}}},
    };

    QNetworkReply* reply = fNam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [reply, done]() {
        reply->deleteLater();
        QByteArray data = reply->readAll();
        if (cfg.debugEnable)
            qDebug().noquote() << "[GeminiDebug]" << reply->error() << data;
        QJsonObject root = QJsonDocument::fromJson(data).object();
        QJsonArray parts = root.value("candidates").toArray().at(0).toObject()
            .value("content").toObject().value("parts").toArray();
        QString out;
        for (const QJsonValue& p : parts)
            out += p.toObject().value("text").toString();
        out = out.trimmed();
        out.replace('\n', ' ');
        done(!out.isEmpty(), out);
    });
}

// ---------------------------------------------------------------------------
// UpdateChecker
// ---------------------------------------------------------------------------

UpdateChecker::UpdateChecker(QObject* parent) : QObject(parent) {}

void UpdateChecker::checkLater(int delayMs)
{
    QTimer::singleShot(delayMs, this, &UpdateChecker::check);
}

static int flattenVersion(const QString& s)
{
    static const QRegularExpression re(R"((\d+)\.(\d+)\.(\d+))");
    auto m = re.match(s);
    if (!m.hasMatch())
        return 0;
    return m.captured(1).toInt() * 10000 + m.captured(2).toInt() * 100 + m.captured(3).toInt();
}

void UpdateChecker::check()
{
    QNetworkRequest req(QUrl("https://raw.githubusercontent.com/ablyssx74/cricket/refs/heads/main/VERSION"));
    req.setHeader(QNetworkRequest::UserAgentHeader, "Cricket-Update-Checker/1.0");
    req.setTransferTimeout(10000);
    QNetworkReply* reply = fNam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        QString remote = QString::fromUtf8(reply->readAll()).trimmed();
        if (remote.isEmpty())
            return;
        if (flattenVersion(remote) > flattenVersion(AppInfo::VERSION_NUMBER))
            emit updateAvailable(remote);
    });
}

// ---------------------------------------------------------------------------
// Notifier
// ---------------------------------------------------------------------------

void Notifier::notify(const QString& title, const QString& body)
{
#ifdef CRICKET_HAVE_DBUS
    QDBusMessage msg = QDBusMessage::createMethodCall("org.freedesktop.Notifications",
        "/org/freedesktop/Notifications", "org.freedesktop.Notifications", "Notify");
    QVariantMap hints;
    hints["desktop-entry"] = QString("cricket");
    msg << QString("Cricket IRC") << uint(0) << QString("cricket") << title << body
        << QStringList() << hints << int(-1);
    QDBusConnection::sessionBus().asyncCall(msg);
#else
    Q_UNUSED(title);
    Q_UNUSED(body);
#endif
}

// ---------------------------------------------------------------------------
// SpellChecker
// ---------------------------------------------------------------------------

SpellChecker& SpellChecker::instance()
{
    static SpellChecker checker;
    return checker;
}

SpellChecker::SpellChecker()
{
    setLanguage(cfg.spellLanguage);
}

SpellChecker::~SpellChecker()
{
#ifdef CRICKET_HAVE_ASPELL
    if (fSpeller)
        delete_aspell_speller(static_cast<AspellSpeller*>(fSpeller));
#endif
}

bool SpellChecker::isAvailable() const
{
    return fSpeller != nullptr;
}

bool SpellChecker::setLanguage(const QString& lang)
{
#ifdef CRICKET_HAVE_ASPELL
    if (fSpeller && lang == fLanguage)
        return true;
    AspellConfig* config = new_aspell_config();
    aspell_config_replace(config, "lang", lang.toUtf8().constData());
    aspell_config_replace(config, "encoding", "utf-8");
    AspellCanHaveError* result = new_aspell_speller(config);
    delete_aspell_config(config);
    if (aspell_error_number(result) != 0) {
        delete_aspell_can_have_error(result);
        return false;
    }
    if (fSpeller)
        delete_aspell_speller(static_cast<AspellSpeller*>(fSpeller));
    fSpeller = to_aspell_speller(result);
    fLanguage = lang;
    return true;
#else
    Q_UNUSED(lang);
    return false;
#endif
}

bool SpellChecker::check(const QString& word)
{
#ifdef CRICKET_HAVE_ASPELL
    if (!fSpeller)
        return true;
    QByteArray w = word.toUtf8();
    return aspell_speller_check(static_cast<AspellSpeller*>(fSpeller), w.constData(), w.size()) == 1;
#else
    Q_UNUSED(word);
    return true;
#endif
}

QStringList SpellChecker::suggest(const QString& word)
{
    QStringList out;
#ifdef CRICKET_HAVE_ASPELL
    if (!fSpeller)
        return out;
    QByteArray w = word.toUtf8();
    const AspellWordList* list = aspell_speller_suggest(static_cast<AspellSpeller*>(fSpeller), w.constData(), w.size());
    AspellStringEnumeration* e = aspell_word_list_elements(list);
    const char* s;
    while ((s = aspell_string_enumeration_next(e)) != nullptr && out.size() < 8)
        out << QString::fromUtf8(s);
    delete_aspell_string_enumeration(e);
#else
    Q_UNUSED(word);
#endif
    return out;
}

void SpellChecker::addToPersonal(const QString& word)
{
#ifdef CRICKET_HAVE_ASPELL
    if (!fSpeller)
        return;
    QByteArray w = word.toUtf8();
    AspellSpeller* sp = static_cast<AspellSpeller*>(fSpeller);
    aspell_speller_add_to_personal(sp, w.constData(), w.size());
    aspell_speller_save_all_word_lists(sp);
#else
    Q_UNUSED(word);
#endif
}
