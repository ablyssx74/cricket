/*
 * Cricket IRC Client - Linux port
 * Distributed under the terms of the MIT license.
 */
#include "dcc.h"

#include "config.h"
#include "portmapper.h"
#include "services.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHostAddress>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QtEndian>

#include <algorithm>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

static const int kAcceptTimeoutMs  = 180 * 1000;
static const int kConnectTimeoutMs = 30 * 1000;
static const int kStallTimeoutMs   = 120 * 1000;
static const int kFinalAckWaitMs   = 30 * 1000;
static const int kMaxPendingPerNick = 3;
static const qint64 kChunk = 64 * 1024;
static const qint64 kMaxQueued = 256 * 1024;

// =============================================================================
// Helpers (mirror the Haiku build's dcc.cpp)
// =============================================================================

namespace {

QString formatSize(qint64 bytes)
{
    if (bytes < 1024)
        return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QString::number(bytes / 1024.0, 'f', 1) + " KiB";
    if (bytes < 1024LL * 1024 * 1024)
        return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MiB";
    return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GiB";
}

bool isNumber(const QString& s)
{
    if (s.isEmpty())
        return false;
    for (QChar c : s)
        if (c < '0' || c > '9')
            return false;
    return true;
}

bool looksLikeAddress(const QString& s)
{
    if (s.isEmpty())
        return false;
    for (QChar c : s) {
        bool ok = c.isDigit() || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') || c == '.' || c == ':';
        if (!ok)
            return false;
    }
    return true;
}

QString ipv4ToString(quint32 hostOrder)
{
    return QString("%1.%2.%3.%4").arg((hostOrder >> 24) & 0xFF).arg((hostOrder >> 16) & 0xFF)
        .arg((hostOrder >> 8) & 0xFF).arg(hostOrder & 0xFF);
}

quint32 parseIPv4(const QString& s)
{
    QHostAddress a;
    if (!a.setAddress(s) || a.protocol() != QAbstractSocket::IPv4Protocol)
        return 0;
    return a.toIPv4Address();
}

bool isPrivateIPv4(quint32 ip)
{
    quint32 a = (ip >> 24) & 0xFF, b = (ip >> 16) & 0xFF;
    return a == 10 || a == 127 || a == 0 || (a == 172 && b >= 16 && b <= 31) || (a == 192 && b == 168)
        || (a == 100 && b >= 64 && b <= 127) || (a == 169 && b == 254);
}

QString dccHostToString(const QString& field)
{
    if (isNumber(field))
        return ipv4ToString((quint32)field.toULongLong());
    return field;
}

QString sanitizeFileName(const QString& name)
{
    QString base = name;
    int cut = qMax(base.lastIndexOf('/'), base.lastIndexOf('\\'));
    if (cut >= 0)
        base = base.mid(cut + 1);
    QString clean;
    for (QChar c : base) {
        if (c.unicode() < 0x20 || c.unicode() == 0x7F || c == ':')
            continue;
        clean += c;
    }
    clean = clean.trimmed();
    while (clean.startsWith('.'))
        clean.remove(0, 1);
    if (clean.toUtf8().size() > 200)
        clean.truncate(200);
    if (clean.isEmpty())
        clean = "dcc_file";
    return clean;
}

QString uniquePath(const QString& dir, const QString& name)
{
    QString path = dir + "/" + name;
    if (!QFileInfo::exists(path))
        return path;
    QString base = name, ext;
    int dot = name.lastIndexOf('.');
    if (dot > 0) {
        base = name.left(dot);
        ext = name.mid(dot);
    }
    for (int n = 1; n < 1000; ++n) {
        path = QString("%1/%2 (%3)%4").arg(dir, base).arg(n).arg(ext);
        if (!QFileInfo::exists(path))
            return path;
    }
    return QString("%1/%2 (%3)%4").arg(dir, base).arg(QDateTime::currentMSecsSinceEpoch()).arg(ext);
}

QString quoteFileName(const QString& name)
{
    QString q = name;
    q.replace('"', '\'');
    return q.contains(' ') ? "\"" + q + "\"" : q;
}

// "DCC <TYPE> <name> <args...>"; the name may be quoted, or (legacy) contain
// unquoted spaces, recovered from the expected number of trailing args.
bool parseDccRequest(const QString& ctcp, QString& type, QString& name, QStringList& args)
{
    QString rest = ctcp.trimmed();
    if (!rest.startsWith("DCC ", Qt::CaseInsensitive))
        return false;
    rest = rest.mid(4).trimmed();
    int sp = rest.indexOf(' ');
    if (sp < 0) {
        type = rest.toUpper();
        return true;
    }
    type = rest.left(sp).toUpper();
    rest = rest.mid(sp + 1).trimmed();

    if (rest.startsWith('"')) {
        int close = rest.indexOf('"', 1);
        if (close < 0)
            return false;
        name = rest.mid(1, close - 1);
        args = rest.mid(close + 1).split(' ', Qt::SkipEmptyParts);
        return true;
    }
    QStringList tokens = rest.split(' ', Qt::SkipEmptyParts);
    int n = tokens.size();
    int argCount;
    if (type == "SEND")
        argCount = (n >= 5 && looksLikeAddress(tokens[n - 4]) && isNumber(tokens[n - 3])
            && isNumber(tokens[n - 2]) && isNumber(tokens[n - 1])) ? 4 : 3;
    else if (type == "RESUME" || type == "ACCEPT")
        argCount = (n >= 4 && isNumber(tokens[n - 3]) && isNumber(tokens[n - 2]) && isNumber(tokens[n - 1])) ? 3 : 2;
    else
        argCount = n > 0 ? n - 1 : 0;
    if (n < argCount + 1)
        return false;
    name = tokens.mid(0, n - argCount).join(' ');
    args = tokens.mid(n - argCount);
    return true;
}

bool isFinished(DccManager::State s)
{
    return s == DccManager::Done || s == DccManager::Failed || s == DccManager::Cancelled;
}

}  // namespace


// =============================================================================
// Transfers dialog
// =============================================================================

