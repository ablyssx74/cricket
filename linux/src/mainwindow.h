/*
 * Cricket IRC Client - Linux port
 * Distributed under the terms of the MIT license.
 */
#ifndef CRICKET_MAINWINDOW_H
#define CRICKET_MAINWINDOW_H

#include "config.h"
#include "ircmessage.h"

#include <QHash>
#include <QMainWindow>
#include <QMap>
#include <QPointer>

class ChatView;
class ChannelListDialog;
class ChannelModesDialog;
class InputEdit;
class IrcConnection;
class QLabel;
class QLineEdit;
class QListWidget;
class QSplitter;
class QTreeWidget;
class QTreeWidgetItem;
class QToolButton;
class Translator;
class UpdateChecker;
struct Session;

struct ChannelUser {
    QString nick;
    QString prefixes; // status symbols held, highest first (e.g. "@+")
    bool away = false;
};

struct Buffer {
    enum Type { Server, Channel, Query };
    Type type = Server;
    QString name;
    Session* session = nullptr;
    QTreeWidgetItem* item = nullptr;
    QStringList lines;             // rendered HTML
    QMap<QString, ChannelUser> users; // keyed by lower-case nick
    QMap<QString, ChannelUser> pendingNames; // collecting 353 replies
    bool namesInProgress = false;
    QString topic;
    QString modes, key, limit;
    bool joined = false;
    bool unread = false;
    bool highlighted = false;
    qint64 lastTimestamp = 0;      // msecs since epoch of last printed stamp
};

struct Session {
    IrcConnection* conn = nullptr;
    bool custom = false;
    int index = 0;
    Buffer* server = nullptr;
    QList<Buffer*> buffers;        // channels and queries
    QPointer<ChannelListDialog> listDialog;
    bool away = false;
    bool identifiedWithServices = false;
    QString pendingFocus;          // channel to focus when its JOIN arrives
    QStringList rejoinChannels;    // channels to rejoin after a reconnect

    ServerConfig& profile() const;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void autoConnect();

protected:
    void closeEvent(QCloseEvent* e) override;
    bool eventFilter(QObject* obj, QEvent* e) override;

private:
    // Construction
    void buildUi();
    void buildMenus();
    Session* createSession(bool custom, int index);
    void wireConnection(Session* s);

    // Buffers
    Buffer* findBuffer(Session* s, const QString& name) const;
    Buffer* ensureBuffer(Session* s, const QString& name, Buffer::Type type, bool focus = false);
    void removeBuffer(Buffer* b);
    void setActiveBuffer(Buffer* b);
    Buffer* bufferForItem(QTreeWidgetItem* item) const;
    Session* currentSession() const;
    void updateTreeItem(Buffer* b);
    void refreshUserList();
    void refreshTopic();
    void refreshStatus();
    void applyAppearance();
    void logToFile(Buffer* b, const QString& plainLine);

    // Output
    enum class LineKind { Normal, Action, Notice, Status, Join, Part, Error, Own, Highlight };
    void print(Buffer* b, const QString& html, LineKind kind = LineKind::Status, const QString& plain = QString());
    void printStatus(Session* s, const QString& text, LineKind kind = LineKind::Status);
    void printActiveOrServer(Session* s, const QString& text, LineKind kind = LineKind::Status);
    void printMessage(Buffer* b, const QString& nick, const QString& body, LineKind kind, bool own);
    QString timestampFor(Buffer* b, bool force = false);
    QString nickHtml(Session* s, const QString& nick) const;
    QString formatBody(Session* s, const QString& body) const;
    bool mentionsNick(const QString& text, const QString& nick) const;

    // Input
    void onSubmit(const QString& text);
    void sendLine(Buffer* b, const QString& line);
    void processCommand(Session* s, Buffer* b, const QString& line);
    void sendPrivmsg(Session* s, Buffer* b, const QString& target, const QString& text, bool echo = true);
    void sendAction(Session* s, Buffer* b, const QString& target, const QString& text);

    // IRC events
    void onMessage(Session* s, const IrcMessage& m);
    void onRegistered(Session* s);
    void onDisconnected(Session* s);
    void handlePrivmsg(Session* s, const IrcMessage& m, const QString& body);
    void handleNames(Session* s, const IrcMessage& m);
    void handleMode(Session* s, const IrcMessage& m);
    void handleWhois(Session* s, const IrcMessage& m);
    bool isIgnored(Session* s, const QString& nick) const;
    int rankOf(Session* s, const ChannelUser& u) const;

    // Actions
    void connectSession(Session* s);
    void disconnectSession(Session* s);
    void configureSession(Session* s);
    void addServer();
    void removeSession(Session* s);
    void openChannelList(Session* s);
    void joinChannelPrompt(Session* s);
    void showTreeMenu(const QPoint& pos);
    void showUserMenu(const QPoint& pos);
    void openModesDialog(Buffer* b);
    void toggleAway(Session* s);
    void openSearch(const QString& text, bool translate);
    void showAbout();
    void registerFingerprint(Session* s, const QString& sha1, const QString& sha512);

    QList<Session*> fSessions;
    Buffer* fActive = nullptr;

    QSplitter* fSplitter = nullptr;
    QTreeWidget* fTree = nullptr;
    QLineEdit* fTopic = nullptr;
    ChatView* fChat = nullptr;
    InputEdit* fInput = nullptr;
    QToolButton* fEmoteButton = nullptr;
    QListWidget* fUsers = nullptr;
    QLabel* fUserCount = nullptr;
    QWidget* fUserPanel = nullptr;
    QLabel* fStatusLabel = nullptr;
    QAction* fAwayAction = nullptr;
    QPointer<ChannelModesDialog> fModesDialog;

    Translator* fTranslator = nullptr;
    UpdateChecker* fUpdates = nullptr;
    QString fPendingReviewedTranslation;
};

#endif
