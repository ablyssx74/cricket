/*
 * Cricket IRC Client - Linux port
 * Distributed under the terms of the MIT license.
 */
#ifndef CRICKET_CONFIG_H
#define CRICKET_CONFIG_H

#include <QColor>
#include <QString>
#include <QStringList>
#include <QVector>

namespace AppInfo {
extern const char* const VERSION_NUMBER;   // "0.0.67"
extern const char* const VERSION_STRING;   // "Cricket IRC Client v.0.0.67 (Linux)"
}

// Per-server profile. Field names in the JSON file match the Haiku build so a
// cricketConfig.txt copied over from Haiku loads unchanged.
struct ServerConfig {
    QString name;
    QString host;
    quint16 port = 6697;
    bool useTLS = true;
    bool verifyTLS = true;
    QString nick;
    QString altNick;
    QString altNick2;
    QString pass;
    QStringList autojoin;
    QStringList autocmdlist;
    bool autoConnect = false;
    bool autoReconnect = false;
    bool hideStatusMessages = false;
    bool enableEmoticons = true;
    QString backgroundImagePath;
    int backgroundOpacity = 30;
    int serverListFontSize = 12;
    int chatLogFontSize = 12;
    int userListFontSize = 12;
    bool nickAlert = true;
    bool logChatsToFile = false;
    bool enableColorCodes = true;
    QStringList ignoredNicks;
    QStringList nickColors;          // wildcard patterns
    QVector<QColor> nickColorValues; // parallel to nickColors
    int timestampInterval = 30;      // minutes between timestamps
    bool useSASL = false;
    QString saslUser;
    bool useCertFP = false;
    QString certProfileName;
    QString certFileName;
    QString keyFileName;

    bool enableInboundTranslation = false;
    QString geminiApiKey;
    QString geminiModel = "gemini-3.5-flash-lite";
    QString targetLanguage = "French";
    bool enableOutboundTranslation = false;
    QString outboundTargetLanguage = "French";
    bool autoSendTranslatedOutbound = false;
};

struct Config {
    bool enableSpellCheck = true;
    bool debugEnable = false;
    QVector<ServerConfig> servers;
    QVector<ServerConfig> customServers;
    int serverListFontSize = 12;
    int chatLogFontSize = 12;
    int userListFontSize = 12;
    QString quitMessage;   // user part only; version suffix appended when sending
    QString awayMessage = "I am away from my computer right now.";
    int timestampInterval = 30;
    QString searchEngine = "https://duckduckgo.com";
    bool showUpdateNotifications = true;
    QString spellLanguage = "en_US";

    QString fullQuitMessage() const;
};

extern Config cfg;

QString configDir();   // ~/.config/cricket
QString certsDir();    // ~/.config/cricket/certs
QString logsDir();     // ~/.local/share/cricket/logs

void loadConfig();
void saveConfig();

// Returns a pointer to the server profile addressed by (custom, index), or nullptr.
ServerConfig* serverProfile(bool custom, int index);

#endif