class DccTransfersDialog : public QDialog {
public:
    explicit DccTransfersDialog(DccManager* manager, QWidget* parent)
        : QDialog(parent), fManager(manager)
    {
        setWindowTitle(tr("DCC Transfers"));
        setAttribute(Qt::WA_DeleteOnClose);
        resize(680, 360);

        fStatus = new QLabel;
        fStatus->setWordWrap(true);
        fTree = new QTreeWidget;
        fTree->setColumnCount(4);
        fTree->setHeaderLabels({tr("File"), tr("Peer"), tr("Progress"), tr("Status")});
        fTree->setRootIsDecorated(false);
        fTree->setUniformRowHeights(true);
        fTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        fTree->header()->setSectionResizeMode(3, QHeaderView::Stretch);
        fTree->setColumnWidth(2, 140);
        connect(fTree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item) {
            for (const DccManager::Info& info : fManager->snapshot()) {
                if (info.id != item->data(0, Qt::UserRole).toInt())
                    continue;
                if (info.direction == DccManager::Receive && info.state != DccManager::Done)
                    return;
                QDesktopServices::openUrl(QUrl::fromLocalFile(info.path));
            }
        });

        auto* cancel = new QPushButton(tr("Cancel Transfer"));
        auto* clear = new QPushButton(tr("Clear Finished"));
        auto* folder = new QPushButton(tr("Open Downloads Folder"));
        connect(cancel, &QPushButton::clicked, this, [this]() {
            if (QTreeWidgetItem* item = fTree->currentItem())
                fManager->cancel(item->data(0, Qt::UserRole).toInt());
        });
        connect(clear, &QPushButton::clicked, this, [this]() {
            fManager->clearFinished();
            refresh();
        });
        connect(folder, &QPushButton::clicked, this, [this]() {
            QDir().mkpath(fManager->downloadDir());
            QDesktopServices::openUrl(QUrl::fromLocalFile(fManager->downloadDir()));
        });

        auto* buttons = new QHBoxLayout;
        buttons->addWidget(cancel);
        buttons->addWidget(clear);
        buttons->addStretch();
        buttons->addWidget(folder);
        auto* layout = new QVBoxLayout(this);
        layout->addWidget(fStatus);
        layout->addWidget(fTree, 1);
        layout->addLayout(buttons);

        auto* timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &DccTransfersDialog::refresh);
        timer->start(500);
        refresh();
    }

    void refresh()
    {
        fStatus->setText(fManager->statusText());
        QList<DccManager::Info> infos = fManager->snapshot();
        for (int i = fTree->topLevelItemCount() - 1; i >= 0; --i) {
            int id = fTree->topLevelItem(i)->data(0, Qt::UserRole).toInt();
            bool found = std::any_of(infos.begin(), infos.end(), [id](const DccManager::Info& x) { return x.id == id; });
            if (!found)
                delete fTree->takeTopLevelItem(i);
        }
        for (const DccManager::Info& info : infos) {
            QTreeWidgetItem* item = nullptr;
            for (int i = 0; i < fTree->topLevelItemCount(); ++i) {
                if (fTree->topLevelItem(i)->data(0, Qt::UserRole).toInt() == info.id) {
                    item = fTree->topLevelItem(i);
                    break;
                }
            }
            if (!item) {
                item = new QTreeWidgetItem(fTree);
                item->setData(0, Qt::UserRole, info.id);
                auto* bar = new QProgressBar;
                bar->setRange(0, 1000);
                bar->setTextVisible(true);
                fTree->setItemWidget(item, 2, bar);
            }
            item->setText(0, QString("%1 %2").arg(info.direction == DccManager::Send ? "↑" : "↓", info.fileName));
            item->setText(1, info.nick);
            item->setText(3, statusLine(info));
            item->setToolTip(3, item->text(3));
            if (auto* bar = qobject_cast<QProgressBar*>(fTree->itemWidget(item, 2))) {
                int value = info.size > 0 ? int(1000.0 * info.bytesDone / info.size) : (info.state == DccManager::Done ? 1000 : 0);
                bar->setValue(qBound(0, value, 1000));
                bar->setFormat(QString("%1%").arg(value / 10));
            }
        }
    }

private:
    static QString statusLine(const DccManager::Info& i)
    {
        switch (i.state) {
        case DccManager::Offered:
            return tr("Waiting for you to accept (%1)").arg(formatSize(i.size));
        case DccManager::Waiting:
            return i.direction == DccManager::Send
                ? tr("Waiting for %1 to accept%2").arg(i.nick, i.passive ? tr(" (passive DCC)") : QString())
                : tr("Waiting for %1 to connect").arg(i.nick);
        case DccManager::Connecting:
            return tr("Connecting to %1…").arg(i.nick);
        case DccManager::Active:
        case DccManager::Done: {
            QString s = formatSize(i.bytesDone);
            if (i.size > 0)
                s += tr(" of %1").arg(formatSize(i.size));
            qint64 end = i.state == DccManager::Done ? i.endMs : QDateTime::currentMSecsSinceEpoch();
            double secs = i.startMs > 0 ? (end - i.startMs) / 1000.0 : 0;
            double rate = secs > 0.5 ? (i.bytesDone - i.startBytes) / secs : 0;
            if (rate > 0) {
                s += QString(" · %1/s").arg(formatSize(qint64(rate)));
                if (i.state == DccManager::Active && i.size > i.bytesDone) {
                    int left = int((i.size - i.bytesDone) / rate);
                    s += left >= 3600 ? tr(" · %1h %2m left").arg(left / 3600).arg((left / 60) % 60, 2, 10, QChar('0'))
                                      : tr(" · %1m %2s left").arg(left / 60).arg(left % 60, 2, 10, QChar('0'));
                }
            }
            if (i.state == DccManager::Done)
                s += tr(" · Done");
            return s;
        }
        case DccManager::Failed:
            return tr("Failed: %1").arg(i.error);
        case DccManager::Cancelled:
            return i.error.isEmpty() ? tr("Cancelled") : i.error;
        }
        return QString();
    }

    DccManager* fManager;
    QLabel* fStatus;
    QTreeWidget* fTree;
};


// =============================================================================
// DccManager
// =============================================================================

DccManager::DccManager(QWidget* dialogParent) : QObject(dialogParent), fParent(dialogParent)
{
    applySettings();
}

DccManager::~DccManager()
{
    stopPortMappers();
    for (Transfer* t : fTransfers) {
        if (t->socket)
            t->socket->abort();
        delete t->file;
        delete t;
    }
}

