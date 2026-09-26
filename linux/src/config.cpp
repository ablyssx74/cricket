/*
 * Cricket IRC Client - Linux port
 * Distributed under the terms of the MIT license.
 */
#include "config.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QStandardPaths>

namespace AppInfo {
const char* const VERSION_NUMBER = CRICKET_VERSION;
const char* const VERSION_STRING = "Cricket IRC Client v." CRICKET_VERSION " (Linux)";
}

Config cfg;

QString configDir()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    QString dir = base + "/cricket";
    QDir().mkpath(dir);
    return dir;
}

QString certsDir()
{
    QString dir = configDir() + "/certs";
    QDir().mkpath(dir);
    return dir;
}

QString logsDir()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    QString dir = base + "/cricket/logs";
    QDir().mkpath(dir);
    return dir;
}

static QString configFilePath()
{
    return configDir() + "/cricketConfig.txt";
}

QString Config::fullQuitMessage() const
{
    if (quitMessage.trimmed().isEmpty())
        return QString("[%1]").arg(AppInfo::VERSION_STRING);
    return QString("%1 [%2]").arg(quitMessage.trimmed(), AppInfo::VERSION_STRING);
}

ServerConfig* serverProfile(bool custom, int index)
{
    QVector<ServerConfig>& list = custom ? cfg.customServers : cfg.servers;
    if (index < 0 || index >= list.size())
        return nullptr;
    return &list[index];
}

static QJsonArray toArray(const QStringList& list)
{
    QJsonArray arr;
    for (const QString& s : list)
        arr.append(s);
    return arr;
}

static QStringList fromArray(const QJsonValue& v)
{
    QStringList out;
    for (const QJsonValue& item : v.toArray())
        out << item.toString();
    return out;
}

static QJsonObject serverToJson(const ServerConfig& srv)
{
    QJsonObject s;
    s["name"] = srv.name;
    s["host"] = srv.host;
    s["port"] = srv.port;
    s["use_tls"] = srv.useTLS;
    s["tls_verify"] = srv.verifyTLS;
    s["nick"] = srv.nick;
    s["altNick"] = srv.altNick;
    s["altNick2"] = srv.altNick2;
    s["pass"] = srv.pass;
    s["autoConnect"] = srv.autoConnect;
    s["autoReconnect"] = srv.autoReconnect;
    s["hideStatusMessages"] = srv.hideStatusMessages;
    s["timestampInterval"] = srv.timestampInterval;
    s["nick_alert"] = srv.nickAlert;
    s["use_sasl"] = srv.useSASL;
    s["sasl_user"] = srv.saslUser;
    s["use_certfp"] = srv.useCertFP;
    s["cert_profile_name"] = srv.certProfileName;
    s["cert_file_name"] = srv.certFileName;
    s["key_file_name"] = srv.keyFileName;
    s["background_image"] = srv.backgroundImagePath;
    s["bg_opacity"] = srv.backgroundOpacity;
    s["enable_emoticons"] = srv.enableEmoticons;
    s["logChatsToFile"] = srv.logChatsToFile;
    s["enableColorCodes"] = srv.enableColorCodes;
    s["serverListFontSize"] = srv.serverListFontSize;
    s["chatLogFontSize"] = srv.chatLogFontSize;
    s["userListFontSize"] = srv.userListFontSize;
    s["enableLiveTranslation"] = srv.enableInboundTranslation;
    s["gemini_api_key"] = srv.geminiApiKey;
    s["target_language"] = srv.targetLanguage;
    s["gemini_model"] = srv.geminiModel;
    s["enableOutboundTranslation"] = srv.enableOutboundTranslation;
    s["outbound_target_language"] = srv.outboundTargetLanguage;
    s["auto_send_translated_outbound"] = srv.autoSendTranslatedOutbound;
    s["autojoin"] = toArray(srv.autojoin);
    s["autocmdlist"] = toArray(srv.autocmdlist);
    s["ignored_nicks"] = toArray(srv.ignoredNicks);
    s["nick_color_names"] = toArray(srv.nickColors);
    QJsonArray colors;
    for (const QColor& c : srv.nickColorValues) {
        QJsonObject rgb;
        rgb["r"] = c.red();
        rgb["g"] = c.green();
        rgb["b"] = c.blue();
        colors.append(rgb);
    }
    s["nick_color_values"] = colors;
    return s;
}

