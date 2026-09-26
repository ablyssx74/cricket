/*
 * Cricket IRC Client - Linux port
 * Distributed under the terms of the MIT license.
 */
#include "mainwindow.h"

#include "chatview.h"
#include "dialogs.h"
#include "inputedit.h"
#include "ircconnection.h"
#include "ircformat.h"
#include "services.h"

#include <QAbstractSocket>
#include <QApplication>
#include <QCloseEvent>
#include <QColorDialog>
#include <QDateTime>
#include <QDesktopServices>
#include <QFile>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QWidgetAction>

Q_DECLARE_METATYPE(Buffer*)

ServerConfig& Session::profile() const
{
    ServerConfig* p = serverProfile(custom, index);
    Q_ASSERT(p);
    return *p;
}

static const int kMaxLines = 3000;

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle("Cricket");
    setWindowIcon(QIcon::fromTheme("cricket", QIcon(":/cricket.svg")));
    fTranslator = new Translator(this);
    fUpdates = new UpdateChecker(this);

    buildUi();
    buildMenus();

    for (int i = 0; i < cfg.servers.size(); ++i)
        createSession(false, i);
    for (int i = 0; i < cfg.customServers.size(); ++i)
        createSession(true, i);

    if (!fSessions.isEmpty())
        setActiveBuffer(fSessions.first()->server);

    QSettings settings;
    restoreGeometry(settings.value("window/geometry").toByteArray());
    if (!fSplitter->restoreState(settings.value("window/splitter").toByteArray()))
        fSplitter->setSizes({180, 700, 170});

    connect(fUpdates, &UpdateChecker::updateAvailable, this, [this](const QString& version) {
        if (!cfg.showUpdateNotifications)
            return;
        QString text = tr("A newer version of Cricket is available! (%1)").arg(version);
        Notifier::notify(tr("Update Available"), text);
        if (!fSessions.isEmpty())
            printStatus(fSessions.first(), "--- " + text);
    });
    fUpdates->checkLater();

    printStatus(fSessions.value(0), QString("--- Welcome to %1. Double-click a network to connect.").arg(AppInfo::VERSION_STRING));
}

MainWindow::~MainWindow()
{
    for (Session* s : fSessions) {
        qDeleteAll(s->buffers);
        delete s->server;
        delete s;
    }
}

void MainWindow::buildUi()
{
    fSplitter = new QSplitter(Qt::Horizontal);

    fTree = new QTreeWidget;
    fTree->setHeaderHidden(true);
    fTree->setRootIsDecorated(true);
    fTree->setContextMenuPolicy(Qt::CustomContextMenu);
    fTree->setMinimumWidth(120);
    connect(fTree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* cur) {
        if (Buffer* b = bufferForItem(cur))
            setActiveBuffer(b);
    });
    connect(fTree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item) {
        Buffer* b = bufferForItem(item);
        if (b && b->type == Buffer::Server && !b->session->conn->isConnected())
            connectSession(b->session);
    });
    connect(fTree, &QTreeWidget::customContextMenuRequested, this, &MainWindow::showTreeMenu);

    auto* center = new QWidget;
    auto* cl = new QVBoxLayout(center);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(4);
    fTopic = new QLineEdit;
    fTopic->setPlaceholderText(tr("No topic"));
    connect(fTopic, &QLineEdit::returnPressed, this, [this]() {
        if (!fActive || fActive->type != Buffer::Channel || !fActive->session->conn->isRegistered())
            return;
        QString t = fTopic->text();
        if (t != IrcFormat::stripCodes(fActive->topic))
            fActive->session->conn->sendRaw(QString("TOPIC %1 :%2").arg(fActive->name, t));
        fInput->setFocus();
    });
    fChat = new ChatView;
    connect(fChat, &ChatView::searchRequested, this, [this](const QString& t) { openSearch(t, false); });
    connect(fChat, &ChatView::translateRequested, this, [this](const QString& t) { openSearch(t, true); });

    auto* inputRow = new QHBoxLayout;
    fEmoteButton = new QToolButton;
    fEmoteButton->setText("🙂");
    fEmoteButton->setToolTip(tr("Emoticons"));
    fEmoteButton->setPopupMode(QToolButton::InstantPopup);
    auto* emoteMenu = new QMenu(fEmoteButton);
    auto* grid = new QWidget;
    auto* gl = new QGridLayout(grid);
    gl->setSpacing(2);
    int col = 0, row = 0;
    for (const IrcFormat::Emote& e : IrcFormat::pickerEmotes()) {
        auto* btn = new QToolButton;
        btn->setText(QString::fromUtf8(e.emoji));
        btn->setToolTip(QString("%1  %2").arg(e.name, e.trigger));
        btn->setAutoRaise(true);
        QFont f = btn->font();
        f.setPointSizeF(f.pointSizeF() * 1.5);
        btn->setFont(f);
        QString trigger = QString::fromUtf8(e.trigger);
        connect(btn, &QToolButton::clicked, this, [this, trigger, emoteMenu]() {
            emoteMenu->close();
            fInput->insertAtCursor(trigger + " ");
        });
        gl->addWidget(btn, row, col);
        if (++col == 5) { col = 0; ++row; }
    }
    auto* wa = new QWidgetAction(emoteMenu);
    wa->setDefaultWidget(grid);
    emoteMenu->addAction(wa);
    fEmoteButton->setMenu(emoteMenu);

    fInput = new InputEdit;
    fInput->setSpellCheckEnabled(cfg.enableSpellCheck);
    fInput->setCompletionSource([this]() {
        QStringList out;
        if (!fActive)
            return out;
        for (const ChannelUser& u : fActive->users)
            out << u.nick;
        if (fActive->type == Buffer::Query)
            out << fActive->name;
        if (fActive->type == Buffer::Channel)
            out << fActive->name;
        return out;
    });
    connect(fInput, &InputEdit::submitted, this, &MainWindow::onSubmit);
    inputRow->addWidget(fEmoteButton);
    inputRow->addWidget(fInput);

    cl->addWidget(fTopic);
    cl->addWidget(fChat, 1);
    cl->addLayout(inputRow);

    fUserPanel = new QWidget;
    auto* ul = new QVBoxLayout(fUserPanel);
    ul->setContentsMargins(0, 0, 0, 0);
    ul->setSpacing(4);
    fUserCount = new QLabel;
    fUsers = new QListWidget;
    fUsers->setContextMenuPolicy(Qt::CustomContextMenu);
    fUsers->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(fUsers, &QListWidget::customContextMenuRequested, this, &MainWindow::showUserMenu);
    connect(fUsers, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        if (!fActive)
            return;
        Buffer* q = ensureBuffer(fActive->session, item->data(Qt::UserRole).toString(), Buffer::Query, true);
        Q_UNUSED(q);
    });
    ul->addWidget(fUserCount);
    ul->addWidget(fUsers, 1);

    fSplitter->addWidget(fTree);
    fSplitter->addWidget(center);
    fSplitter->addWidget(fUserPanel);
    fSplitter->setStretchFactor(1, 1);
    fSplitter->setChildrenCollapsible(false);

    auto* wrapper = new QWidget;
    auto* wl = new QVBoxLayout(wrapper);
    wl->setContentsMargins(6, 6, 6, 6);
    wl->addWidget(fSplitter);
    setCentralWidget(wrapper);

    fStatusLabel = new QLabel;
    statusBar()->addWidget(fStatusLabel, 1);

    // Typing anywhere in the chat log goes to the input line.
    fChat->installEventFilter(this);
    resize(1100, 700);
}

void MainWindow::buildMenus()
{
    QMenu* app = menuBar()->addMenu(tr("&Cricket"));
    app->addAction(QIcon::fromTheme("configure"), tr("&Preferences…"), this, [this]() {
        SettingsDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            dlg.apply();
            fInput->setSpellCheckEnabled(cfg.enableSpellCheck);
        }
    });
    app->addSeparator();
    QAction* quit = app->addAction(QIcon::fromTheme("application-exit"), tr("&Quit"), this, &QWidget::close);
    quit->setShortcut(QKeySequence::Quit);

    QMenu* server = menuBar()->addMenu(tr("&Server"));
    server->addAction(QIcon::fromTheme("network-connect"), tr("&Connect"), this, [this]() {
        if (Session* s = currentSession()) connectSession(s);
    });
    server->addAction(QIcon::fromTheme("network-disconnect"), tr("&Disconnect"), this, [this]() {
        if (Session* s = currentSession()) disconnectSession(s);
    });
    server->addSeparator();
    QAction* join = server->addAction(tr("&Join Channel…"), this, [this]() {
        if (Session* s = currentSession()) joinChannelPrompt(s);
    });
    join->setShortcut(QKeySequence("Ctrl+J"));
    server->addAction(tr("Channel &List…"), this, [this]() {
        if (Session* s = currentSession()) openChannelList(s);
    });
    fAwayAction = server->addAction(tr("Set &Away"), this, [this]() {
        if (Session* s = currentSession()) toggleAway(s);
    });
    fAwayAction->setCheckable(true);
    server->addSeparator();
    server->addAction(QIcon::fromTheme("configure"), tr("C&onfigure Server…"), this, [this]() {
        if (Session* s = currentSession()) configureSession(s);
    });
    server->addAction(QIcon::fromTheme("list-add"), tr("&Add Server…"), this, &MainWindow::addServer);

    QMenu* view = menuBar()->addMenu(tr("&View"));
    QAction* users = view->addAction(tr("Show &User List"));
    users->setCheckable(true);
    users->setChecked(true);
    connect(users, &QAction::toggled, fUserPanel, &QWidget::setVisible);
    QAction* clear = view->addAction(tr("&Clear Buffer"), this, [this]() {
        if (!fActive) return;
        fActive->lines.clear();
        fChat->clear();
    });
    clear->setShortcut(QKeySequence("Ctrl+L"));
    QAction* next = view->addAction(tr("&Next Buffer"), this, [this]() {
        QTreeWidgetItem* cur = fTree->currentItem();
        QTreeWidgetItem* n = cur ? fTree->itemBelow(cur) : nullptr;
        if (n) fTree->setCurrentItem(n);
    });
    next->setShortcut(QKeySequence("Alt+Down"));
    QAction* prev = view->addAction(tr("&Previous Buffer"), this, [this]() {
        QTreeWidgetItem* cur = fTree->currentItem();
        QTreeWidgetItem* p = cur ? fTree->itemAbove(cur) : nullptr;
        if (p) fTree->setCurrentItem(p);
    });
    prev->setShortcut(QKeySequence("Alt+Up"));

    QMenu* help = menuBar()->addMenu(tr("&Help"));
    help->addAction(tr("&Commands"), this, [this]() {
        if (!fActive) return;
        const char* lines[] = {
            "/join #chan [key]  /part [#chan] [reason]  /msg nick text  /query nick  /me action",
            "/nick newnick  /topic text  /whois nick  /notice target text  /ctcp nick VERSION",
            "/op /deop /voice /devoice nick  /kick nick [reason]  /ban nick  /mode ...  /invite nick",
            "/away [msg]  /back  /ignore mask  /unignore mask  /list  /names  /clear  /close",
            "/connect  /disconnect  /reconnect  /quote RAW LINE  (anything else is sent as-is)",
        };
        for (const char* l : lines)
            print(fActive, IrcFormat::escape(QString::fromUtf8(l)), LineKind::Status);
    });
    help->addAction(QIcon::fromTheme("help-about"), tr("&About Cricket"), this, &MainWindow::showAbout);
}