void DccManager::applySettings()
{
    QString dir = cfg.dccDownloadDir.trimmed();
    if (dir.isEmpty())
        dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (dir.isEmpty())
        dir = QDir::homePath() + "/Downloads";
    int port = cfg.dccFirstPort;
    if (port < 1024 || port > 65530)
        port = 59200;

    bool restart = quint16(port) != fFirstPort || cfg.dccUsePortMapping != fUsePortMapping
        || (fMappers.empty() && cfg.dccUsePortMapping);
    fDownloadDir = dir;
    fExternalIP = cfg.dccExternalIP.trimmed();
    fForcePassive = cfg.dccForcePassive;
    if (fPortBusy.size() != fPortCount)
        fPortBusy.fill(false, fPortCount);
    if (!restart)
        return;
    stopPortMappers();
    fFirstPort = quint16(port);
    fUsePortMapping = cfg.dccUsePortMapping;
    startPortMappers();
}

// -----------------------------------------------------------------------------
// Router port mapping
// -----------------------------------------------------------------------------

void DccManager::startPortMappers()
{
    fMappedCount = 0;
    fPortMapped.fill(false, fPortCount);
    fRouterIP.clear();
    fReachable = -1;
    fInternetIP.clear();
    fReachStatus.clear();
    if (!fUsePortMapping) {
        fMapStatus = tr("Router port forwarding is off.");
        return;
    }
    fMapStatus = tr("Looking for a UPnP / NAT-PMP router…");
    QPointer<DccManager> self(this);
    for (int i = 0; i < fPortCount; ++i) {
        auto report = [self](const cricket::PortMapReport& r) {
            // Worker thread -> GUI thread.
            cricket::PortMapReport copy = r;
            if (self)
                QMetaObject::invokeMethod(self, [self, copy]() {
                    if (self)
                        self->onPortMapReport(copy);
                }, Qt::QueuedConnection);
        };
        auto mapper = std::make_unique<cricket::PortMapper>(report, quint16(fFirstPort + i), "Cricket DCC", i == 0);
        if (i == 0)
            mapper->ProbeReachability();  // learn our public IP even if mapping fails
        mapper->Start();
        fMappers.push_back(std::move(mapper));
    }
}

void DccManager::stopPortMappers()
{
    for (auto& m : fMappers)
        m->RequestStop();
    fMappers.clear();  // each destructor joins its thread; mappings are removed on the way out
    fMappedCount = 0;
}

void DccManager::onPortMapReport(const cricket::PortMapReport& r)
{
    if (r.reachable != -2) {
        fReachable = r.reachable;
        if (r.internetIP.Length() > 0)
            fInternetIP = QString::fromStdString(r.internetIP.Std());
        fReachStatus = r.reachable == 1 ? tr("Reachable from the internet.")
            : r.reachable == 0 ? tr("Not reachable from the internet; using passive DCC.")
                               : tr("Internet reachability unknown.");
        return;
    }
    int index = r.internalPort - fFirstPort;
    if (fPortMapped.size() != fPortCount)
        fPortMapped.fill(false, fPortCount);
    if (index >= 0 && index < fPortMapped.size()) {
        if (r.state == cricket::PORT_MAP_STATE_MAPPED) {
            fPortMapped[index] = true;
            if (r.externalIP.Length() > 0)
                fRouterIP = QString::fromStdString(r.externalIP.Std());
        } else if (r.state == cricket::PORT_MAP_STATE_LOST || r.state == cricket::PORT_MAP_STATE_FAILED
            || r.state == cricket::PORT_MAP_STATE_REMOVED) {
            fPortMapped[index] = false;
        }
    }
    fMappedCount = int(std::count(fPortMapped.begin(), fPortMapped.end(), true));
    if (fMappedCount > 0) {
        fMapStatus = tr("Router port forwarding active");
        if (r.method.Length() > 0)
            fMapStatus += QString(" (%1)").arg(QString::fromStdString(r.method.Std()));
        fMapStatus += tr(": %1 of %2 DCC ports").arg(fMappedCount).arg(fPortCount);
        if (!fRouterIP.isEmpty())
            fMapStatus += tr(", external address %1").arg(fRouterIP);
        fMapStatus += ".";
    } else if (index == 0 && r.message.Length() > 0) {
        fMapStatus = QString::fromStdString(r.message.Std());
    }
}

// -----------------------------------------------------------------------------
// Addresses
// -----------------------------------------------------------------------------

quint32 DccManager::localIPv4()
{
    quint32 result = 0;
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
        return 0;
    sockaddr_in dst {};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(9);
    dst.sin_addr.s_addr = inet_addr("8.8.8.8");
    if (::connect(s, (sockaddr*)&dst, sizeof(dst)) == 0) {
        sockaddr_in local {};
        socklen_t len = sizeof(local);
        if (getsockname(s, (sockaddr*)&local, &len) == 0)
            result = ntohl(local.sin_addr.s_addr);
    }
    ::close(s);
    return result;
}

QString DccManager::advertisedIP() const
{
    if (!fExternalIP.isEmpty() && parseIPv4(fExternalIP) != 0)
        return fExternalIP;
    if (fReachable == 1 && !fInternetIP.isEmpty())
        return fInternetIP;
    if (fMappedCount > 0 && fReachable != 0 && !fRouterIP.isEmpty()) {
        quint32 router = parseIPv4(fRouterIP);
        if (router != 0 && !isPrivateIPv4(router))
            return fRouterIP;
    }
    quint32 local = localIPv4();
    if (local != 0 && !isPrivateIPv4(local))
        return ipv4ToString(local);
    return QString();
}

bool DccManager::usePassive() const
{
    if (fForcePassive)
        return true;
    if (!fExternalIP.isEmpty())
        return false;
    if (fReachable == 0)
        return true;
    return advertisedIP().isEmpty();
}

bool DccManager::isOwnPublicAddress(const QString& host) const
{
    quint32 ip = parseIPv4(host);
    if (ip == 0 || isPrivateIPv4(ip))
        return false;
    return (!fInternetIP.isEmpty() && parseIPv4(fInternetIP) == ip)
        || (!fRouterIP.isEmpty() && parseIPv4(fRouterIP) == ip)
        || (!fExternalIP.isEmpty() && parseIPv4(fExternalIP) == ip);
}

