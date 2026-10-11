/*
 * Cricket IRC Client - Linux port
 * Distributed under the terms of the MIT license.
 */
#include "ircconnection.h"

#include <QFile>
#include <QFileInfo>
#include <QSslCertificate>
#include <QSslKey>
#include <QStringDecoder>

static const int kReconnectDelaysSecs[] = {5, 10, 20, 30, 60};
static const int kMaxReconnectAttempts = 8;

IrcConnection::IrcConnection(const ServerConfig& profile, QObject* parent)
    : QObject(parent), fProfile(profile)
{
    fReconnectTimer.setSingleShot(true);
    connect(&fReconnectTimer, &QTimer::timeout, this, &IrcConnection::onReconnectTimer);
    fChanModes = {"beI", "k", "l", "imnpst"};
}

IrcConnection::~IrcConnection()
{
    if (fSocket) {
        fSocket->disconnect(this);
        fSocket->abort();
    }
}

bool IrcConnection::isConnected() const
{
    return fSocket && fSocket->state() == QAbstractSocket::ConnectedState;
}

bool IrcConnection::loadClientCertificate()
{
    const QString dir = certsDir();
    QString certName = fProfile.certFileName.isEmpty() ? QString("nick.crt") : fProfile.certFileName;
    QString keyName = fProfile.keyFileName.isEmpty() ? QString("nick.key") : fProfile.keyFileName;
    auto resolve = [&](const QString& name) {
        return QFileInfo(name).isAbsolute() ? name : dir + "/" + name;
    };

    QString certPath = resolve(certName);
    QString keyPath = resolve(keyName);
    // A single .pem profile holding both cert and key is also accepted.
    if ((!QFile::exists(certPath) || !QFile::exists(keyPath)) && !fProfile.certProfileName.isEmpty()) {
        certPath = keyPath = resolve(fProfile.certProfileName);
    }

    QFile certFile(certPath);
    QFile keyFile(keyPath);
    if (!certFile.open(QIODevice::ReadOnly) || !keyFile.open(QIODevice::ReadOnly)) {
        emit statusMessage(QString("--- [CertFP] Certificate files not found (%1, %2). Connecting without a client certificate.")
            .arg(certPath, keyPath));
        return false;
    }

    QList<QSslCertificate> certs = QSslCertificate::fromData(certFile.readAll(), QSsl::Pem);
    QByteArray keyData = keyFile.readAll();
    QSslKey key(keyData, QSsl::Rsa, QSsl::Pem);
    if (key.isNull())
        key = QSslKey(keyData, QSsl::Ec, QSsl::Pem);
    if (certs.isEmpty() || key.isNull()) {
        emit statusMessage("--- [CertFP] Could not parse the certificate or private key. Connecting without a client certificate.");
        return false;
    }

    fSocket->setLocalCertificateChain(certs);
    fSocket->setPrivateKey(key);
    emit statusMessage(QString("--- [CertFP] Using client certificate %1").arg(QFileInfo(certPath).fileName()));
    return true;
}

