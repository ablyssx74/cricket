/*
 * Cricket IRC Client - Linux port
 * Distributed under the terms of the MIT license.
 */
#ifndef CRICKET_IRCCONNECTION_H
#define CRICKET_IRCCONNECTION_H

#include "config.h"
#include "ircmessage.h"

#include <QObject>
#include <QSet>
#include <QSslSocket>
#include <QTimer>

// One connection to one IRC network. Handles the socket, TLS/CertFP,
// registration (CAP negotiation, SASL PLAIN/EXTERNAL, nick fallback),
// PING/PONG and the auto-reconnect backoff. Everything else is handed to the
// UI through messageReceived().
class IrcConnection : public QObject {
    Q_OBJECT
public:
    explicit IrcConnection(const ServerConfig& profile, QObject* parent = nullptr);
    ~IrcConnection() override;

    void setProfile(const ServerConfig& profile) { fProfile = profile; }
    const ServerConfig& profile() const { return fProfile; }

    void connectToServer();
    void disconnectFromServer(const QString& quitMessage);

    bool isConnected() const;
    bool isRegistered() const { return fRegistered; }
    QString nick() const { return fNick; }
    bool hasCap(const QString& cap) const { return fEnabledCaps.contains(cap); }

    void sendRaw(const QString& line);

    // RPL_ISUPPORT values the UI needs.
    QString prefixModes() const { return fPrefixModes; }     // e.g. "qaohv"
    QString prefixSymbols() const { return fPrefixSymbols; } // e.g. "~&@%+"
    QString chanModesA() const { return fChanModes.value(0); }
    QString chanModesB() const { return fChanModes.value(1); }
    QString chanModesC() const { return fChanModes.value(2); }

signals:
    void messageReceived(const IrcMessage& msg);
    void statusMessage(const QString& text);
    void connected();
    void registered();
    void disconnected();
    void nickChanged(const QString& nick);
    void rawTraffic(const QString& direction, const QString& line);

private slots:
    void onEncryptedOrConnected();
    void onReadyRead();
    void onDisconnected();
    void onSocketError(QAbstractSocket::SocketError err);
    void onSslErrors(const QList<QSslError>& errors);
    void onReconnectTimer();

private:
    void handleLine(const QString& line);
    bool handleRegistration(const IrcMessage& msg);
    void finishCap();
    void sendSaslPayload(const QByteArray& payload);
    void scheduleReconnect();
    void handleDrop();
    bool loadClientCertificate();
    QString tryNextNick();

    ServerConfig fProfile;
    QSslSocket* fSocket = nullptr;
    QByteArray fReadBuffer;
    QTimer fReconnectTimer;
    int fReconnectAttempts = 0;
    bool fUserRequestedDisconnect = false;
    bool fDropHandled = true;
    bool fRegistered = false;
    bool fCapFinished = false;
    bool fSaslInProgress = false;
    QString fSaslMechanism;
    QString fAvailableCaps;
    QSet<QString> fEnabledCaps;
    QString fNick;
    int fNickAttempts = 0;
    QString fPrefixModes = "ov";
    QString fPrefixSymbols = "@+";
    QStringList fChanModes;
};

#endif