QString DccManager::makeToken(int id) const
{
    return QString::number((QDateTime::currentMSecsSinceEpoch() / 7 + id * 7919) % 100000000);
}

// -----------------------------------------------------------------------------
// Transfer plumbing
// -----------------------------------------------------------------------------

DccManager::Transfer* DccManager::find(int id) const
{
    for (Transfer* t : fTransfers)
        if (t->info.id == id)
            return t;
    return nullptr;
}

DccManager::Transfer* DccManager::newTransfer(Direction dir, void* server, const QString& nick)
{
    auto* t = new Transfer;
    t->info.id = fNextId++;
    t->info.direction = dir;
    t->info.nick = nick;
    t->server = server;
    t->timer = new QTimer(this);
    t->timer->setSingleShot(true);
    fTransfers << t;
    return t;
}

bool DccManager::allocListenPort(Transfer* t)
{
    for (int i = 0; i < fPortCount; ++i) {
        if (fPortBusy[i])
            continue;
        auto* server = new QTcpServer(this);
        if (!server->listen(QHostAddress::AnyIPv4, quint16(fFirstPort + i))) {
            delete server;
            continue;
        }
        server->setMaxPendingConnections(1);
        fPortBusy[i] = true;
        t->listener = server;
        t->listenPort = quint16(fFirstPort + i);
        return true;
    }
    return false;
}

void DccManager::releaseListenPort(Transfer* t)
{
    if (t->listener) {
        t->listener->close();
        t->listener->deleteLater();
        t->listener = nullptr;
    }
    int i = int(t->listenPort) - int(fFirstPort);
    if (t->listenPort && i >= 0 && i < fPortBusy.size())
        fPortBusy[i] = false;
    t->listenPort = 0;
}

void DccManager::armTimer(Transfer* t, int ms, const QString& failure)
{
    t->timer->disconnect();
    int id = t->info.id;
    connect(t->timer, &QTimer::timeout, this, [this, id, failure]() {
        if (Transfer* x = find(id))
            finish(x, Failed, failure);
    });
    t->timer->start(ms);
}

void DccManager::startListening(Transfer* t)
{
    int id = t->info.id;
    t->info.state = Waiting;
    armTimer(t, kAcceptTimeoutMs, tr("The peer never connected (timed out)"));
    connect(t->listener, &QTcpServer::newConnection, this, [this, id]() {
        Transfer* x = find(id);
        if (!x || !x->listener)
            return;
        QTcpSocket* sock = x->listener->nextPendingConnection();
        releaseListenPort(x);
        if (!sock)
            return;
        sock->setParent(this);
        x->socket = sock;
        onConnected(x);
    });
}

void DccManager::startConnecting(Transfer* t)
{
    int id = t->info.id;
    t->info.state = Connecting;
    armTimer(t, kConnectTimeoutMs, tr("Could not connect to the peer at %1:%2").arg(t->remoteHost).arg(t->remotePort));
    t->socket = new QTcpSocket(this);
    connect(t->socket, &QTcpSocket::connected, this, [this, id]() {
        if (Transfer* x = find(id))
            onConnected(x);
    });
    connect(t->socket, &QAbstractSocket::errorOccurred, this, [this, id](QAbstractSocket::SocketError) {
        Transfer* x = find(id);
        if (!x || x->info.state != Connecting)
            return;
        QString host = x->remoteHost;
        // The peer advertised our own public IP: it is on this machine (or
        // LAN). Without hairpin NAT only the loopback reaches it.
        if (!x->triedLoopback && isOwnPublicAddress(host)) {
            x->triedLoopback = true;
            x->socket->disconnect(this);
            x->socket->deleteLater();
            x->socket = nullptr;
            log(x->server, tr("--- [DCC] %1 advertised your own public address; trying this machine (127.0.0.1).").arg(x->info.nick));
            QString saved = x->remoteHost;
            x->remoteHost = "127.0.0.1";
            startConnecting(x);
            x->remoteHost = saved;  // keep the advertised one for messages
            return;
        }
        finish(x, Failed, tr("Could not connect to the peer at %1:%2 (%3)")
            .arg(x->triedLoopback ? QString("127.0.0.1") : host).arg(x->remotePort).arg(x->socket->errorString()));
    });
    t->socket->connectToHost(t->remoteHost, t->remotePort);
}

void DccManager::onConnected(Transfer* t)
{
    int id = t->info.id;
    t->timer->stop();
    t->info.state = Active;
    t->info.startMs = QDateTime::currentMSecsSinceEpoch();
    t->socket->setSocketOption(QAbstractSocket::LowDelayOption, t->info.direction == Chat ? 1 : 0);

    connect(t->socket, &QTcpSocket::readyRead, this, [this, id]() {
        if (Transfer* x = find(id))
            onReadyRead(x);
    });
    connect(t->socket, &QTcpSocket::disconnected, this, [this, id]() {
        Transfer* x = find(id);
        if (!x || isFinished(x->info.state))
            return;
        if (x->info.direction == Chat) {
            emit chatClosed(x->server, x->info.nick, tr("the other side closed the chat"));
            finish(x, Done);
        } else if (x->info.direction == Receive) {
            onReadyRead(x);  // drain anything still buffered
            if (isFinished(x->info.state))
                return;
            if (x->info.size > 0 && x->info.bytesDone < x->info.size)
                finish(x, Failed, tr("The sender closed the connection after %1 of %2")
                    .arg(formatSize(x->info.bytesDone), formatSize(x->info.size)));
            else
                finish(x, Done);
        } else {
            if (x->fileDone && x->socket->bytesToWrite() == 0)
                finish(x, Done);
            else
                finish(x, Failed, tr("The peer closed the connection"));
        }
    });

    if (t->info.direction == Chat) {
        emit chatOpened(t->server, t->info.nick);
        return;
    }

    t->file = new QFile(t->info.path);
    if (t->info.direction == Send) {
        if (!t->file->open(QIODevice::ReadOnly) || (t->position > 0 && !t->file->seek(t->position))) {
            finish(t, Failed, tr("Could not open the file"));
            return;
        }
        t->info.bytesDone = t->position;
        t->info.startBytes = t->position;
        connect(t->socket, &QTcpSocket::bytesWritten, this, [this, id](qint64 n) {
            Transfer* x = find(id);
            if (!x || x->info.state != Active)
                return;
            x->info.bytesDone += n;
            pumpSend(x);
        });
        armTimer(t, kStallTimeoutMs, tr("The transfer stalled"));
        pumpSend(t);
    } else {
        // Note: for QFile, WriteOnly on its own implies Truncate, which would wipe
        // the partial file we are resuming; ReadWrite keeps its contents.
        QIODevice::OpenMode mode = t->position > 0 ? QIODevice::ReadWrite
                                                   : (QIODevice::WriteOnly | QIODevice::Truncate);
        if (!t->file->open(mode)) {
            finish(t, Failed, tr("Could not create the file in the download folder"));
            return;
        }
        if (t->position > 0 && (!t->file->resize(t->position) || !t->file->seek(t->position))) {
            finish(t, Failed, tr("Could not resume the file"));
            return;
        }
        t->info.bytesDone = t->position;
        t->info.startBytes = t->position;
        armTimer(t, kStallTimeoutMs, tr("The transfer stalled (no data for 2 minutes)"));
        onReadyRead(t);
    }
}