static ServerConfig serverFromJson(const QJsonObject& s, const QString& fallbackName)
{
    ServerConfig srv;
    srv.name = s.value("name").toString(fallbackName);
    srv.host = s.value("host").toString("127.0.0.1");
    srv.port = static_cast<quint16>(s.value("port").toInt(6697));
    srv.useTLS = s.value("use_tls").toBool(true);
    srv.verifyTLS = s.value("tls_verify").toBool(true);
    srv.nick = s.value("nick").toString("LinuxIRCUser");
    srv.altNick = s.value("altNick").toString(srv.nick + "+");
    srv.altNick2 = s.value("altNick2").toString(srv.nick + "__");
    srv.pass = s.value("pass").toString();
    srv.autoReconnect = s.value("autoReconnect").toBool(false);
    srv.autoConnect = s.value("autoConnect").toBool(false);
    srv.hideStatusMessages = s.value("hideStatusMessages").toBool(false);
    srv.timestampInterval = s.value("timestampInterval").toInt(cfg.timestampInterval);
    srv.nickAlert = s.value("nick_alert").toBool(true);
    srv.backgroundImagePath = s.value("background_image").toString();
    srv.backgroundOpacity = s.value("bg_opacity").toInt(30);
    srv.enableEmoticons = s.value("enable_emoticons").toBool(true);
    srv.logChatsToFile = s.value("logChatsToFile").toBool(false);
    srv.enableColorCodes = s.value("enableColorCodes").toBool(true);
    srv.useSASL = s.value("use_sasl").toBool(false);
    srv.saslUser = s.value("sasl_user").toString();
    srv.useCertFP = s.value("use_certfp").toBool(false);
    srv.certProfileName = s.value("cert_profile_name").toString();
    srv.certFileName = s.value("cert_file_name").toString();
    srv.keyFileName = s.value("key_file_name").toString();
    srv.enableInboundTranslation = s.value("enableLiveTranslation").toBool(false);
    srv.geminiApiKey = s.value("gemini_api_key").toString();
    srv.targetLanguage = s.value("target_language").toString("French");
    srv.geminiModel = s.value("gemini_model").toString("gemini-3.5-flash-lite");
    srv.enableOutboundTranslation = s.value("enableOutboundTranslation").toBool(false);
    srv.outboundTargetLanguage = s.value("outbound_target_language").toString("French");
    srv.autoSendTranslatedOutbound = s.value("auto_send_translated_outbound").toBool(false);
    srv.serverListFontSize = s.value("serverListFontSize").toInt(cfg.serverListFontSize);
    srv.chatLogFontSize = s.value("chatLogFontSize").toInt(cfg.chatLogFontSize);
    srv.userListFontSize = s.value("userListFontSize").toInt(cfg.userListFontSize);
    srv.autojoin = fromArray(s.value("autojoin"));
    srv.autocmdlist = fromArray(s.value("autocmdlist"));
    srv.ignoredNicks = fromArray(s.value("ignored_nicks"));
    srv.nickColors = fromArray(s.value("nick_color_names"));
    for (const QJsonValue& v : s.value("nick_color_values").toArray()) {
        QJsonObject o = v.toObject();
        srv.nickColorValues << QColor(o.value("r").toInt(255), o.value("g").toInt(255), o.value("b").toInt(255));
    }
    while (srv.nickColorValues.size() < srv.nickColors.size())
        srv.nickColorValues << QColor(Qt::red);
    return srv;
}