Session* MainWindow::createSession(bool custom, int index)
{
    auto* s = new Session;
    s->custom = custom;
    s->index = index;
    s->conn = new IrcConnection(s->profile(), this);
    s->server = new Buffer;
    s->server->type = Buffer::Server;
    s->server->name = s->profile().name;
    s->server->session = s;
    s->server->item = new QTreeWidgetItem(fTree, {s->profile().name});
    s->server->item->setData(0, Qt::UserRole, QVariant::fromValue(s->server));
    s->server->item->setIcon(0, QIcon::fromTheme("network-server"));
    s->server->item->setExpanded(true);
    fSessions << s;
    wireConnection(s);
    updateTreeItem(s->server);
    return s;
}

void MainWindow::wireConnection(Session* s)
{
    IrcConnection* c = s->conn;
    connect(c, &IrcConnection::messageReceived, this, [this, s](const IrcMessage& m) { onMessage(s, m); });
    connect(c, &IrcConnection::statusMessage, this, [this, s](const QString& t) {
        LineKind kind = t.contains("error", Qt::CaseInsensitive) || t.contains("failed", Qt::CaseInsensitive)
            ? LineKind::Error : LineKind::Status;
        printStatus(s, t, kind);
    });
    connect(c, &IrcConnection::registered, this, [this, s]() { onRegistered(s); });
    connect(c, &IrcConnection::disconnected, this, [this, s]() { onDisconnected(s); });
    connect(c, &IrcConnection::connected, this, [this, s]() { updateTreeItem(s->server); refreshStatus(); });
    connect(c, &IrcConnection::nickChanged, this, [this](const QString&) { refreshStatus(); });
    connect(c, &IrcConnection::rawTraffic, this, [s](const QString& dir, const QString& line) {
        QTextStream(stdout) << "[" << s->profile().name << "] " << dir << " " << line << Qt::endl;
    });
}

void MainWindow::autoConnect()
{
    for (Session* s : fSessions)
        if (s->profile().autoConnect)
            connectSession(s);
}

// ---------------------------------------------------------------------------
// Buffers
// ---------------------------------------------------------------------------

Buffer* MainWindow::bufferForItem(QTreeWidgetItem* item) const
{
    return item ? item->data(0, Qt::UserRole).value<Buffer*>() : nullptr;
}

Session* MainWindow::currentSession() const
{
    return fActive ? fActive->session : fSessions.value(0);
}

Buffer* MainWindow::findBuffer(Session* s, const QString& name) const
{
    for (Buffer* b : s->buffers)
        if (b->name.compare(name, Qt::CaseInsensitive) == 0)
            return b;
    return nullptr;
}

Buffer* MainWindow::ensureBuffer(Session* s, const QString& name, Buffer::Type type, bool focus)
{
    Buffer* b = findBuffer(s, name);
    if (!b) {
        b = new Buffer;
        b->type = type;
        b->name = name;
        b->session = s;
        b->item = new QTreeWidgetItem(s->server->item, {name});
        b->item->setData(0, Qt::UserRole, QVariant::fromValue(b));
        b->item->setIcon(0, QIcon::fromTheme(type == Buffer::Channel ? "irc-channel-active" : "im-user",
            QIcon::fromTheme(type == Buffer::Channel ? "user-group-new" : "user-identity")));
        s->buffers << b;
        s->server->item->setExpanded(true);
        updateTreeItem(b);
    }
    if (focus)
        setActiveBuffer(b);
    return b;
}

void MainWindow::removeBuffer(Buffer* b)
{
    if (!b || b->type == Buffer::Server)
        return;
    Session* s = b->session;
    if (fActive == b) {
        int idx = s->buffers.indexOf(b);
        Buffer* next = s->buffers.value(idx + 1, s->buffers.value(idx - 1, nullptr));
        if (next == b || !next)
            next = s->server;
        setActiveBuffer(next);
    }
    s->buffers.removeAll(b);
    delete b->item;
    delete b;
}

void MainWindow::setActiveBuffer(Buffer* b)
{
    if (!b)
        return;
    bool sessionChanged = !fActive || fActive->session != b->session;
    fActive = b;
    b->unread = false;
    b->highlighted = false;
    updateTreeItem(b);
    if (fTree->currentItem() != b->item) {
        QSignalBlocker block(fTree);
        fTree->setCurrentItem(b->item);
    }
    if (sessionChanged)
        applyAppearance();
    fChat->setLines(b->lines);
    refreshTopic();
    refreshUserList();
    refreshStatus();
    setWindowTitle(QString("%1 — %2 — Cricket").arg(b->name, b->session->profile().name));
    fInput->setFocus();
}

void MainWindow::updateTreeItem(Buffer* b)
{
    if (!b || !b->item)
        return;
    QFont f = fTree->font();
    QBrush fg = fTree->palette().brush(QPalette::Text);
    bool inactive = (b->type == Buffer::Server && !b->session->conn->isConnected())
        || (b->type == Buffer::Channel && !b->joined);
    if (inactive) {
        fg = fTree->palette().brush(QPalette::Disabled, QPalette::Text);
        f.setItalic(b->type == Buffer::Channel);
    }
    if (b->highlighted) {
        fg = QBrush(QColor(0xE0, 0x40, 0x40));
        f.setBold(true);
    } else if (b->unread) {
        fg = QBrush(QColor(0x3D, 0xAE, 0xE9)); // Plasma highlight blue
        f.setBold(true);
    }
    b->item->setFont(0, f);
    b->item->setForeground(0, fg);
    QString text = b->type == Buffer::Server ? b->session->profile().name : b->name;
    b->item->setText(0, text);
}

int MainWindow::rankOf(Session* s, const ChannelUser& u) const
{
    const QString symbols = s->conn->prefixSymbols();
    if (u.prefixes.isEmpty())
        return symbols.size();
    int idx = symbols.indexOf(u.prefixes.at(0));
    return idx < 0 ? symbols.size() : idx;
}

void MainWindow::refreshUserList()
{
    fUsers->clear();
    if (!fActive || fActive->type != Buffer::Channel) {
        fUserCount->setText(fActive && fActive->type == Buffer::Query ? tr("Private chat") : QString());
        return;
    }
    Session* s = fActive->session;
    QList<ChannelUser> list = fActive->users.values();
    std::sort(list.begin(), list.end(), [this, s](const ChannelUser& a, const ChannelUser& b) {
        int ra = rankOf(s, a), rb = rankOf(s, b);
        if (ra != rb)
            return ra < rb;
        return a.nick.compare(b.nick, Qt::CaseInsensitive) < 0;
    });
    const QPalette pal = fUsers->palette();
    for (const ChannelUser& u : list) {
        auto* item = new QListWidgetItem(u.prefixes.left(1) + u.nick);
        item->setData(Qt::UserRole, u.nick);
        if (u.away) {
            QFont f = item->font();
            f.setItalic(true);
            item->setFont(f);
            item->setForeground(pal.brush(QPalette::Disabled, QPalette::Text));
            item->setToolTip(tr("%1 (away)").arg(u.nick));
        }
        fUsers->addItem(item);
    }
    fUserCount->setText(list.size() == 1 ? tr("1 user") : tr("%1 users").arg(list.size()));
}

void MainWindow::refreshTopic()
{
    if (!fActive) {
        fTopic->clear();
        return;
    }
    fTopic->setReadOnly(fActive->type != Buffer::Channel);
    if (fActive->type == Buffer::Channel)
        fTopic->setText(IrcFormat::stripCodes(fActive->topic));
    else if (fActive->type == Buffer::Query)
        fTopic->setText(tr("Private conversation with %1").arg(fActive->name));
    else
        fTopic->setText(QString("%1 (%2:%3)").arg(fActive->session->profile().name,
            fActive->session->profile().host).arg(fActive->session->profile().port));
    fTopic->setCursorPosition(0);
    fTopic->setToolTip(fTopic->text());
}

void MainWindow::refreshStatus()
{
    Session* s = currentSession();
    if (!s) {
        fStatusLabel->clear();
        return;
    }
    QString state;
    if (s->conn->isRegistered())
        state = tr("Connected as %1").arg(s->conn->nick());
    else if (s->conn->isConnected())
        state = tr("Connecting…");
    else
        state = tr("Disconnected");
    if (s->away)
        state += tr(" (away)");
    fStatusLabel->setText(QString("%1 — %2").arg(s->profile().name, state));
    if (fAwayAction) {
        QSignalBlocker block(fAwayAction);
        fAwayAction->setChecked(s->away);
    }
}

void MainWindow::applyAppearance()
{
    Session* s = currentSession();
    if (!s)
        return;
    const ServerConfig& p = s->profile();
    QFont tf = QApplication::font();
    tf.setPointSize(p.serverListFontSize);
    fTree->setFont(tf);
    QFont cf = QApplication::font();
    cf.setPointSize(p.chatLogFontSize);
    fChat->setFont(cf);
    fChat->document()->setDefaultFont(cf);
    QFont uf = QApplication::font();
    uf.setPointSize(p.userListFontSize);
    fUsers->setFont(uf);
    fChat->setBackgroundImage(p.backgroundImagePath, p.backgroundOpacity);
    for (Session* ss : fSessions) {
        updateTreeItem(ss->server);
        for (Buffer* b : ss->buffers)
            updateTreeItem(b);
    }
}

void MainWindow::logToFile(Buffer* b, const QString& plainLine)
{
    if (!b || plainLine.isEmpty() || !b->session->profile().logChatsToFile)
        return;
    QString server = b->session->profile().name;
    server.replace('/', '_').replace(' ', '_');
    QString chan = b->type == Buffer::Server ? QString("server") : b->name;
    chan.replace('/', '_');
    QFile f(QString("%1/%2_%3.log").arg(logsDir(), server, chan));
    if (!f.open(QIODevice::Append | QIODevice::Text))
        return;
    QTextStream out(&f);
    out << QDateTime::currentDateTime().toString("[yyyy-MM-dd HH:mm:ss] ") << plainLine.trimmed() << "\n";
}