void DccManager::pumpSend(Transfer* t)
{
    t->timer->start(kStallTimeoutMs);  // progress (or a fresh start) resets the stall timer
    while (!t->fileDone && t->socket->bytesToWrite() < kMaxQueued) {
        QByteArray chunk = t->file->read(kChunk);
        if (chunk.isEmpty()) {
            t->fileDone = true;
            break;
        }
        t->socket->write(chunk);
    }
    if (t->fileDone && t->socket->bytesToWrite() == 0) {
        // Everything is out; give the receiver a moment to acknowledge it all.
        if (t->lastAck == quint32(t->info.size & 0xFFFFFFFF)) {
            finish(t, Done);
            return;
        }
        int id = t->info.id;
        t->timer->disconnect();
        connect(t->timer, &QTimer::timeout, this, [this, id]() {
            if (Transfer* x = find(id))
                finish(x, Done);  // all data was sent; the ack just never came
        });
        t->timer->start(kFinalAckWaitMs);
    }
}

void DccManager::onReadyRead(Transfer* t)
{
    if (!t->socket)
        return;
    QByteArray data = t->socket->readAll();
    if (data.isEmpty())
        return;

    if (t->info.direction == Send) {
        // Acks: 4-byte big-endian running totals.
        t->inbuf += data;
        while (t->inbuf.size() >= 4) {
            t->lastAck = qFromBigEndian<quint32>(t->inbuf.constData());
            t->inbuf.remove(0, 4);
        }
        if (t->fileDone && t->socket->bytesToWrite() == 0 && t->lastAck == quint32(t->info.size & 0xFFFFFFFF))
            finish(t, Done);
        return;
    }

    if (t->info.direction == Chat) {
        t->inbuf += data;
        int nl;
        while ((nl = t->inbuf.indexOf('\n')) >= 0 || t->inbuf.size() > 8192) {
            QByteArray raw = nl >= 0 ? t->inbuf.left(nl) : t->inbuf.left(8192);
            t->inbuf.remove(0, nl >= 0 ? nl + 1 : 8192);
            raw.replace("\r", "");
            QString line = QString::fromUtf8(raw);
            bool action = false;
            if (line.startsWith("\x01" "ACTION ")) {
                line = line.mid(8);
                line.remove(QChar(1));
                action = true;
            } else if (line.startsWith(QChar(1))) {
                continue;
            }
            emit chatLine(t->server, t->info.nick, line, action);
        }
        return;
    }

    // Receive
    t->timer->start(kStallTimeoutMs);
    qint64 n = data.size();
    if (t->info.size > 0 && t->info.bytesDone + n > t->info.size)
        n = t->info.size - t->info.bytesDone;  // never write past the announced size
    if (n > 0 && t->file->write(data.constData(), n) != n) {
        finish(t, Failed, tr("Could not write to disk (disk full?)"));
        return;
    }
    t->info.bytesDone += n;
    char ack[4];
    qToBigEndian<quint32>(quint32(t->info.bytesDone & 0xFFFFFFFF), ack);
    t->socket->write(ack, 4);
    if (t->info.size > 0 && t->info.bytesDone >= t->info.size) {
        t->socket->flush();
        finish(t, Done);
    }
}

void DccManager::finish(Transfer* t, State state, const QString& error)
{
    if (isFinished(t->info.state))
        return;
    t->info.state = state;
    if (!error.isEmpty() && t->info.error.isEmpty())
        t->info.error = error;
    t->info.endMs = QDateTime::currentMSecsSinceEpoch();
    t->timer->stop();
    t->timer->disconnect();
    releaseListenPort(t);
    if (t->file) {
        t->file->close();
        delete t->file;
        t->file = nullptr;
    }
    if (t->socket) {
        QTcpSocket* sock = t->socket;
        t->socket = nullptr;
        sock->disconnect(this);
        if (state == Done && t->info.direction != Chat)
            sock->disconnectFromHost();  // flush final acks / data
        else
            sock->abort();
        sock->deleteLater();
    }

    const Info& i = t->info;
    if (i.direction == Chat) {
        if (state == Failed)
            log(t->server, tr("--- [DCC] The DCC CHAT with %1 could not be opened: %2.").arg(i.nick, i.error));
        return;
    }
    if (state == Done) {
        double secs = i.startMs > 0 ? (i.endMs - i.startMs) / 1000.0 : 0;
        QString line = tr("--- [DCC] %1 \"%2\" %3 %4 (%5")
            .arg(i.direction == Send ? tr("Sent") : tr("Received"), i.fileName,
                 i.direction == Send ? tr("to") : tr("from"), i.nick, formatSize(i.bytesDone));
        if (secs > 0.5)
            line += QString(", %1/s").arg(formatSize(qint64((i.bytesDone - i.startBytes) / secs)));
        line += ")";
        if (i.direction == Receive) {
            line += tr(", saved to %1").arg(i.path);
            Notifier::notify(tr("DCC download complete"), tr("%1 from %2").arg(i.fileName, i.nick));
        }
        log(t->server, line + ".");
    } else if (state == Cancelled) {
        log(t->server, tr("--- [DCC] Transfer of \"%1\" with %2 was cancelled.").arg(i.fileName, i.nick));
    } else {
        log(t->server, tr("--- [DCC] Transfer of \"%1\" with %2 failed: %3.").arg(i.fileName, i.nick, i.error));
    }
}