// Haiku builds saved the quit reason with a version suffix in older releases;
// strip anything that looks like one so it is not duplicated.
static QString cleanQuitMessage(QString msg)
{
    for (const char* marker : {": Cricket IRC Client", " [Cricket IRC Client", "[Cricket IRC Client"}) {
        int idx = msg.indexOf(marker);
        if (idx >= 0)
            msg.truncate(idx);
    }
    msg = msg.trimmed();
    if (msg.compare("Quit", Qt::CaseInsensitive) == 0 || msg.compare("Quit:", Qt::CaseInsensitive) == 0)
        msg.clear();
    return msg;
}

void saveConfig()
{
    QJsonObject j;
    j["enableSpellCheck"] = cfg.enableSpellCheck;
    j["debugEnable"] = cfg.debugEnable;
    j["search_engine"] = cfg.searchEngine;
    j["serverListFontSize"] = cfg.serverListFontSize;
    j["chatLogFontSize"] = cfg.chatLogFontSize;
    j["userListFontSize"] = cfg.userListFontSize;
    j["show_update_notifications"] = cfg.showUpdateNotifications;
    j["quitMessage"] = cleanQuitMessage(cfg.quitMessage);
    j["awayMessage"] = cfg.awayMessage;
    j["spell_language"] = cfg.spellLanguage;

    QJsonArray servers;
    for (const ServerConfig& srv : cfg.servers)
        servers.append(serverToJson(srv));
    j["servers"] = servers;

    QJsonArray custom;
    for (const ServerConfig& srv : cfg.customServers)
        custom.append(serverToJson(srv));
    j["custom_servers"] = custom;

    QFile file(configFilePath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(j).toJson(QJsonDocument::Indented));
        file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }
}

static ServerConfig defaultProfile(const QString& name, const QString& host, const QString& nick,
    const QStringList& autojoin)
{
    ServerConfig s;
    s.name = name;
    s.host = host;
    s.port = 6697;
    s.nick = nick;
    s.altNick = nick + "+";
    s.altNick2 = nick + "__";
    s.autojoin = autojoin;
    s.serverListFontSize = cfg.serverListFontSize;
    s.chatLogFontSize = cfg.chatLogFontSize;
    s.userListFontSize = cfg.userListFontSize;
    return s;
}

void loadConfig()
{
    cfg.servers.clear();
    cfg.customServers.clear();

    bool mustSaveDefaults = false;
    QFile file(configFilePath());
    if (file.open(QIODevice::ReadOnly)) {
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            mustSaveDefaults = true;
        } else {
            QJsonObject j = doc.object();
            cfg.quitMessage = cleanQuitMessage(j.value("quitMessage").toString());
            cfg.awayMessage = j.value("awayMessage").toString("I am away from my computer right now.");
            cfg.enableSpellCheck = j.value("enableSpellCheck").toBool(true);
            cfg.debugEnable = j.value("debugEnable").toBool(false);
            cfg.serverListFontSize = j.value("serverListFontSize").toInt(12);
            cfg.chatLogFontSize = j.value("chatLogFontSize").toInt(12);
            cfg.userListFontSize = j.value("userListFontSize").toInt(12);
            cfg.searchEngine = j.value("search_engine").toString("https://duckduckgo.com");
            cfg.showUpdateNotifications = j.value("show_update_notifications").toBool(true);
            cfg.spellLanguage = j.value("spell_language").toString("en_US");

            for (const QJsonValue& v : j.value("servers").toArray())
                cfg.servers << serverFromJson(v.toObject(), "Unknown Server");
            for (const QJsonValue& v : j.value("custom_servers").toArray())
                cfg.customServers << serverFromJson(v.toObject(), "Custom Server");
        }
    } else {
        mustSaveDefaults = true;
    }

    if (mustSaveDefaults || cfg.servers.isEmpty()) {
        cfg.servers.clear();
        cfg.customServers.clear();
        cfg.quitMessage = "App Quit";
        cfg.awayMessage = "I am away from my computer right now.";

        QString nick = QString("LinuxIRCUser%1").arg(QRandomGenerator::global()->bounded(1000, 10000));
        cfg.servers << defaultProfile("Libera Chat", "irc.libera.chat", nick, {});
        cfg.servers << defaultProfile("OFTC", "irc.oftc.net", nick, {});
        saveConfig();
    }
}