// ---------------------------------------------------------------------------
// Output
// ---------------------------------------------------------------------------

QString MainWindow::timestampFor(Buffer* b, bool force)
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    int interval = b->session->profile().timestampInterval;
    if (!force && interval > 0 && b->lastTimestamp && now - b->lastTimestamp < qint64(interval) * 60000)
        return QString();
    b->lastTimestamp = now;
    return QDateTime::currentDateTime().toString("[HH:mm] ");
}

static QString kindColor(int kind, bool dark)
{
    using K = int;
    switch (K(kind)) {
    case 1: return dark ? "#d7a0ff" : "#8a2be2"; // Action
    case 2: return dark ? "#e0b070" : "#8a5a00"; // Notice
    case 3: return dark ? "#9a9a9a" : "#6f6f6f"; // Status
    case 4: return dark ? "#7fd07f" : "#1e8a1e"; // Join
    case 5: return dark ? "#e08080" : "#a33030"; // Part
    case 6: return dark ? "#ff6b6b" : "#c00000"; // Error
    default: return QString();
    }
}

void MainWindow::print(Buffer* b, const QString& html, LineKind kind, const QString& plain)
{
    if (!b)
        return;
    const bool dark = fChat->isDark();
    QString line;
    const bool isChat = kind == LineKind::Normal || kind == LineKind::Action || kind == LineKind::Notice
        || kind == LineKind::Own || kind == LineKind::Highlight;
    if (isChat || kind == LineKind::Join || kind == LineKind::Part) {
        QString ts = timestampFor(b);
        if (!ts.isEmpty())
            line += QString("<span style=\"color:%1\">%2</span>").arg(dark ? "#8a8a8a" : "#8a8a8a", ts.toHtmlEscaped());
    }
    QString color = kindColor(int(kind), dark);
    if (kind == LineKind::Highlight)
        line += QString("<span style=\"background-color:%1\">%2</span>").arg(dark ? "#5a4a10" : "#fff2a8", html);
    else if (!color.isEmpty())
        line += QString("<span style=\"color:%1\">%2</span>").arg(color, html);
    else
        line += html;

    b->lines << line;
    bool trimmed = false;
    if (b->lines.size() > kMaxLines) {
        b->lines.erase(b->lines.begin(), b->lines.begin() + 500);
        trimmed = true;
    }
    if (b == fActive) {
        if (trimmed)
            fChat->setLines(b->lines);
        else
            fChat->appendLine(line);
    } else if (kind != LineKind::Status || b->type != Buffer::Server) {
        if (kind == LineKind::Highlight)
            b->highlighted = true;
        if (isChat || b->type != Buffer::Server)
            b->unread = true;
        updateTreeItem(b);
    }

    QString text = plain;
    if (text.isEmpty()) {
        QTextDocument doc;
        doc.setHtml(html);
        text = doc.toPlainText();
    }
    logToFile(b, text);
}

void MainWindow::printStatus(Session* s, const QString& text, LineKind kind)
{
    if (!s)
        return;
    print(s->server, IrcFormat::toHtml(text, true, false), kind, IrcFormat::stripCodes(text));
}

void MainWindow::printActiveOrServer(Session* s, const QString& text, LineKind kind)
{
    Buffer* b = (fActive && fActive->session == s) ? fActive : s->server;
    print(b, IrcFormat::toHtml(text, true, false), kind, IrcFormat::stripCodes(text));
}

QString MainWindow::nickHtml(Session* s, const QString& nick) const
{
    QColor color;
    const ServerConfig& p = s->profile();
    for (int i = 0; i < p.nickColors.size(); ++i) {
        if (matchWildcard(nick, p.nickColors.at(i))) {
            color = p.nickColorValues.value(i);
            break;
        }
    }
    if (!color.isValid())
        color = IrcFormat::nickColor(nick, fChat->isDark());
    return QString("<b style=\"color:%1\">%2</b>").arg(color.name(), nick.toHtmlEscaped());
}

QString MainWindow::formatBody(Session* s, const QString& body) const
{
    const ServerConfig& p = s->profile();
    return IrcFormat::toHtml(body, p.enableColorCodes, p.enableEmoticons);
}

bool MainWindow::mentionsNick(const QString& text, const QString& nick) const
{
    if (nick.isEmpty())
        return false;
    QRegularExpression re(QString(R"((?<![\w\[\]\\`^{}|-])%1(?![\w\[\]\\`^{}|-]))").arg(QRegularExpression::escape(nick)),
        QRegularExpression::CaseInsensitiveOption);
    return re.match(IrcFormat::stripCodes(text)).hasMatch();
}