// -----------------------------------------------------------------------------
// Incoming CTCP
// -----------------------------------------------------------------------------

bool DccManager::handleCtcp(void* server, const QString& nick, const QString& ctcp)
{
    if (!ctcp.startsWith("DCC ", Qt::CaseInsensitive))
        return false;
    QString type, name;
    QStringList args;
    if (!parseDccRequest(ctcp, type, name, args)) {
        log(server, tr("--- [DCC] Ignored a malformed DCC request from %1").arg(nick));
        return true;
    }

    if (type == "SEND") {
        if (args.size() < 3) {
            log(server, tr("--- [DCC] Ignored a malformed DCC SEND from %1").arg(nick));
            return true;
        }
        QString host = dccHostToString(args[0]);
        qulonglong portValue = args[1].toULongLong();
        qint64 size = args[2].toLongLong();
        QString token = args.value(3);
        if (portValue > 65535)
            return true;
        quint16 port = quint16(portValue);
        if (!token.isEmpty() && port != 0) {
            for (Transfer* t : fTransfers) {
                if (t->info.direction == Send && t->info.passive && t->info.state == Waiting
                    && t->token == token && t->info.nick.compare(nick, Qt::CaseInsensitive) == 0 && !t->socket) {
                    if (port < 1024) {
                        finish(t, Failed, tr("The peer asked us to connect to a privileged port"));
                        return true;
                    }
                    t->remoteHost = host;
                    t->remotePort = port;
                    startConnecting(t);
                    return true;
                }
            }
        }
        handleIncomingSend(server, nick, name, host, port, size, token);
        return true;
    }

    if (type == "RESUME") {
        if (args.size() < 2)
            return true;
        quint16 port = quint16(args[0].toUInt());
        qint64 pos = args[1].toLongLong();
        QString token = args.value(2);
        for (Transfer* t : fTransfers) {
            if (t->info.direction != Send || t->info.state != Waiting || t->info.nick.compare(nick, Qt::CaseInsensitive) != 0)
                continue;
            bool match = t->info.passive ? (!token.isEmpty() && t->token == token) : (port != 0 && t->listenPort == port);
            if (!match)
                continue;
            if (pos <= 0 || pos >= t->info.size)
                return true;
            t->position = pos;
            QString reply = QString("DCC ACCEPT %1 %2 %3").arg(quoteFileName(t->info.fileName))
                .arg(t->info.passive ? 0 : t->listenPort).arg(pos);
            if (t->info.passive)
                reply += " " + t->token;
            emit sendCtcp(t->server, t->info.nick, reply);
            return true;
        }
        return true;
    }

    if (type == "ACCEPT") {
        if (args.size() < 2)
            return true;
        quint16 port = quint16(args[0].toUInt());
        qint64 pos = args[1].toLongLong();
        for (Transfer* t : fTransfers) {
            if (t->info.direction == Receive && t->resumeRequested && t->info.state == Waiting
                && t->remotePort == port && t->info.nick.compare(nick, Qt::CaseInsensitive) == 0) {
                t->resumeRequested = false;
                if (pos < 0 || pos > t->position) {
                    finish(t, Failed, tr("The sender answered the resume request with a bad offset"));
                    return true;
                }
                t->position = pos;
                startConnecting(t);
                return true;
            }
        }
        return true;
    }

    if (type == "CHAT") {
        if (args.size() < 2)
            return true;
        QString host = dccHostToString(args[0]);
        qulonglong portValue = args[1].toULongLong();
        QString token = args.value(2);
        if (portValue > 65535)
            return true;
        quint16 port = quint16(portValue);
        if (!token.isEmpty() && port != 0) {
            for (Transfer* t : fTransfers) {
                if (t->info.direction == Chat && t->info.passive && t->info.state == Waiting && !t->socket
                    && t->token == token && t->info.nick.compare(nick, Qt::CaseInsensitive) == 0) {
                    if (port < 1024)
                        return true;
                    t->remoteHost = host;
                    t->remotePort = port;
                    startConnecting(t);
                    return true;
                }
            }
        }
        handleIncomingChat(server, nick, host, port, token);
        return true;
    }

    log(server, tr("--- [DCC] Ignored an unsupported DCC %1 request from %2").arg(type, nick));
    return true;
}

void DccManager::handleIncomingSend(void* server, const QString& nick, const QString& fileName,
    const QString& host, quint16 port, qint64 size, const QString& token)
{
    bool passive = port == 0;
    if (!passive && port < 1024) {
        log(server, tr("--- [DCC] Refused a file offer from %1: it pointed at a privileged port (%2).").arg(nick).arg(port));
        return;
    }
    if ((passive && token.isEmpty()) || size < 0)
        return;
    int pending = 0;
    for (Transfer* t : fTransfers)
        if (t->info.state == Offered && t->info.nick.compare(nick, Qt::CaseInsensitive) == 0)
            ++pending;
    if (pending >= kMaxPendingPerNick)
        return;

    Transfer* t = newTransfer(Receive, server, nick);
    t->info.state = Offered;
    t->info.fileName = sanitizeFileName(fileName);
    t->info.size = size;
    t->info.passive = passive;
    t->token = token;
    t->remoteHost = host;
    t->remotePort = port;
    int id = t->info.id;

    QFileInfo existing(fDownloadDir + "/" + t->info.fileName);
    bool canResume = !passive && size > 0 && existing.isFile() && existing.size() > 0 && existing.size() < size;

    log(server, tr("--- [DCC] %1 offers to send you \"%2\" (%3).").arg(nick, t->info.fileName, formatSize(size)));

    QString text = tr("%1 wants to send you a file:\n\n%2\n%3\n\n").arg(nick, t->info.fileName, formatSize(size));
    text += passive ? tr("(Passive DCC: you will listen for the sender's connection.)\n\n")
                    : tr("From %1:%2\n\n").arg(host).arg(port);
    if (canResume)
        text += tr("A partial copy is already in your download folder; \"Resume\" continues it.\n\n");
    text += tr("Only accept files from people you trust.");

    auto* box = new QMessageBox(QMessageBox::Question, tr("DCC File Offer"), text, QMessageBox::NoButton, fParent);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->setWindowModality(Qt::NonModal);
    QPushButton* decline = box->addButton(tr("Decline"), QMessageBox::RejectRole);
    QPushButton* resume = canResume ? box->addButton(tr("Resume"), QMessageBox::AcceptRole) : nullptr;
    QPushButton* accept = box->addButton(canResume ? tr("Accept as New Copy") : tr("Accept"), QMessageBox::AcceptRole);
    box->setDefaultButton(canResume ? resume : accept);
    box->setEscapeButton(decline);
    connect(box, &QMessageBox::buttonClicked, this, [this, id, decline, resume](QAbstractButton* b) {
        Transfer* x = find(id);
        if (!x || x->info.state != Offered)
            return;
        if (b == decline) {
            x->info.error = tr("Declined");
            finish(x, Cancelled);
            return;
        }
        acceptOffer(id, b == resume);
    });
    box->show();
}