void IrcConnection::connectToServer()
{
    fReconnectTimer.stop();
    if (fSocket) {
        fSocket->disconnect(this);
        fSocket->abort();
        fSocket->deleteLater();
    }

    fReadBuffer.clear();
    fRegistered = false;
    fCapFinished = false;
    fSaslInProgress = false;
    fAvailableCaps.clear();
    fEnabledCaps.clear();
    fNickAttempts = 0;
    fUserRequestedDisconnect = false;
    fDropHandled = false;
    fNick = fProfile.nick;

    fSocket = new QSslSocket(this);
    fSocket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
    connect(fSocket, &QSslSocket::readyRead, this, &IrcConnection::onReadyRead);
    connect(fSocket, &QSslSocket::disconnected, this, &IrcConnection::onDisconnected);
    connect(fSocket, &QAbstractSocket::errorOccurred, this, &IrcConnection::onSocketError);
    connect(fSocket, &QSslSocket::sslErrors, this, &IrcConnection::onSslErrors);

    emit statusMessage(QString("--- Connecting to %1:%2%3...")
        .arg(fProfile.host).arg(fProfile.port).arg(fProfile.useTLS ? " (TLS)" : ""));

    if (fProfile.useTLS) {
        if (!fProfile.verifyTLS)
            fSocket->setPeerVerifyMode(QSslSocket::VerifyNone);
        if (fProfile.useCertFP)
            loadClientCertificate();
        connect(fSocket, &QSslSocket::encrypted, this, &IrcConnection::onEncryptedOrConnected);
        fSocket->connectToHostEncrypted(fProfile.host, fProfile.port);
    } else {
        connect(fSocket, &QSslSocket::connected, this, &IrcConnection::onEncryptedOrConnected);
        fSocket->connectToHost(fProfile.host, fProfile.port);
    }
}

void IrcConnection::disconnectFromServer(const QString& quitMessage)
{
    fUserRequestedDisconnect = true;
    fReconnectTimer.stop();
    fReconnectAttempts = 0;
    if (!fSocket)
        return;
    if (fSocket->state() == QAbstractSocket::ConnectedState) {
        sendRaw("QUIT :" + quitMessage);
        fSocket->flush();
        fSocket->waitForBytesWritten(1000);
        fSocket->disconnectFromHost();
        // Give the server a moment to close the link, then force it.
        QTimer::singleShot(2000, fSocket, [s = fSocket]() { s->abort(); });
    } else {
        fSocket->abort();
        handleDrop();
    }
}

void IrcConnection::sendRaw(const QString& line)
{
    if (!fSocket || fSocket->state() != QAbstractSocket::ConnectedState)
        return;
    QString clean = line;
    clean.remove('\r');
    clean.remove('\n');
    fSocket->write(clean.toUtf8() + "\r\n");
    if (cfg.debugEnable)
        emit rawTraffic("OUT", clean);
}

void IrcConnection::onEncryptedOrConnected()
{
    fReconnectAttempts = 0;
    emit statusMessage("--- Connected. Registering...");
    emit connected();

    if (!fProfile.pass.isEmpty() && !fProfile.useSASL)
        sendRaw("PASS " + fProfile.pass);
    sendRaw("CAP LS 302");
    sendRaw("NICK " + fNick);
    sendRaw("USER " + fNick + " 0 * :Cricket IRC Client");
}