void MainWindow::printMessage(Buffer* b, const QString& nick, const QString& body, LineKind kind, bool own)
{
    Session* s = b->session;
    QString html;
    QString plain;
    const QString clean = IrcFormat::stripCodes(body);
    if (kind == LineKind::Action) {
        html = QString("* %1 %2").arg(nickHtml(s, nick), formatBody(s, body));
        plain = QString("* %1 %2").arg(nick, clean);
    } else if (kind == LineKind::Notice) {
        html = QString("-%1- %2").arg(nickHtml(s, nick), formatBody(s, body));
        plain = QString("-%1- %2").arg(nick, clean);
    } else {
        html = QString("&lt;%1&gt; %2").arg(nickHtml(s, nick), formatBody(s, body));
        plain = QString("<%1> %2").arg(nick, clean);
    }
    if (own && kind == LineKind::Normal)
        kind = LineKind::Own;
    print(b, html, kind, plain);
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void MainWindow::onSubmit(const QString& text)
{
    if (!fActive || text.isEmpty())
        return;
    QStringList lines = text.split('\n');
    if (lines.size() > 3) {
        auto answer = QMessageBox::question(this, tr("Paste"),
            tr("Send %1 lines to %2?").arg(lines.size()).arg(fActive->name));
        if (answer != QMessageBox::Yes) {
            fInput->setText(text);
            return;
        }
    }
    for (const QString& l : lines)
        if (!l.isEmpty())
            sendLine(fActive, l);
}

void MainWindow::sendLine(Buffer* b, const QString& line)
{
    Session* s = b->session;
    if (line.startsWith('/') && !line.startsWith("//")) {
        processCommand(s, b, line);
        return;
    }
    QString text = line.startsWith("//") ? line.mid(1) : line;
    if (b->type == Buffer::Server) {
        print(b, tr("Use slash commands (like /join #channel) in the server log.").toHtmlEscaped(), LineKind::Error);
        return;
    }
    if (!s->conn->isRegistered()) {
        print(b, tr("Not connected. Message not sent.").toHtmlEscaped(), LineKind::Error);
        return;
    }

    ServerConfig& p = s->profile();
    bool reviewedResend = !fPendingReviewedTranslation.isEmpty() && text == fPendingReviewedTranslation;
    fPendingReviewedTranslation.clear();
    if (!reviewedResend && p.enableOutboundTranslation && !p.geminiApiKey.isEmpty()) {
        QPointer<MainWindow> self(this);
        QString target = b->name;
        bool autoSend = p.autoSendTranslatedOutbound;
        fInput->setText(text);
        fInput->setEnabled(false);
        fTranslator->translate(text, p.outboundTargetLanguage, p.geminiModel, p.geminiApiKey,
            [self, s, target, text, autoSend](bool ok, const QString& translated) {
                if (!self)
                    return;
                self->fInput->setEnabled(true);
                self->fInput->setFocus();
                if (!self->fSessions.contains(s))
                    return;
                Buffer* dest = self->findBuffer(s, target);
                if (!ok) {
                    if (dest)
                        self->print(dest, tr("Outbound translation failed; your message was not sent. "
                            "Press Enter to try again, or turn off outbound translation to send as-is.").toHtmlEscaped(),
                            LineKind::Error);
                    self->fInput->setText(text);
                    return;
                }
                if (autoSend && dest) {
                    self->fInput->setText(QString());
                    self->sendPrivmsg(s, dest, target, translated);
                } else {
                    self->fPendingReviewedTranslation = translated;
                    self->fInput->setText(translated);
                }
            });
        return;
    }
    sendPrivmsg(s, b, b->name, text);
}

void MainWindow::sendPrivmsg(Session* s, Buffer* b, const QString& target, const QString& text, bool echo)
{
    // Keep each protocol line under the 512-byte limit, splitting on spaces.
    const int maxBytes = 400;
    QString rest = text;
    while (!rest.isEmpty()) {
        QString chunk = rest;
        while (chunk.toUtf8().size() > maxBytes) {
            int cut = chunk.lastIndexOf(' ', chunk.size() - 2);
            chunk = cut > maxBytes / 4 ? chunk.left(cut) : chunk.left(chunk.size() * maxBytes / chunk.toUtf8().size());
        }
        rest = rest.mid(chunk.size()).trimmed();
        s->conn->sendRaw(QString("PRIVMSG %1 :%2").arg(target, chunk));
        if (echo && b)
            printMessage(b, s->conn->nick(), chunk, LineKind::Normal, true);
    }
}

void MainWindow::sendAction(Session* s, Buffer* b, const QString& target, const QString& text)
{
    s->conn->sendRaw(QString("PRIVMSG %1 :\x01" "ACTION %2\x01").arg(target, text));
    if (b)
        printMessage(b, s->conn->nick(), text, LineKind::Action, true);
}

void MainWindow::processCommand(Session* s, Buffer* b, const QString& input)
{
    QString line = input.startsWith('/') ? input.mid(1) : input;
    int sp = line.indexOf(' ');
    QString cmd = (sp < 0 ? line : line.left(sp)).toLower();
    QString args = sp < 0 ? QString() : line.mid(sp + 1).trimmed();
    QStringList argv = args.split(' ', Qt::SkipEmptyParts);
    IrcConnection* c = s->conn;
    const bool inChannel = b && b->type == Buffer::Channel;
    const QString target = (b && b->type != Buffer::Server) ? b->name : QString();
    auto error = [&](const QString& text) { print(b ? b : s->server, text.toHtmlEscaped(), LineKind::Error); };
    auto need = [&](bool connected) {
        if (!connected) {
            error(tr("Not connected to %1.").arg(s->profile().name));
            return false;
        }
        return true;
    };

    if (cmd == "clear") {
        if (b) { b->lines.clear(); if (b == fActive) fChat->clear(); }
        return;
    }
    if (cmd == "connect" || cmd == "server") { connectSession(s); return; }
    if (cmd == "reconnect") { disconnectSession(s); QTimer::singleShot(1500, this, [this, s]() { if (fSessions.contains(s)) connectSession(s); }); return; }
    if (cmd == "disconnect") { disconnectSession(s); return; }
    if (cmd == "quit") {
        c->disconnectFromServer(args.isEmpty() ? cfg.fullQuitMessage() : args);
        return;
    }
    if (cmd == "close") {
        if (b && b->type != Buffer::Server) {
            if (b->type == Buffer::Channel && b->joined)
                c->sendRaw("PART " + b->name);
            removeBuffer(b);
        }
        return;
    }
    if (cmd == "help") {
        for (QAction* a : menuBar()->actions())
            if (a->menu() && a->text().contains("Help"))
                a->menu()->actions().first()->trigger();
        return;
    }
    if (cmd == "ignore" || cmd == "unignore") {
        ServerConfig& p = s->profile();
        if (argv.isEmpty()) {
            print(b ? b : s->server, tr("Ignored: %1").arg(p.ignoredNicks.isEmpty() ? tr("(none)") : p.ignoredNicks.join(", ")).toHtmlEscaped());
            return;
        }
        if (cmd == "ignore" && !p.ignoredNicks.contains(argv[0], Qt::CaseInsensitive))
            p.ignoredNicks << argv[0];
        if (cmd == "unignore")
            p.ignoredNicks.removeAll(argv[0]);
        saveConfig();
        print(b ? b : s->server, tr("%1 %2").arg(cmd == "ignore" ? tr("Now ignoring") : tr("No longer ignoring"), argv[0]).toHtmlEscaped());
        return;
    }
    if (cmd == "query") {
        if (argv.isEmpty()) { error(tr("Usage: /query nick [message]")); return; }
        Buffer* q = ensureBuffer(s, argv[0], Buffer::Query, true);
        if (argv.size() > 1 && need(c->isRegistered()))
            sendPrivmsg(s, q, argv[0], args.mid(args.indexOf(' ') + 1));
        return;
    }
    if (cmd == "list") {
        if (need(c->isRegistered()))
            openChannelList(s);
        return;
    }

    if (!need(c->isConnected()))
        return;

    if (cmd == "me") {
        if (target.isEmpty()) { error(tr("You can only use /me in a channel or private chat.")); return; }
        sendAction(s, b, target, args);
    } else if (cmd == "msg" || cmd == "privmsg") {
        if (argv.size() < 2) { error(tr("Usage: /msg target message")); return; }
        QString body = args.mid(args.indexOf(' ') + 1);
        Buffer* dest = findBuffer(s, argv[0]);
        sendPrivmsg(s, dest, argv[0], body, dest != nullptr);
        if (!dest)
            print(b ? b : s->server, QString("-&gt; *%1* %2").arg(argv[0].toHtmlEscaped(), formatBody(s, body)), LineKind::Own);
    } else if (cmd == "notice") {
        if (argv.size() < 2) { error(tr("Usage: /notice target message")); return; }
        QString body = args.mid(args.indexOf(' ') + 1);
        c->sendRaw(QString("NOTICE %1 :%2").arg(argv[0], body));
        print(b ? b : s->server, QString("-&gt; -%1- %2").arg(argv[0].toHtmlEscaped(), formatBody(s, body)), LineKind::Notice);
    } else if (cmd == "ctcp") {
        if (argv.size() < 2) { error(tr("Usage: /ctcp nick COMMAND")); return; }
        QString rest = args.mid(args.indexOf(' ') + 1);
        if (rest.compare("ping", Qt::CaseInsensitive) == 0)
            rest = "PING " + QString::number(QDateTime::currentMSecsSinceEpoch());
        c->sendRaw(QString("PRIVMSG %1 :\x01%2\x01").arg(argv[0], rest.toUpper().startsWith("PING") ? rest : rest.toUpper()));
        print(b ? b : s->server, tr("[CTCP] Sent %1 to %2").arg(rest, argv[0]).toHtmlEscaped());
    } else if (cmd == "join" || cmd == "j") {
        if (argv.isEmpty()) { joinChannelPrompt(s); return; }
        QString chan = argv[0];
        if (!isChannelName(chan))
            chan.prepend('#');
        s->pendingFocus = chan.split(',').first();
        c->sendRaw("JOIN " + chan + (argv.size() > 1 ? " " + argv[1] : QString()));
    } else if (cmd == "part" || cmd == "leave") {
        QString chan = inChannel ? target : QString();
        QString reason = args;
        if (!argv.isEmpty() && isChannelName(argv[0])) {
            chan = argv[0];
            reason = args.mid(argv[0].size()).trimmed();
        }
        if (chan.isEmpty()) { error(tr("Usage: /part #channel [reason]")); return; }
        c->sendRaw(QString("PART %1 :%2").arg(chan, reason.isEmpty() ? QString("Leaving") : reason));
    } else if (cmd == "cycle" || cmd == "rejoin") {
        if (!inChannel) return;
        c->sendRaw("PART " + target);
        c->sendRaw("JOIN " + target + (b->key.isEmpty() ? QString() : " " + b->key));
    } else if (cmd == "topic") {
        if (!inChannel) { error(tr("You can only set a topic inside a channel.")); return; }
        if (args.isEmpty())
            c->sendRaw("TOPIC " + target);
        else
            c->sendRaw(QString("TOPIC %1 :%2").arg(target, args));
    } else if (cmd == "nick") {
        if (argv.isEmpty()) { error(tr("Usage: /nick newnick")); return; }
        c->sendRaw("NICK " + argv[0]);
    } else if (cmd == "away") {
        c->sendRaw("AWAY :" + (args.isEmpty() ? cfg.awayMessage : args));
    } else if (cmd == "back") {
        c->sendRaw("AWAY");
    } else if (cmd == "whois" || cmd == "wi") {
        if (argv.isEmpty() && b && b->type == Buffer::Query) argv << b->name;
        if (argv.isEmpty()) { error(tr("Usage: /whois nick")); return; }
        c->sendRaw("WHOIS " + argv[0] + " " + argv[0]);
    } else if (cmd == "op" || cmd == "deop" || cmd == "voice" || cmd == "devoice") {
        if (!inChannel || argv.isEmpty()) { error(tr("Usage: /%1 nick [nick...] (inside a channel)").arg(cmd)); return; }
        QString flag = cmd == "op" ? "+o" : cmd == "deop" ? "-o" : cmd == "voice" ? "+v" : "-v";
        for (const QString& n : argv)
            c->sendRaw(QString("MODE %1 %2 %3").arg(target, flag, n));
    } else if (cmd == "kick" || cmd == "k") {
        if (!inChannel || argv.isEmpty()) { error(tr("Usage: /kick nick [reason] (inside a channel)")); return; }
        QString reason = args.mid(argv[0].size()).trimmed();
        c->sendRaw(QString("KICK %1 %2 :%3").arg(target, argv[0], reason.isEmpty() ? QString("Bye") : reason));
    } else if (cmd == "ban" || cmd == "unban") {
        if (!inChannel || argv.isEmpty()) { error(tr("Usage: /%1 nick|mask (inside a channel)").arg(cmd)); return; }
        QString mask = argv[0].contains('!') || argv[0].contains('@') ? argv[0] : argv[0] + "!*@*";
        c->sendRaw(QString("MODE %1 %2b %3").arg(target, cmd == "ban" ? "+" : "-", mask));
    } else if (cmd == "banlist" || cmd == "bans") {
        if (inChannel) c->sendRaw(QString("MODE %1 +b").arg(target));
    } else if (cmd == "invite") {
        if (argv.isEmpty()) { error(tr("Usage: /invite nick [#channel]")); return; }
        c->sendRaw(QString("INVITE %1 %2").arg(argv[0], argv.value(1, target)));
    } else if (cmd == "mode") {
        if (argv.isEmpty() && inChannel) c->sendRaw("MODE " + target);
        else if (!argv.isEmpty() && !isChannelName(argv[0]) && argv[0].compare(c->nick(), Qt::CaseInsensitive) != 0 && inChannel)
            c->sendRaw(QString("MODE %1 %2").arg(target, args));
        else c->sendRaw("MODE " + args);
    } else if (cmd == "names") {
        QString chan = argv.value(0, target);
        if (!chan.isEmpty()) c->sendRaw("NAMES " + chan);
    } else if (cmd == "knock") {
        if (argv.isEmpty()) { error(tr("Usage: /knock #channel [message]")); return; }
        c->sendRaw("KNOCK " + args);
    } else if (cmd == "quote" || cmd == "raw") {
        c->sendRaw(args);
    } else if (cmd == "say") {
        if (!target.isEmpty()) sendPrivmsg(s, b, target, args);
    } else {
        // Unknown commands go to the server verbatim, as in the Haiku build.
        c->sendRaw(line);
    }
}

// ---------------------------------------------------------------------------
// IRC events
// ---------------------------------------------------------------------------

bool MainWindow::isIgnored(Session* s, const QString& nick) const
{
    for (const QString& mask : s->profile().ignoredNicks)
        if (matchWildcard(nick, mask))
            return true;
    return false;
}

void MainWindow::onRegistered(Session* s)
{
    ServerConfig& p = s->profile();
    // Remember the nick the server actually gave us.
    updateTreeItem(s->server);
    refreshStatus();

    // Entries may carry a key ("#chan key"); de-duplicate on the channel name.
    QStringList channels = s->rejoinChannels;
    auto listed = [&channels](const QString& entry) {
        const QString name = entry.section(' ', 0, 0);
        for (const QString& c : channels)
            if (c.section(' ', 0, 0).compare(name, Qt::CaseInsensitive) == 0)
                return true;
        return false;
    };
    for (const QString& chan : p.autojoin) {
        const QString c = chan.trimmed();
        if (!c.isEmpty() && !listed(c))
            channels << c;
    }
    s->rejoinChannels.clear();
    for (const QString& chan : channels)
        s->conn->sendRaw("JOIN " + chan);
    for (const QString& cmd : p.autocmdlist) {
        if (!cmd.trimmed().isEmpty())
            processCommand(s, s->server, cmd.trimmed());
    }
    if (s->away)
        s->conn->sendRaw("AWAY :" + cfg.awayMessage);
}

void MainWindow::onDisconnected(Session* s)
{
    s->identifiedWithServices = false;
    for (Buffer* b : s->buffers) {
        if (b->type == Buffer::Channel && b->joined) {
            s->rejoinChannels << (b->key.isEmpty() ? b->name : b->name + " " + b->key);
            b->joined = false;
            b->users.clear();
            print(b, tr("--- Disconnected from %1").arg(s->profile().name).toHtmlEscaped(), LineKind::Part);
        }
        updateTreeItem(b);
    }
    updateTreeItem(s->server);
    if (fActive && fActive->session == s)
        refreshUserList();
    refreshStatus();
}

void MainWindow::onMessage(Session* s, const IrcMessage& m)
{
    const QString& cmd = m.command;
    IrcConnection* c = s->conn;
    const QString me = c->nick();
    const QString nick = m.nick();
    const bool fromMe = nick.compare(me, Qt::CaseInsensitive) == 0;
    const ServerConfig& p = s->profile();

    if (cmd == "PRIVMSG" || cmd == "NOTICE") {
        if (!nick.isEmpty() && m.prefix.contains('!') && isIgnored(s, nick))
            return;
        QString body = m.last();
        if (cmd == "PRIVMSG" && p.enableInboundTranslation && !p.geminiApiKey.isEmpty()
            && !body.startsWith('\x01') && !fromMe) {
            QPointer<MainWindow> self(this);
            IrcMessage copy = m;
            fTranslator->translate(IrcFormat::stripCodes(body), p.targetLanguage, p.geminiModel, p.geminiApiKey,
                [self, s, copy](bool ok, const QString& translated) {
                    if (!self || !self->fSessions.contains(s))
                        return;
                    self->handlePrivmsg(s, copy, ok ? translated : copy.last());
                });
            return;
        }
        handlePrivmsg(s, m, body);
        return;
    }

    if (cmd == "JOIN") {
        const QString chan = m.param(0);
        if (fromMe) {
            bool focus = s->pendingFocus.compare(chan, Qt::CaseInsensitive) == 0
                || (fActive && fActive->session == s && fActive->type == Buffer::Server && s->buffers.isEmpty());
            if (focus)
                s->pendingFocus.clear();
            Buffer* b = ensureBuffer(s, chan, Buffer::Channel, focus);
            b->joined = true;
            b->users.clear();
            print(b, tr("--- Joined channel %1").arg(chan).toHtmlEscaped(), LineKind::Join);
            updateTreeItem(b);
            c->sendRaw("MODE " + chan);
            if (c->hasCap("away-notify"))
                c->sendRaw("WHO " + chan);
            return;
        }
        if (isIgnored(s, nick))
            return;
        Buffer* b = findBuffer(s, chan);
        if (!b)
            return;
        ChannelUser u;
        u.nick = nick;
        b->users.insert(nick.toLower(), u);
        if (!p.hideStatusMessages)
            print(b, QString("--&gt; %1 (%2) has joined %3").arg(nick.toHtmlEscaped(), m.userHost().toHtmlEscaped(), chan.toHtmlEscaped()), LineKind::Join);
        if (b == fActive)
            refreshUserList();
        return;
    }

    if (cmd == "PART") {
        const QString chan = m.param(0);
        Buffer* b = findBuffer(s, chan);
        if (!b)
            return;
        QString reason = m.params.size() > 1 ? m.last() : QString();
        if (fromMe) {
            b->joined = false;
            b->users.clear();
            print(b, tr("--- You have left %1").arg(chan).toHtmlEscaped(), LineKind::Part);
            updateTreeItem(b);
        } else {
            b->users.remove(nick.toLower());
            if (!p.hideStatusMessages && !isIgnored(s, nick))
                print(b, QString("&lt;-- %1 has left %2%3").arg(nick.toHtmlEscaped(), chan.toHtmlEscaped(),
                    reason.isEmpty() ? QString() : " (" + formatBody(s, reason) + ")"), LineKind::Part);
        }
        if (b == fActive)
            refreshUserList();
        return;
    }

    if (cmd == "QUIT") {
        const QString key = nick.toLower();
        const QString reason = m.last();
        for (Buffer* b : s->buffers) {
            bool present = b->users.remove(key) > 0;
            if (b->type == Buffer::Query && b->name.compare(nick, Qt::CaseInsensitive) == 0)
                present = true;
            if (present && !p.hideStatusMessages && !isIgnored(s, nick))
                print(b, QString("&lt;-- %1 has quit (%2)").arg(nick.toHtmlEscaped(), formatBody(s, reason)), LineKind::Part);
            if (present && b == fActive)
                refreshUserList();
        }
        return;
    }

    if (cmd == "NICK") {
        const QString newNick = m.param(0);
        const QString key = nick.toLower();
        for (Buffer* b : s->buffers) {
            bool present = false;
            if (b->users.contains(key)) {
                ChannelUser u = b->users.take(key);
                u.nick = newNick;
                b->users.insert(newNick.toLower(), u);
                present = true;
            }
            if (b->type == Buffer::Query && b->name.compare(nick, Qt::CaseInsensitive) == 0) {
                b->name = newNick;
                updateTreeItem(b);
                present = true;
            }
            if (present && (fromMe || !p.hideStatusMessages))
                print(b, fromMe ? tr("--- You are now known as %1").arg(newNick).toHtmlEscaped()
                                : QString("--- %1 is now known as %2").arg(nick.toHtmlEscaped(), newNick.toHtmlEscaped()));
            if (present && b == fActive)
                refreshUserList();
        }
        if (fromMe) {
            printStatus(s, tr("--- You are now known as %1").arg(newNick));
            refreshStatus();
        }
        return;
    }

    if (cmd == "KICK") {
        const QString chan = m.param(0);
        const QString victim = m.param(1);
        Buffer* b = findBuffer(s, chan);
        if (!b)
            return;
        b->users.remove(victim.toLower());
        bool meKicked = victim.compare(me, Qt::CaseInsensitive) == 0;
        print(b, QString("&lt;-- %1 was kicked from %2 by %3 (%4)").arg(
            meKicked ? tr("You") : victim.toHtmlEscaped(), chan.toHtmlEscaped(), nick.toHtmlEscaped(), formatBody(s, m.param(2))),
            meKicked ? LineKind::Error : LineKind::Part);
        if (meKicked) {
            b->joined = false;
            b->users.clear();
            updateTreeItem(b);
            Notifier::notify(tr("Kicked from %1").arg(chan), tr("%1: %2").arg(nick, m.param(2)));
        }
        if (b == fActive)
            refreshUserList();
        return;
    }

    if (cmd == "MODE") {
        handleMode(s, m);
        return;
    }

    if (cmd == "TOPIC") {
        Buffer* b = findBuffer(s, m.param(0));
        if (!b)
            return;
        b->topic = m.last();
        print(b, QString("--- %1 changed the topic to: %2").arg(nick.toHtmlEscaped(), formatBody(s, b->topic)));
        if (b == fActive)
            refreshTopic();
        return;
    }

    if (cmd == "AWAY") {
        bool away = !m.params.isEmpty();
        for (Buffer* b : s->buffers) {
            auto it = b->users.find(nick.toLower());
            if (it != b->users.end()) {
                it->away = away;
                if (b == fActive)
                    refreshUserList();
            }
        }
        return;
    }

    if (cmd == "INVITE") {
        const QString chan = m.param(1);
        printActiveOrServer(s, tr("--- %1 invites you to %2").arg(nick, chan), LineKind::Notice);
        auto answer = QMessageBox::question(this, tr("Invitation"),
            tr("%1 has invited you to %2 on %3.\n\nJoin now?").arg(nick, chan, p.name));
        if (answer == QMessageBox::Yes) {
            s->pendingFocus = chan;
            c->sendRaw("JOIN " + chan);
        }
        return;
    }

    if (cmd == "ERROR") {
        printStatus(s, "--- " + m.last(), LineKind::Error);
        return;
    }

    if (cmd == "PONG" || cmd == "CHGHOST" || cmd == "ACCOUNT" || cmd == "CAP")
        return;

    if (!m.isNumeric()) {
        printStatus(s, QString("%1 %2").arg(cmd, m.params.join(' ')));
        return;
    }

    // ---- numerics ----
    const int num = cmd.toInt();
    const QStringList rest = m.params.mid(1);

    switch (num) {
    case 1:
        printStatus(s, "--> " + m.last());
        return;
    case 5: {
        QStringList tokens = rest;
        if (m.hasTrailing && !tokens.isEmpty())
            tokens.removeLast();
        printStatus(s, QString("--> Server Features: %1").arg(tokens.join(' ')));
        return;
    }
    case 42:
        printStatus(s, QString("--> Session Assigned: Unique ID '%1' (%2)").arg(m.param(1), m.last()));
        return;
    case 301: { // RPL_AWAY
        Buffer* q = findBuffer(s, m.param(1));
        print(q ? q : (fActive && fActive->session == s ? fActive : s->server),
            QString("--- %1 is away: %2").arg(m.param(1).toHtmlEscaped(), formatBody(s, m.last())));
        return;
    }
    case 305:
        s->away = false;
        printActiveOrServer(s, "--- " + m.last());
        refreshStatus();
        return;
    case 306:
        s->away = true;
        printActiveOrServer(s, "--- " + m.last());
        refreshStatus();
        return;
    case 311: case 312: case 313: case 317: case 318: case 319: case 330: case 338: case 378: case 379:
    case 276: case 671: case 320: case 307: case 314: case 369:
        handleWhois(s, m);
        return;
    case 315: // end of WHO
        return;
    case 321:
        return;
    case 322:
        if (s->listDialog)
            s->listDialog->addChannel(m.param(1), m.param(2).toInt(), IrcFormat::stripCodes(m.last()));
        return;
    case 323:
        if (s->listDialog)
            s->listDialog->finished();
        return;
    case 324: { // RPL_CHANNELMODEIS
        Buffer* b = findBuffer(s, m.param(1));
        if (!b)
            return;
        QString flags = m.param(2);
        QStringList args = m.params.mid(3);
        b->modes.clear();
        b->key.clear();
        b->limit.clear();
        int ai = 0;
        for (QChar ch : flags) {
            if (ch == '+' || ch == '-')
                continue;
            b->modes += ch;
            if (ch == 'k') b->key = args.value(ai++);
            else if (ch == 'l') b->limit = args.value(ai++);
            else if (c->chanModesB().contains(ch) || c->chanModesC().contains(ch)) ++ai;
        }
        print(b, QString("--- Channel modes: +%1").arg((b->modes + (args.isEmpty() ? QString() : " " + args.join(' '))).toHtmlEscaped()));
        if (fModesDialog && fModesDialog->channel().compare(b->name, Qt::CaseInsensitive) == 0)
            fModesDialog->setModes(b->modes, b->key, b->limit);
        return;
    }
    case 329: { // creation time
        Buffer* b = findBuffer(s, m.param(1));
        if (b)
            print(b, tr("--- Channel created %1").arg(QDateTime::fromSecsSinceEpoch(m.param(2).toLongLong()).toString()).toHtmlEscaped());
        return;
    }
    case 331: {
        Buffer* b = findBuffer(s, m.param(1));
        if (b) {
            b->topic.clear();
            print(b, tr("--- No topic is set").toHtmlEscaped());
            if (b == fActive) refreshTopic();
        }
        return;
    }
    case 332: {
        Buffer* b = findBuffer(s, m.param(1));
        if (b) {
            b->topic = m.last();
            print(b, QString("--- Topic: %1").arg(formatBody(s, b->topic)));
            if (b == fActive) refreshTopic();
        }
        return;
    }
    case 333: {
        Buffer* b = findBuffer(s, m.param(1));
        if (b) {
            QString who = m.param(2).section('!', 0, 0);
            print(b, tr("--- Topic set by %1 on %2").arg(who,
                QDateTime::fromSecsSinceEpoch(m.param(3).toLongLong()).toString()).toHtmlEscaped());
        }
        return;
    }
    case 341: // RPL_INVITING
        printActiveOrServer(s, tr("--- Invited %1 to %2").arg(m.param(1), m.param(2)));
        return;
    case 352: { // RPL_WHOREPLY: me chan user host server nick flags :hops realname
        Buffer* b = findBuffer(s, m.param(1));
        if (!b)
            return;
        auto it = b->users.find(m.param(5).toLower());
        if (it != b->users.end()) {
            it->away = m.param(6).startsWith('G');
            if (b == fActive)
                refreshUserList();
        }
        return;
    }
    case 353:
        handleNames(s, m);
        return;
    case 366: {
        Buffer* b = findBuffer(s, m.param(1));
        if (b && b->namesInProgress) {
            // Keep known away flags across a NAMES refresh.
            for (auto it = b->pendingNames.begin(); it != b->pendingNames.end(); ++it)
                if (b->users.contains(it.key()))
                    it->away = b->users.value(it.key()).away;
            b->users = b->pendingNames;
            b->pendingNames.clear();
            b->namesInProgress = false;
            if (b == fActive)
                refreshUserList();
        }
        return;
    }
    case 367: { // ban list entry: me chan mask [setter time]
        Buffer* b = findBuffer(s, m.param(1));
        QString text = tr("--- Ban: %1").arg(m.param(2));
        if (!m.param(3).isEmpty())
            text += tr(" (set by %1 on %2)").arg(m.param(3).section('!', 0, 0),
                QDateTime::fromSecsSinceEpoch(m.param(4).toLongLong()).toString());
        print(b ? b : s->server, text.toHtmlEscaped());
        return;
    }
    case 368: {
        Buffer* b = findBuffer(s, m.param(1));
        print(b ? b : s->server, tr("--- End of ban list").toHtmlEscaped());
        return;
    }
    case 372: case 375: case 376: case 422:
        printStatus(s, m.last());
        return;
    case 396:
        printStatus(s, QString("--> Network Security: '%1' %2").arg(m.param(1), m.last()));
        return;
    case 432: case 433: case 436: case 437:
        printActiveOrServer(s, QString("--- %1: %2").arg(m.param(1), m.last()), LineKind::Error);
        return;
    case 473: { // invite only
        const QString chan = m.param(1);
        printActiveOrServer(s, QString("--- %1: %2").arg(chan, m.last()), LineKind::Error);
        auto answer = QMessageBox::question(this, tr("Invite Only"),
            tr("%1 is invite-only.\n\nSend a KNOCK to ask the channel operators for an invite?").arg(chan));
        if (answer == QMessageBox::Yes)
            c->sendRaw(QString("KNOCK %1 :Requesting an invite").arg(chan));
        return;
    }
    case 475: { // bad key
        const QString chan = m.param(1);
        printActiveOrServer(s, QString("--- %1: %2").arg(chan, m.last()), LineKind::Error);
        bool ok = false;
        QString key = QInputDialog::getText(this, tr("Channel Key"),
            tr("%1 requires a key (password):").arg(chan), QLineEdit::Password, QString(), &ok);
        if (ok && !key.isEmpty()) {
            s->pendingFocus = chan;
            c->sendRaw(QString("JOIN %1 %2").arg(chan, key));
        }
        return;
    }
    case 710: { // RPL_KNOCK: me chan knocker!mask :msg
        const QString chan = m.param(1);
        const QString knocker = m.param(2).section('!', 0, 0);
        Buffer* b = findBuffer(s, chan);
        print(b ? b : s->server, QString("--&gt; [KNOCK] %1 %2").arg(knocker.toHtmlEscaped(), formatBody(s, m.last())), LineKind::Notice);
        auto answer = QMessageBox::question(this, tr("Incoming Knock"),
            tr("User %1 is knocking on %2.\n\nDo you want to invite them into the channel?").arg(knocker, chan));
        if (answer == QMessageBox::Yes)
            c->sendRaw(QString("INVITE %1 %2").arg(knocker, chan));
        return;
    }
    default:
        break;
    }

    if (num >= 400 && num < 600) {
        QStringList parts = rest;
        if (m.hasTrailing && !parts.isEmpty())
            parts.removeLast();
        QString subject = parts.join(' ');
        printActiveOrServer(s, QString("--- %1%2").arg(subject.isEmpty() ? QString() : subject + ": ", m.last()), LineKind::Error);
        return;
    }

    // Anything else: show the human-readable part in the server log.
    QStringList parts = rest;
    printStatus(s, parts.join(' '));
}

void MainWindow::handleNames(Session* s, const IrcMessage& m)
{
    // me = #chan :names  (param(1) is the channel type symbol)
    Buffer* b = findBuffer(s, m.param(2));
    if (!b) {
        printStatus(s, QString("%1: %2").arg(m.param(2), m.last()));
        return;
    }
    if (!b->namesInProgress) {
        b->namesInProgress = true;
        b->pendingNames.clear();
    }
    const QString symbols = s->conn->prefixSymbols();
    for (QString entry : m.last().split(' ', Qt::SkipEmptyParts)) {
        ChannelUser u;
        int i = 0;
        while (i < entry.size() && symbols.contains(entry.at(i)))
            u.prefixes += entry.at(i++);
        QString n = entry.mid(i);
        int bang = n.indexOf('!');
        if (bang >= 0)
            n.truncate(bang);
        u.nick = n;
        if (!n.isEmpty())
            b->pendingNames.insert(n.toLower(), u);
    }
}

void MainWindow::handleMode(Session* s, const IrcMessage& m)
{
    IrcConnection* c = s->conn;
    const QString target = m.param(0);
    const QString setter = m.nick();
    const QString modeText = m.params.mid(1).join(' ');

    if (!isChannelName(target)) {
        printStatus(s, QString("--- Mode %1 [%2] by %3").arg(target, modeText, setter));
        return;
    }
    Buffer* b = findBuffer(s, target);
    if (!b)
        return;

    const QString flags = m.param(1);
    QStringList args = m.params.mid(2);
    const QString prefixModes = c->prefixModes();
    const QString prefixSymbols = c->prefixSymbols();
    bool adding = true;
    int ai = 0;
    bool usersChanged = false;
    for (QChar ch : flags) {
        if (ch == '+') { adding = true; continue; }
        if (ch == '-') { adding = false; continue; }
        int pi = prefixModes.indexOf(ch);
        if (pi >= 0) {
            QString who = args.value(ai++);
            auto it = b->users.find(who.toLower());
            if (it != b->users.end()) {
                QChar sym = pi < prefixSymbols.size() ? prefixSymbols.at(pi) : QChar('@');
                if (adding && !it->prefixes.contains(sym)) {
                    it->prefixes += sym;
                    std::sort(it->prefixes.begin(), it->prefixes.end(), [&](QChar a, QChar b2) {
                        return prefixSymbols.indexOf(a) < prefixSymbols.indexOf(b2);
                    });
                } else if (!adding) {
                    it->prefixes.remove(sym);
                }
                usersChanged = true;
            }
            continue;
        }
        if (c->chanModesA().contains(ch)) { ++ai; continue; }
        if (ch == 'k') {
            QString k = args.value(ai++);
            b->key = adding ? k : QString();
        } else if (ch == 'l') {
            b->limit = adding ? args.value(ai++) : QString();
        } else if (c->chanModesB().contains(ch)) {
            ++ai;
        } else if (c->chanModesC().contains(ch)) {
            if (adding) ++ai;
        }
        if (adding && !b->modes.contains(ch)) b->modes += ch;
        if (!adding) b->modes.remove(ch);
    }
    print(b, QString("--- %1 sets mode %2").arg(setter.toHtmlEscaped(), modeText.toHtmlEscaped()));
    if (usersChanged && b == fActive)
        refreshUserList();
    if (fModesDialog && fModesDialog->channel().compare(b->name, Qt::CaseInsensitive) == 0)
        fModesDialog->setModes(b->modes, b->key, b->limit);
}

void MainWindow::handleWhois(Session* s, const IrcMessage& m)
{
    const QString who = m.param(1);
    QString text;
    switch (m.command.toInt()) {
    case 311: text = QString("%1 is %2@%3 (%4)").arg(who, m.param(2), m.param(3), m.last()); break;
    case 314: text = QString("%1 was %2@%3 (%4)").arg(who, m.param(2), m.param(3), m.last()); break;
    case 312: text = QString("%1 is connected to %2 (%3)").arg(who, m.param(2), m.last()); break;
    case 313: text = QString("%1 %2").arg(who, m.last()); break;
    case 317: {
        qint64 idle = m.param(2).toLongLong();
        text = QString("%1 has been idle %2h %3m %4s, signed on %5").arg(who).arg(idle / 3600).arg((idle / 60) % 60).arg(idle % 60)
            .arg(QDateTime::fromSecsSinceEpoch(m.param(3).toLongLong()).toString());
        break;
    }
    case 318: case 369: text = QString("End of WHOIS for %1").arg(who); break;
    case 319: text = QString("%1 is on %2").arg(who, m.last()); break;
    case 330: text = QString("%1 is logged in as %2").arg(who, m.param(2)); break;
    default: text = QString("%1 %2").arg(who, m.last()); break;
    }
    Buffer* q = findBuffer(s, who);
    Buffer* b = q ? q : (fActive && fActive->session == s ? fActive : s->server);
    print(b, QString("[whois] %1").arg(text).toHtmlEscaped(), LineKind::Notice);
}

void MainWindow::handlePrivmsg(Session* s, const IrcMessage& m, const QString& body)
{
    IrcConnection* c = s->conn;
    const QString me = c->nick();
    const QString nick = m.nick();
    const QString target = m.param(0);
    const bool isNotice = m.command == "NOTICE";
    const bool fromServer = !m.prefix.contains('!');
    const bool fromMe = nick.compare(me, Qt::CaseInsensitive) == 0;
    ServerConfig& p = s->profile();

    // --- CTCP ---
    if (body.startsWith('\x01')) {
        QString inner = body.mid(1);
        if (inner.endsWith('\x01'))
            inner.chop(1);
        QString ctcp = inner.section(' ', 0, 0).toUpper();
        QString ctcpArgs = inner.section(' ', 1);
        if (ctcp == "ACTION") {
            Buffer* b = isChannelName(target) ? findBuffer(s, target) : ensureBuffer(s, fromMe ? target : nick, Buffer::Query);
            if (!b)
                return;
            bool hl = !fromMe && mentionsNick(ctcpArgs, me);
            printMessage(b, nick, ctcpArgs, hl ? LineKind::Highlight : LineKind::Action, fromMe);
            if ((hl || b->type == Buffer::Query) && !fromMe && p.nickAlert && (!isActiveWindow() || fActive != b))
                Notifier::notify(QString("[%1] %2").arg(p.name, b->name), QString("* %1 %2").arg(nick, IrcFormat::stripCodes(ctcpArgs)));
            return;
        }
        if (isNotice) {
            QString shown = inner;
            if (ctcp == "PING") {
                qint64 sent = ctcpArgs.toLongLong();
                qint64 lag = QDateTime::currentMSecsSinceEpoch() - sent;
                if (sent > 0 && lag >= 0)
                    shown = QString("PING reply: %1 ms").arg(lag);
            }
            printActiveOrServer(s, QString("--- [CTCP] %1 reply from %2: %3").arg(ctcp, nick, shown), LineKind::Notice);
            return;
        }
        QString reply;
        if (ctcp == "VERSION")
            reply = QString("VERSION %1").arg(AppInfo::VERSION_STRING);
        else if (ctcp == "PING")
            reply = "PING " + ctcpArgs;
        else if (ctcp == "TIME")
            reply = "TIME " + QDateTime::currentDateTime().toString(Qt::RFC2822Date);
        else if (ctcp == "CLIENTINFO")
            reply = "CLIENTINFO ACTION CLIENTINFO PING TIME VERSION";
        if (!reply.isEmpty())
            c->sendRaw(QString("NOTICE %1 :\x01%2\x01").arg(nick, reply));
        printStatus(s, QString("--- [CTCP] %1 query from %2%3").arg(ctcp, nick, reply.isEmpty() ? QString() : " answered"));
        return;
    }

    // --- NickServ identification fallback ---
    if (isNotice && nick.compare("NickServ", Qt::CaseInsensitive) == 0 && !s->identifiedWithServices) {
        const QString lower = body.toLower();
        bool challenge = lower.contains("identify") || lower.contains("registered and protected")
            || lower.contains("access list") || lower.contains("authenticate");
        bool loggedInViaSasl = c->hasCap("sasl") && p.useSASL;
        if (challenge && !loggedInViaSasl) {
            if (p.useCertFP) {
                s->identifiedWithServices = true;
                printStatus(s, "--- [Authentication] Certificate authentication (CertFP) is active.");
            } else if (!p.pass.isEmpty()) {
                s->identifiedWithServices = true;
                c->sendRaw("PRIVMSG NickServ :IDENTIFY " + p.pass);
                printStatus(s, "--- [Auto-Services] Identification sent to NickServ.");
            }
        }
    }

    // --- Route to a buffer ---
    Buffer* b = nullptr;
    if (isChannelName(target) || (target.size() > 1 && c->prefixSymbols().contains(target.at(0)) && isChannelName(target.mid(1)))) {
        QString chan = isChannelName(target) ? target : target.mid(1); // STATUSMSG like @#chan
        b = findBuffer(s, chan);
    } else if (fromServer || target == "*" || target == "$*") {
        b = s->server;
    } else if (isNotice) {
        // Private notices go to an open query, then the active buffer on this network.
        b = findBuffer(s, nick);
        if (!b)
            b = (fActive && fActive->session == s) ? fActive : s->server;
    } else {
        QString other = fromMe ? target : nick;
        bool isNew = !findBuffer(s, other);
        b = ensureBuffer(s, other, Buffer::Query);
        if (isNew)
            print(b, tr("--- Private conversation started with %1").arg(other).toHtmlEscaped());
    }
    if (!b)
        b = s->server;

    if (fromServer) {
        print(b, QString("-%1- %2").arg(nick.toHtmlEscaped(), formatBody(s, body)), LineKind::Notice);
        return;
    }

    bool highlight = !fromMe && mentionsNick(body, me);
    LineKind kind = isNotice ? LineKind::Notice : (highlight ? LineKind::Highlight : LineKind::Normal);
    printMessage(b, nick, body, kind, fromMe);

    // Anyone who talks is not away.
    if (b->type == Buffer::Channel) {
        auto it = b->users.find(nick.toLower());
        if (it != b->users.end() && it->away && !c->hasCap("away-notify")) {
            it->away = false;
            if (b == fActive) refreshUserList();
        }
    }

    bool privateMsg = !isNotice && b->type == Buffer::Query && !fromMe;
    if ((highlight || privateMsg) && p.nickAlert && (!isActiveWindow() || fActive != b)) {
        Notifier::notify(QString("[%1] %2 — %3").arg(p.name, b->name, nick), IrcFormat::stripCodes(body));
        QApplication::alert(this);
    }
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

void MainWindow::connectSession(Session* s)
{
    if (s->conn->isConnected()) {
        printStatus(s, tr("--- Already connected."));
        return;
    }
    s->conn->setProfile(s->profile());
    s->conn->connectToServer();
    updateTreeItem(s->server);
    refreshStatus();
}

void MainWindow::disconnectSession(Session* s)
{
    s->conn->disconnectFromServer(cfg.fullQuitMessage());
}

void MainWindow::configureSession(Session* s)
{
    ServerDialog dlg(s->profile(), false, this);
    connect(&dlg, &ServerDialog::registerFingerprint, this, [this, s](const QString& sha1, const QString& sha512) {
        registerFingerprint(s, sha1, sha512);
    });
    if (dlg.exec() != QDialog::Accepted)
        return;
    s->profile() = dlg.result();
    s->server->name = s->profile().name;
    s->conn->setProfile(s->profile());
    saveConfig();
    updateTreeItem(s->server);
    if (fActive && fActive->session == s) {
        applyAppearance();
        fChat->setLines(fActive->lines);
        refreshTopic();
    }
    refreshStatus();
}

void MainWindow::registerFingerprint(Session* s, const QString& sha1, const QString& sha512)
{
    if (!s->conn->isRegistered())
        return;
    // OFTC's services use SHA-1 fingerprints; Libera and most Atheme networks use SHA-512.
    QString fp = s->profile().host.contains("oftc", Qt::CaseInsensitive) ? sha1 : sha512;
    s->conn->sendRaw("PRIVMSG NickServ :CERT ADD " + fp);
    printStatus(s, tr("--- [CertFP] Sent CERT ADD %1 to NickServ.").arg(fp));
}

void MainWindow::addServer()
{
    ServerConfig blank;
    blank.name = tr("New Network");
    blank.port = 6697;
    ServerConfig* base = serverProfile(false, 0);
    if (base) {
        blank.nick = base->nick;
        blank.altNick = base->altNick;
        blank.altNick2 = base->altNick2;
    }
    blank.serverListFontSize = cfg.serverListFontSize;
    blank.chatLogFontSize = cfg.chatLogFontSize;
    blank.userListFontSize = cfg.userListFontSize;
    ServerDialog dlg(blank, true, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    cfg.customServers << dlg.result();
    saveConfig();
    Session* s = createSession(true, cfg.customServers.size() - 1);
    setActiveBuffer(s->server);
    if (QMessageBox::question(this, tr("Add Server"), tr("Connect to %1 now?").arg(s->profile().name)) == QMessageBox::Yes)
        connectSession(s);
}

void MainWindow::removeSession(Session* s)
{
    if (!s->custom) {
        QMessageBox::information(this, tr("Remove Server"), tr("The built-in networks cannot be removed."));
        return;
    }
    if (QMessageBox::question(this, tr("Remove Server"), tr("Remove %1 and its settings?").arg(s->profile().name)) != QMessageBox::Yes)
        return;
    if (s->conn->isConnected())
        s->conn->disconnectFromServer(cfg.fullQuitMessage());
    const int removedIndex = s->index;
    cfg.customServers.removeAt(removedIndex);
    for (Session* other : fSessions)
        if (other->custom && other->index > removedIndex)
            --other->index;
    saveConfig();

    fSessions.removeAll(s);
    if (fActive && fActive->session == s)
        fActive = nullptr;
    s->conn->disconnect(this);
    s->conn->deleteLater();
    qDeleteAll(s->buffers);
    delete s->server->item;
    delete s->server;
    delete s;
    if (!fSessions.isEmpty())
        setActiveBuffer(fSessions.first()->server);
}

void MainWindow::openChannelList(Session* s)
{
    if (!s->conn->isRegistered()) {
        printStatus(s, tr("--- Connect first to list channels."), LineKind::Error);
        return;
    }
    if (s->listDialog) {
        s->listDialog->raise();
        s->listDialog->activateWindow();
        return;
    }
    auto* dlg = new ChannelListDialog(s->profile().name, this);
    s->listDialog = dlg;
    connect(dlg, &ChannelListDialog::joinRequested, this, [this, s](const QString& chan) {
        s->pendingFocus = chan;
        s->conn->sendRaw("JOIN " + chan);
    });
    connect(dlg, &ChannelListDialog::refreshRequested, this, [s]() { s->conn->sendRaw("LIST"); });
    dlg->show();
    s->conn->sendRaw("LIST");
}

void MainWindow::joinChannelPrompt(Session* s)
{
    bool ok = false;
    QString chan = QInputDialog::getText(this, tr("Join Channel"), tr("Channel on %1:").arg(s->profile().name),
        QLineEdit::Normal, "#", &ok).trimmed();
    if (!ok || chan.isEmpty() || chan == "#")
        return;
    processCommand(s, fActive && fActive->session == s ? fActive : s->server, "/join " + chan);
}

void MainWindow::toggleAway(Session* s)
{
    if (!s->conn->isRegistered()) {
        refreshStatus();
        return;
    }
    if (s->away)
        s->conn->sendRaw("AWAY");
    else
        s->conn->sendRaw("AWAY :" + cfg.awayMessage);
}

void MainWindow::openModesDialog(Buffer* b)
{
    if (fModesDialog)
        fModesDialog->close();
    auto* dlg = new ChannelModesDialog(b->name, this);
    fModesDialog = dlg;
    dlg->setModes(b->modes, b->key, b->limit);
    Session* s = b->session;
    connect(dlg, &ChannelModesDialog::applyModes, this, [s](const QString& chan, const QString& modes) {
        s->conn->sendRaw(QString("MODE %1 %2").arg(chan, modes));
    });
    dlg->show();
    s->conn->sendRaw("MODE " + b->name);
}

void MainWindow::showTreeMenu(const QPoint& pos)
{
    Buffer* b = bufferForItem(fTree->itemAt(pos));
    if (!b)
        return;
    Session* s = b->session;
    IrcConnection* c = s->conn;
    QMenu menu(this);

    if (b->type == Buffer::Server) {
        if (c->isConnected())
            menu.addAction(QIcon::fromTheme("network-disconnect"), tr("Disconnect"), this, [this, s]() { disconnectSession(s); });
        else
            menu.addAction(QIcon::fromTheme("network-connect"), tr("Connect"), this, [this, s]() { connectSession(s); });
        menu.addAction(tr("Join Channel…"), this, [this, s]() { joinChannelPrompt(s); })->setEnabled(c->isRegistered());
        menu.addAction(tr("Channel List…"), this, [this, s]() { openChannelList(s); })->setEnabled(c->isRegistered());
        QAction* away = menu.addAction(tr("Away"), this, [this, s]() { toggleAway(s); });
        away->setCheckable(true);
        away->setChecked(s->away);
        away->setEnabled(c->isRegistered());
        menu.addSeparator();
        QAction* ac = menu.addAction(tr("Connect on Startup"));
        ac->setCheckable(true);
        ac->setChecked(s->profile().autoConnect);
        connect(ac, &QAction::toggled, this, [s](bool on) { s->profile().autoConnect = on; saveConfig(); });
        QAction* ar = menu.addAction(tr("Auto-Reconnect"));
        ar->setCheckable(true);
        ar->setChecked(s->profile().autoReconnect);
        connect(ar, &QAction::toggled, this, [s](bool on) {
            s->profile().autoReconnect = on;
            s->conn->setProfile(s->profile());
            saveConfig();
        });
        QAction* hs = menu.addAction(tr("Hide Join/Part Messages"));
        hs->setCheckable(true);
        hs->setChecked(s->profile().hideStatusMessages);
        connect(hs, &QAction::toggled, this, [s](bool on) { s->profile().hideStatusMessages = on; saveConfig(); });
        menu.addSeparator();
        menu.addAction(QIcon::fromTheme("configure"), tr("Configure…"), this, [this, s]() { configureSession(s); });
        if (s->custom)
            menu.addAction(QIcon::fromTheme("list-remove"), tr("Remove Server"), this, [this, s]() { removeSession(s); });
    } else {
        if (b->type == Buffer::Channel) {
            if (b->joined) {
                menu.addAction(tr("Leave Channel"), this, [c, b]() { c->sendRaw("PART " + b->name); });
                menu.addAction(tr("Channel Modes…"), this, [this, b]() { openModesDialog(b); });
                menu.addAction(tr("Show Ban List"), this, [c, b]() { c->sendRaw("MODE " + b->name + " +b"); });
            } else {
                menu.addAction(tr("Rejoin Channel"), this, [c, b]() {
                    c->sendRaw("JOIN " + b->name + (b->key.isEmpty() ? QString() : " " + b->key));
                })->setEnabled(c->isRegistered());
            }
            QAction* aj = menu.addAction(tr("Auto-Join This Channel"));
            aj->setCheckable(true);
            QStringList& list = s->profile().autojoin;
            bool isAuto = std::any_of(list.begin(), list.end(), [b](const QString& e) {
                return e.section(' ', 0, 0).compare(b->name, Qt::CaseInsensitive) == 0;
            });
            aj->setChecked(isAuto);
            connect(aj, &QAction::toggled, this, [s, b](bool on) {
                QStringList& l = s->profile().autojoin;
                for (int i = l.size() - 1; i >= 0; --i)
                    if (l.at(i).section(' ', 0, 0).compare(b->name, Qt::CaseInsensitive) == 0)
                        l.removeAt(i);
                if (on)
                    l << (b->key.isEmpty() ? b->name : b->name + " " + b->key);
                saveConfig();
            });
        } else {
            menu.addAction(tr("Whois"), this, [c, b]() { c->sendRaw("WHOIS " + b->name + " " + b->name); })->setEnabled(c->isRegistered());
        }
        menu.addAction(tr("Clear"), this, [this, b]() {
            b->lines.clear();
            if (b == fActive) fChat->clear();
        });
        menu.addSeparator();
        menu.addAction(QIcon::fromTheme("tab-close"), tr("Close"), this, [this, s, b]() {
            processCommand(s, b, "/close");
        });
    }
    menu.exec(fTree->viewport()->mapToGlobal(pos));
}

void MainWindow::showUserMenu(const QPoint& pos)
{
    if (!fActive || fActive->type != Buffer::Channel)
        return;
    QList<QListWidgetItem*> selected = fUsers->selectedItems();
    QListWidgetItem* under = fUsers->itemAt(pos);
    if (under && !selected.contains(under)) {
        fUsers->clearSelection();
        under->setSelected(true);
        selected = {under};
    }
    if (selected.isEmpty())
        return;
    QStringList nicks;
    for (QListWidgetItem* i : selected)
        nicks << i->data(Qt::UserRole).toString();
    const QString first = nicks.first();
    Buffer* b = fActive;
    Session* s = b->session;
    IrcConnection* c = s->conn;
    const QString chan = b->name;

    QMenu menu(this);
    menu.addAction(tr("Private Message"), this, [this, s, first]() { ensureBuffer(s, first, Buffer::Query, true); });
    menu.addAction(tr("Whois"), this, [c, first]() { c->sendRaw("WHOIS " + first + " " + first); });
    menu.addAction(tr("CTCP Version"), this, [this, s, b, first]() { processCommand(s, b, "/ctcp " + first + " VERSION"); });
    menu.addSeparator();
    QMenu* ops = menu.addMenu(tr("Operator"));
    auto modeAll = [c, chan, nicks](const QString& flag) {
        for (const QString& n : nicks)
            c->sendRaw(QString("MODE %1 %2 %3").arg(chan, flag, n));
    };
    ops->addAction(tr("Give Op"), this, [modeAll]() { modeAll("+o"); });
    ops->addAction(tr("Take Op"), this, [modeAll]() { modeAll("-o"); });
    ops->addAction(tr("Give Voice"), this, [modeAll]() { modeAll("+v"); });
    ops->addAction(tr("Take Voice"), this, [modeAll]() { modeAll("-v"); });
    ops->addSeparator();
    ops->addAction(tr("Kick…"), this, [this, c, chan, nicks]() {
        bool ok = false;
        QString reason = QInputDialog::getText(this, tr("Kick"), tr("Reason for kicking %1:").arg(nicks.join(", ")),
            QLineEdit::Normal, "Bye", &ok);
        if (!ok) return;
        for (const QString& n : nicks)
            c->sendRaw(QString("KICK %1 %2 :%3").arg(chan, n, reason));
    });
    ops->addAction(tr("Ban"), this, [c, chan, nicks]() {
        for (const QString& n : nicks)
            c->sendRaw(QString("MODE %1 +b %2!*@*").arg(chan, n));
    });
    ops->addAction(tr("Kick + Ban…"), this, [this, c, chan, nicks]() {
        bool ok = false;
        QString reason = QInputDialog::getText(this, tr("Kick + Ban"), tr("Reason:"), QLineEdit::Normal, "Banned", &ok);
        if (!ok) return;
        for (const QString& n : nicks) {
            c->sendRaw(QString("MODE %1 +b %2!*@*").arg(chan, n));
            c->sendRaw(QString("KICK %1 %2 :%3").arg(chan, n, reason));
        }
    });
    menu.addSeparator();
    menu.addAction(tr("Ignore"), this, [this, s, b, nicks]() {
        for (const QString& n : nicks)
            processCommand(s, b, "/ignore " + n);
    });
    menu.addAction(tr("Set Nick Colour…"), this, [this, s, first]() {
        QColor color = QColorDialog::getColor(IrcFormat::nickColor(first, fChat->isDark()), this, tr("Colour for %1").arg(first));
        if (!color.isValid())
            return;
        ServerConfig& p = s->profile();
        int idx = p.nickColors.indexOf(first);
        if (idx >= 0) {
            p.nickColorValues[idx] = color;
        } else {
            p.nickColors << first;
            p.nickColorValues << color;
        }
        saveConfig();
    });
    menu.exec(fUsers->viewport()->mapToGlobal(pos));
}

void MainWindow::openSearch(const QString& text, bool translate)
{
    QString engine = cfg.searchEngine;
    QString query = translate ? QString("translate %1 in %2").arg(text,
        QLocale::languageToString(QLocale::system().language())) : text;
    QString encoded = QString::fromLatin1(QUrl::toPercentEncoding(query));
    QString url;
    if (engine.contains("%s"))
        url = QString(engine).replace("%s", encoded);
    else if (engine.contains("duckduckgo", Qt::CaseInsensitive))
        url = engine + "/?q=" + encoded;
    else
        url = engine + "/search?q=" + encoded;
    QDesktopServices::openUrl(QUrl(url));
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, tr("About Cricket"),
        tr("<h3>Cricket</h3><p>%1</p>"
           "<p>A lightweight multi-server IRC client, originally written for Haiku OS "
           "and ported to Linux with Qt %2.</p>"
           "<p>CertFP and SASL, Gemini live translation, aspell spell checking, "
           "nick colours, ignore lists and emoticons.</p>"
           "<p><a href=\"https://github.com/ablyssx74/cricket\">github.com/ablyssx74/cricket</a></p>"
           "<p>MIT License</p>").arg(AppInfo::VERSION_STRING, QT_VERSION_STR));
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

bool MainWindow::eventFilter(QObject* obj, QEvent* e)
{
    if (obj == fChat && e->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(e);
        if (!ke->text().isEmpty() && ke->text().at(0).isPrint() && !(ke->modifiers() & (Qt::ControlModifier | Qt::AltModifier))) {
            fInput->setFocus();
            fInput->insertAtCursor(ke->text());
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, e);
}

void MainWindow::closeEvent(QCloseEvent* e)
{
    QSettings settings;
    settings.setValue("window/geometry", saveGeometry());
    settings.setValue("window/splitter", fSplitter->saveState());
    for (Session* s : fSessions)
        if (s->conn->isConnected())
            s->conn->disconnectFromServer(cfg.fullQuitMessage());
    saveConfig();
    e->accept();
}