void DccManager::acceptOffer(int id, bool resume)
{
    Transfer* t = find(id);
    if (!t || t->info.state != Offered)
        return;
    QDir().mkpath(fDownloadDir);
    showTransfers();

    if (resume) {
        QFileInfo existing(fDownloadDir + "/" + t->info.fileName);
        if (existing.isFile() && existing.size() < t->info.size) {
            t->info.path = existing.filePath();
            t->position = existing.size();
        } else {
            resume = false;
        }
    }
    if (!resume)
        t->info.path = uniquePath(fDownloadDir, t->info.fileName);

    if (resume) {
        t->resumeRequested = true;
        t->info.state = Waiting;
        emit sendCtcp(t->server, t->info.nick, QString("DCC RESUME %1 %2 %3")
            .arg(quoteFileName(t->info.fileName)).arg(t->remotePort).arg(t->position));
        log(t->server, tr("--- [DCC] Asking %1 to resume \"%2\".").arg(t->info.nick, t->info.fileName));
        return;
    }
    if (!t->info.passive) {
        startConnecting(t);
        return;
    }
    // Passive offer: we listen and tell the sender where to connect.
    if (!allocListenPort(t)) {
        finish(t, Failed, tr("No free DCC port (all listen ports are busy)"));
        return;
    }
    QString ip = advertisedIP();
    if (ip.isEmpty()) {
        ip = ipv4ToString(localIPv4());
        log(t->server, tr("--- [DCC] Warning: you don't seem to be reachable from the internet, and neither is %1 "
            "(passive DCC). The transfer will probably only work on a LAN.").arg(t->info.nick));
    }
    startListening(t);
    emit sendCtcp(t->server, t->info.nick, QString("DCC SEND %1 %2 %3 %4 %5").arg(quoteFileName(t->info.fileName))
        .arg(parseIPv4(ip)).arg(t->listenPort).arg(t->info.size).arg(t->token));
}

// -----------------------------------------------------------------------------
// Outgoing
// -----------------------------------------------------------------------------

void DccManager::offerFile(void* server, const QString& nick, const QString& path)
{
    QFileInfo fi(path);
    if (!fi.isFile() || !fi.isReadable()) {
        log(server, tr("--- [DCC] Can't send \"%1\": not a readable file.").arg(path));
        return;
    }
    QString fileName = sanitizeFileName(fi.fileName());
    bool passive = usePassive();
    QString ip = advertisedIP();
    if (!passive && ip.isEmpty())
        passive = true;

    Transfer* t = newTransfer(Send, server, nick);
    t->info.fileName = fileName;
    t->info.path = fi.absoluteFilePath();
    t->info.size = fi.size();
    t->info.passive = passive;
    t->info.state = Waiting;

    QString ctcp;
    if (passive) {
        t->token = makeToken(t->info.id);
        quint32 advertised = parseIPv4(ip);
        if (advertised == 0)
            advertised = localIPv4();
        ctcp = QString("DCC SEND %1 %2 0 %3 %4").arg(quoteFileName(fileName)).arg(advertised).arg(fi.size()).arg(t->token);
    } else {
        if (!allocListenPort(t)) {
            finish(t, Failed, tr("No free DCC port (all listen ports are busy)"));
            return;
        }
        startListening(t);
        ctcp = QString("DCC SEND %1 %2 %3 %4").arg(quoteFileName(fileName)).arg(parseIPv4(ip)).arg(t->listenPort).arg(fi.size());
    }
    emit sendCtcp(server, nick, ctcp);
    log(server, tr("--- [DCC] Offering \"%1\" (%2) to %3%4. Waiting for them to accept…")
        .arg(fileName, formatSize(fi.size()), nick, passive ? tr(" using passive DCC") : QString()));
    if (passive && !fForcePassive)
        log(server, tr("--- [DCC] You don't seem to be reachable from the internet, so this uses passive DCC. "
            "Clients without passive DCC support (Vision, for one) can't receive it. If %1 is on your own network, "
            "set Preferences → DCC → External IP override to this computer's LAN address (or 127.0.0.1 "
            "for the same computer).").arg(nick));
    showTransfers();
}

// -----------------------------------------------------------------------------
// DCC CHAT
// -----------------------------------------------------------------------------

void DccManager::offerChat(void* server, const QString& nick)
{
    for (Transfer* t : fTransfers) {
        if (t->info.direction == Chat && t->server == server && t->info.nick.compare(nick, Qt::CaseInsensitive) == 0
            && !isFinished(t->info.state)) {
            log(server, tr("--- [DCC] A DCC CHAT with %1 is already open or pending.").arg(nick));
            return;
        }
    }
    bool passive = usePassive();
    QString ip = advertisedIP();
    if (!passive && ip.isEmpty())
        passive = true;

    Transfer* t = newTransfer(Chat, server, nick);
    t->info.fileName = "DCC CHAT";
    t->info.passive = passive;
    t->info.state = Waiting;
    QString ctcp;
    if (passive) {
        t->token = makeToken(t->info.id);
        quint32 advertised = parseIPv4(ip);
        if (advertised == 0)
            advertised = localIPv4();
        ctcp = QString("DCC CHAT chat %1 0 %2").arg(advertised).arg(t->token);
    } else {
        if (!allocListenPort(t)) {
            finish(t, Failed, tr("No free DCC port"));
            return;
        }
        startListening(t);
        ctcp = QString("DCC CHAT chat %1 %2").arg(parseIPv4(ip)).arg(t->listenPort);
    }
    emit sendCtcp(server, nick, ctcp);
    log(server, tr("--- [DCC] Offering a DCC CHAT to %1%2. Waiting for them to accept…")
        .arg(nick, passive ? tr(" (passive DCC)") : QString()));
}

