/*
 * Cricket IRC Client - Linux port
 * Distributed under the terms of the MIT license.
 *
 * DCC file transfers and DCC CHAT (Linux counterpart of ../../dcc.cpp).
 *
 * DCC SEND in both directions, active and passive (reverse, with a token)
 * variants, DCC RESUME / ACCEPT and DCC CHAT. Listen ports come from a pool
 * of 5; each pool port is forwarded on the home router by a PortMapper
 * (PCP / NAT-PMP / UPnP). When the reachability check says we can't be
 * reached from the internet, offers switch to passive DCC.
 *
 * Everything runs on the GUI thread with Qt's asynchronous sockets. The
 * main window owns the IRC connections: DccManager asks it to send CTCP
 * lines through sendCtcp() and reports through the other signals. Servers
 * are identified by an opaque pointer (the window's Session*).
 */
#ifndef CRICKET_DCC_H
#define CRICKET_DCC_H

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>
#include <memory>
#include <vector>

class QTcpServer;
class QTcpSocket;
class QFile;
class QTimer;
class QWidget;
class DccTransfersDialog;

namespace cricket {
class PortMapper;
struct PortMapReport;
}

class DccManager : public QObject {
    Q_OBJECT
public:
    enum Direction { Send, Receive, Chat };
    enum State { Offered, Waiting, Connecting, Active, Done, Failed, Cancelled };

    struct Info {
        int id = 0;
        Direction direction = Send;
        State state = Waiting;
        QString nick;
        QString fileName;
        QString path;
        qint64 size = 0;
        qint64 bytesDone = 0;
        qint64 startBytes = 0;   // resume offset
        bool passive = false;
        QString error;
        qint64 startMs = 0;      // when data started flowing
        qint64 endMs = 0;
    };

    explicit DccManager(QWidget* dialogParent);
    ~DccManager() override;

    // Re-reads the DCC settings from cfg; restarts the port mappers when the
    // port or the port-mapping switch changed.
    void applySettings();

    // A CTCP "DCC ..." request (no \x01 framing) from `nick`. Returns true if
    // it was a DCC request.
    bool handleCtcp(void* server, const QString& nick, const QString& ctcp);

    void offerFile(void* server, const QString& nick, const QString& path);
    void offerChat(void* server, const QString& nick);
    bool chatSend(void* server, const QString& nick, const QString& text, bool action);
    void closeChat(void* server, const QString& nick);
    void forgetServer(void* server);

    // For the transfers dialog (file transfers only).
    QList<Info> snapshot() const;
    QString statusText() const;
    void cancel(int id);
    void clearFinished();
    QString downloadDir() const;
    void showTransfers();

signals:
    void sendCtcp(void* server, const QString& target, const QString& ctcp);
    void logLine(void* server, const QString& text);
    void chatOpened(void* server, const QString& nick);
    void chatLine(void* server, const QString& nick, const QString& text, bool action);
    void chatClosed(void* server, const QString& nick, const QString& reason);

private:
    struct Transfer {
        Info info;
        void* server = nullptr;
        QString token;
        QString remoteHost;
        quint16 remotePort = 0;
        quint16 listenPort = 0;
        bool listenMode = false;       // accept a connection instead of connecting
        bool resumeRequested = false;
        bool triedLoopback = false;
        qint64 position = 0;           // resume offset
        QTcpServer* listener = nullptr;
        QTcpSocket* socket = nullptr;
        QFile* file = nullptr;
        QTimer* timer = nullptr;       // accept / connect / stall / final-ack timeout
        QByteArray inbuf;              // acks (send) or partial lines (chat)
        quint32 lastAck = 0;
        bool fileDone = false;         // send: everything handed to the socket
    };

    Transfer* find(int id) const;
    Transfer* newTransfer(Direction dir, void* server, const QString& nick);
    void startListening(Transfer* t);  // on t->listener (already bound)
    void startConnecting(Transfer* t);
    void onConnected(Transfer* t);
    void pumpSend(Transfer* t);
    void onReadyRead(Transfer* t);
    void finish(Transfer* t, State state, const QString& error = QString());
    void armTimer(Transfer* t, int ms, const QString& failure);

    void handleIncomingSend(void* server, const QString& nick, const QString& fileName,
        const QString& host, quint16 port, qint64 size, const QString& token);
    void handleIncomingChat(void* server, const QString& nick, const QString& host,
        quint16 port, const QString& token);
    void acceptOffer(int id, bool resume);
    void acceptChat(int id);

    bool allocListenPort(Transfer* t);
    void releaseListenPort(Transfer* t);

    QString advertisedIP() const;   // "" when we think we can't be reached
    bool usePassive() const;
    bool isOwnPublicAddress(const QString& host) const;
    static quint32 localIPv4();

    void startPortMappers();
    void stopPortMappers();
    void onPortMapReport(const cricket::PortMapReport& report);

    QString makeToken(int id) const;
    void log(void* server, const QString& text) { emit logLine(server, text); }

    QWidget* fParent;
    QList<Transfer*> fTransfers;
    int fNextId = 1;

    // settings snapshot
    QString fDownloadDir;
    quint16 fFirstPort = 59200;
    int fPortCount = 5;
    bool fUsePortMapping = true;
    QString fExternalIP;
    bool fForcePassive = false;

    QVector<bool> fPortBusy;
    QVector<bool> fPortMapped;
    std::vector<std::unique_ptr<cricket::PortMapper>> fMappers;
    QString fMapStatus;
    QString fReachStatus;
    int fReachable = -1;       // 1 yes, 0 no, -1 unknown
    QString fInternetIP;
    QString fRouterIP;
    int fMappedCount = 0;

    QPointer<DccTransfersDialog> fDialog;
};

#endif