void IrcConnection::onReadyRead()
{
    fReadBuffer.append(fSocket->readAll());
    int nl;
    while ((nl = fReadBuffer.indexOf('\n')) >= 0) {
        QByteArray rawLine = fReadBuffer.left(nl);
        fReadBuffer.remove(0, nl + 1);
        if (rawLine.endsWith('\r'))
            rawLine.chop(1);
        if (rawLine.isEmpty())
            continue;

        // IRC has no fixed encoding; prefer UTF-8 and fall back to Latin-1.
        auto decoder = QStringDecoder(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
        QString line = decoder(rawLine);
        if (decoder.hasError())
            line = QString::fromLatin1(rawLine);
        handleLine(line);
    }
}

void IrcConnection::handleLine(const QString& line)
{
    if (cfg.debugEnable)
        emit rawTraffic("IN", line);

    IrcMessage msg = IrcMessage::parse(line);
    if (msg.command.isEmpty())
        return;

    if (msg.command == "PING") {
        sendRaw("PONG :" + msg.last());
        return;
    }

    if (handleRegistration(msg))
        return;

    emit messageReceived(msg);
}

QString IrcConnection::tryNextNick()
{
    ++fNickAttempts;
    if (fNickAttempts == 1 && !fProfile.altNick.isEmpty())
        return fProfile.altNick;
    if (fNickAttempts == 2 && !fProfile.altNick2.isEmpty())
        return fProfile.altNick2;
    return fProfile.nick + QString("_").repeated(fNickAttempts - 1);
}

void IrcConnection::finishCap()
{
    if (fCapFinished)
        return;
    fCapFinished = true;
    sendRaw("CAP END");
}

void IrcConnection::sendSaslPayload(const QByteArray& payload)
{
    QByteArray b64 = payload.toBase64();
    if (b64.isEmpty()) {
        sendRaw("AUTHENTICATE +");
        return;
    }
    for (int i = 0; i < b64.size(); i += 400)
        sendRaw("AUTHENTICATE " + QString::fromLatin1(b64.mid(i, 400)));
    if (b64.size() % 400 == 0)
        sendRaw("AUTHENTICATE +");
}

// Returns true when the message was consumed by the registration logic.
bool IrcConnection::handleRegistration(const IrcMessage& msg)
{
    const QString& cmd = msg.command;

    if (cmd == "CAP") {
        const QString sub = msg.param(1).toUpper();
        if (sub == "LS") {
            // Multi-line listing: "CAP * LS * :caps..." has a '*' before the final chunk.
            bool more = msg.params.size() >= 4 && msg.param(2) == "*";
            fAvailableCaps += " " + msg.last();
            if (more)
                return true;

            QStringList wanted;
            const QStringList offered = fAvailableCaps.split(' ', Qt::SkipEmptyParts);
            auto offers = [&](const QString& cap) {
                for (const QString& o : offered)
                    if (o == cap || o.startsWith(cap + "="))
                        return true;
                return false;
            };
            for (const char* cap : {"multi-prefix", "away-notify", "account-notify", "chghost", "invite-notify"})
                if (offers(cap))
                    wanted << cap;
            if (fProfile.useSASL && offers("sasl"))
                wanted << "sasl";

            if (wanted.isEmpty())
                finishCap();
            else
                sendRaw("CAP REQ :" + wanted.join(' '));
            return true;
        }
        if (sub == "ACK") {
            const QStringList acked = msg.last().split(' ', Qt::SkipEmptyParts);
            for (const QString& c : acked)
                fEnabledCaps.insert(c.startsWith('-') ? c.mid(1) : c);
            if (acked.contains("sasl") && fProfile.useSASL && !fRegistered) {
                fSaslMechanism = fProfile.useCertFP ? "EXTERNAL" : "PLAIN";
                fSaslInProgress = true;
                sendRaw("AUTHENTICATE " + fSaslMechanism);
            } else if (!fSaslInProgress) {
                finishCap();
            }
            return true;
        }
        if (sub == "NAK") {
            finishCap();
            return true;
        }
        if (sub == "NEW" || sub == "DEL") {
            emit statusMessage(QString("--- [CAP] %1: %2").arg(sub, msg.last()));
            return true;
        }
        return true;
    }

    if (cmd == "AUTHENTICATE") {
        if (msg.param(0) == "+") {
            if (fSaslMechanism == "EXTERNAL") {
                sendSaslPayload(QByteArray());
            } else {
                QByteArray user = (fProfile.saslUser.isEmpty() ? fProfile.nick : fProfile.saslUser).toUtf8();
                QByteArray payload = user + '\0' + user + '\0' + fProfile.pass.toUtf8();
                sendSaslPayload(payload);
            }
        }
        return true;
    }

    if (cmd == "900") { // RPL_LOGGEDIN
        emit statusMessage(">>> " + msg.last());
        return true;
    }
    if (cmd == "903") {
        emit statusMessage(">>> SASL authentication successful. Logged in securely.");
        fSaslInProgress = false;
        finishCap();
        return true;
    }
    if (cmd == "902" || cmd == "904" || cmd == "905" || cmd == "906" || cmd == "907" || cmd == "908") {
        if (cmd == "908")
            return true; // mechanism list, informational
        emit statusMessage(">>> SASL authentication failed (" + msg.last() + "). Continuing without it.");
        fSaslInProgress = false;
        finishCap();
        return true;
    }

    if (!fRegistered && (cmd == "433" || cmd == "432" || cmd == "437")) {
        QString next = tryNextNick();
        emit statusMessage(QString("--- Nickname %1 unavailable. Trying %2").arg(msg.param(1), next));
        fNick = next;
        sendRaw("NICK " + next);
        return true;
    }

    if (cmd == "001") {
        fRegistered = true;
        fCapFinished = true;
        if (!msg.param(0).isEmpty() && msg.param(0) != "*")
            fNick = msg.param(0);
        emit nickChanged(fNick);
        emit messageReceived(msg);
        emit registered();
        return true;
    }

    if (cmd == "005") {
        for (int i = 1; i < msg.params.size() - (msg.hasTrailing ? 1 : 0); ++i) {
            const QString token = msg.params.at(i);
            if (token.startsWith("PREFIX=(")) {
                int close = token.indexOf(')');
                if (close > 8) {
                    fPrefixModes = token.mid(8, close - 8);
                    fPrefixSymbols = token.mid(close + 1);
                }
            } else if (token.startsWith("CHANMODES=")) {
                fChanModes = token.mid(10).split(',');
            }
        }
        return false; // still shown in the status log
    }

    if (cmd == "NICK" && msg.nick().compare(fNick, Qt::CaseInsensitive) == 0) {
        fNick = msg.param(0);
        emit nickChanged(fNick);
        return false;
    }

    return false;
}

void IrcConnection::handleDrop()
{
    // A failed connect only reports an error, while a lost link can report an
    // error and then disconnected(); make sure one drop is handled once.
    if (fDropHandled)
        return;
    fDropHandled = true;
    fRegistered = false;
    emit statusMessage("--- Disconnected.");
    emit disconnected();
    scheduleReconnect();
}

void IrcConnection::onDisconnected()
{
    handleDrop();
}

void IrcConnection::onSocketError(QAbstractSocket::SocketError err)
{
    if (err == QAbstractSocket::RemoteHostClosedError && fUserRequestedDisconnect)
        return;
    emit statusMessage("--- Connection error: " + fSocket->errorString());
    if (fSocket->state() == QAbstractSocket::UnconnectedState)
        handleDrop();
}

void IrcConnection::onSslErrors(const QList<QSslError>& errors)
{
    for (const QSslError& e : errors)
        emit statusMessage("--- TLS error: " + e.errorString());
    if (!fProfile.verifyTLS)
        fSocket->ignoreSslErrors();
    else
        emit statusMessage("--- Certificate verification failed. Disable \"Verify server certificate\" in the server settings to connect anyway.");
}

void IrcConnection::scheduleReconnect()
{
    bool userRequested = fUserRequestedDisconnect;
    fUserRequestedDisconnect = false;
    if (!fProfile.autoReconnect || userRequested || fReconnectTimer.isActive())
        return;

    ++fReconnectAttempts;
    if (fReconnectAttempts > kMaxReconnectAttempts) {
        emit statusMessage(QString("--- [Auto-Reconnect] Gave up after %1 failed attempts. Use Connect to try again manually.")
            .arg(kMaxReconnectAttempts));
        fReconnectAttempts = 0;
        return;
    }
    const int count = int(sizeof(kReconnectDelaysSecs) / sizeof(kReconnectDelaysSecs[0]));
    int delay = kReconnectDelaysSecs[qMin(fReconnectAttempts - 1, count - 1)];
    emit statusMessage(QString("--- [Auto-Reconnect] Attempt %1 of %2 in %3s...")
        .arg(fReconnectAttempts).arg(kMaxReconnectAttempts).arg(delay));
    fReconnectTimer.start(delay * 1000);
}

void IrcConnection::onReconnectTimer()
{
    int attempts = fReconnectAttempts;
    connectToServer();
    fReconnectAttempts = attempts;
}