void DccManager::handleIncomingChat(void* server, const QString& nick, const QString& host,
    quint16 port, const QString& token)
{
    bool passive = port == 0;
    if (!passive && port < 1024) {
        log(server, tr("--- [DCC] Refused a DCC CHAT from %1: it pointed at a privileged port (%2).").arg(nick).arg(port));
        return;
    }
    if (passive && token.isEmpty())
        return;
    int pending = 0;
    for (Transfer* t : fTransfers)
        if (t->info.state == Offered && t->info.nick.compare(nick, Qt::CaseInsensitive) == 0)
            ++pending;
    if (pending >= kMaxPendingPerNick)
        return;

    Transfer* t = newTransfer(Chat, server, nick);
    t->info.state = Offered;
    t->info.fileName = "DCC CHAT";
    t->info.passive = passive;
    t->token = token;
    t->remoteHost = host;
    t->remotePort = port;
    int id = t->info.id;
    log(server, tr("--- [DCC] %1 offers a DCC CHAT.").arg(nick));

    QString text = tr("%1 wants to chat with you directly (DCC CHAT).\n\nThe chat bypasses the IRC server and "
        "connects your computers directly, so each of you learns the other's IP address.").arg(nick);
    if (!passive)
        text += tr("\n\nFrom %1:%2").arg(host).arg(port);
    auto* box = new QMessageBox(QMessageBox::Question, tr("DCC Chat Offer"), text, QMessageBox::NoButton, fParent);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->setWindowModality(Qt::NonModal);
    QPushButton* decline = box->addButton(tr("Decline"), QMessageBox::RejectRole);
    QPushButton* accept = box->addButton(tr("Accept"), QMessageBox::AcceptRole);
    box->setDefaultButton(accept);
    box->setEscapeButton(decline);
    connect(box, &QMessageBox::buttonClicked, this, [this, id, accept](QAbstractButton* b) {
        Transfer* x = find(id);
        if (!x || x->info.state != Offered)
            return;
        if (b == accept) {
            acceptChat(id);
        } else {
            x->info.error = tr("Declined");
            log(x->server, tr("--- [DCC] Declined a DCC CHAT from %1.").arg(x->info.nick));
            finish(x, Cancelled);
        }
    });
    box->show();
}

void DccManager::acceptChat(int id)
{
    Transfer* t = find(id);
    if (!t || t->info.state != Offered)
        return;
    if (!t->info.passive) {
        startConnecting(t);
        return;
    }
    if (!allocListenPort(t)) {
        finish(t, Failed, tr("No free DCC port"));
        return;
    }
    QString ip = advertisedIP();
    if (ip.isEmpty())
        ip = ipv4ToString(localIPv4());
    startListening(t);
    emit sendCtcp(t->server, t->info.nick, QString("DCC CHAT chat %1 %2 %3").arg(parseIPv4(ip)).arg(t->listenPort).arg(t->token));
}

bool DccManager::chatSend(void* server, const QString& nick, const QString& text, bool action)
{
    QString line = text;
    line.remove('\r');
    line.remove('\n');
    if (action)
        line = "\x01" "ACTION " + line + "\x01";
    for (Transfer* t : fTransfers) {
        if (t->info.direction == Chat && t->server == server && t->info.state == Active && t->socket
            && t->info.nick.compare(nick, Qt::CaseInsensitive) == 0) {
            t->socket->write(line.toUtf8() + "\n");
            return true;
        }
    }
    return false;
}

void DccManager::closeChat(void* server, const QString& nick)
{
    for (Transfer* t : fTransfers) {
        if (t->info.direction == Chat && t->server == server && t->info.nick.compare(nick, Qt::CaseInsensitive) == 0
            && !isFinished(t->info.state)) {
            bool wasOpen = t->info.state == Active;
            finish(t, Cancelled);
            if (wasOpen)
                emit chatClosed(server, nick, tr("you closed the chat"));
        }
    }
}

void DccManager::forgetServer(void* server)
{
    for (Transfer* t : fTransfers) {
        if (t->server == server) {
            if (t->info.direction == Chat && !isFinished(t->info.state))
                finish(t, Cancelled);
            t->server = nullptr;
        }
    }
}

// -----------------------------------------------------------------------------
// Dialog support
// -----------------------------------------------------------------------------

QList<DccManager::Info> DccManager::snapshot() const
{
    QList<Info> out;
    for (Transfer* t : fTransfers)
        if (t->info.direction != Chat)
            out << t->info;
    return out;
}

QString DccManager::statusText() const
{
    QString text = fMapStatus;
    if (!fReachStatus.isEmpty())
        text += "  " + fReachStatus;
    if (fForcePassive)
        text += tr("  (Passive DCC is forced on.)");
    if (!fExternalIP.isEmpty())
        text += tr("  (Advertising %1.)").arg(fExternalIP);
    return text;
}

void DccManager::cancel(int id)
{
    Transfer* t = find(id);
    if (!t || isFinished(t->info.state))
        return;
    if (t->info.state == Offered)
        t->info.error = tr("Declined");
    finish(t, Cancelled);
}

void DccManager::clearFinished()
{
    for (int i = fTransfers.size() - 1; i >= 0; --i) {
        Transfer* t = fTransfers[i];
        if (isFinished(t->info.state)) {
            t->timer->deleteLater();
            delete t;
            fTransfers.removeAt(i);
        }
    }
}

QString DccManager::downloadDir() const
{
    return fDownloadDir;
}

void DccManager::showTransfers()
{
    if (!fDialog)
        fDialog = new DccTransfersDialog(this, fParent);
    fDialog->show();
    fDialog->raise();
    fDialog->activateWindow();
}
