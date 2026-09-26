/*
 * Cricket IRC Client - Linux port
 * Distributed under the terms of the MIT license.
 */
#ifndef CRICKET_DIALOGS_H
#define CRICKET_DIALOGS_H

#include "config.h"

#include <QDialog>
#include <QVector>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;
class QSlider;
class QSortFilterProxyModel;
class QStandardItemModel;
class QTableView;
class QTableWidget;

// Editor for one server profile.
class ServerDialog : public QDialog {
    Q_OBJECT
public:
    ServerDialog(const ServerConfig& profile, bool isNew, QWidget* parent = nullptr);
    ServerConfig result() const;

signals:
    // Emitted after a CertFP keypair is generated; the window forwards it to
    // NickServ if the network is connected.
    void registerFingerprint(const QString& sha1, const QString& sha512);

private:
    void generateCertificate();
    void browseBackground();
    void accept() override;

    ServerConfig fProfile;

    QLineEdit *fName, *fHost, *fNick, *fAltNick, *fAltNick2, *fPass;
    QSpinBox* fPort;
    QCheckBox *fUseTLS, *fVerifyTLS, *fAutoConnect, *fAutoReconnect, *fHideStatus, *fNickAlert, *fLogToFile;
    QCheckBox *fUseSASL, *fUseCertFP;
    QLineEdit *fSaslUser, *fCertProfile, *fCertFile, *fKeyFile;
    QLabel* fFingerprint;
    QPlainTextEdit *fAutojoin, *fAutocmd, *fIgnored;
    QCheckBox *fEmoticons, *fColorCodes;
    QLineEdit* fBgImage;
    QSlider* fBgOpacity;
    QSpinBox *fTimestamp, *fServerFont, *fChatFont, *fUserFont;
    QTableWidget* fNickColors;
    QCheckBox *fInbound, *fOutbound, *fAutoSend;
    QLineEdit *fApiKey, *fModel;
    QComboBox *fTargetLang, *fOutLang;
};

// Application-wide preferences.
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    void apply();

private:
    QLineEdit *fQuit, *fAway, *fSearch, *fSpellLang;
    QCheckBox *fSpell, *fUpdates, *fDebug;
};

// Browser for the reply to LIST.
class ChannelListDialog : public QDialog {
    Q_OBJECT
public:
    explicit ChannelListDialog(const QString& serverName, QWidget* parent = nullptr);
    void clear();
    void addChannel(const QString& name, int users, const QString& topic);
    void finished();

signals:
    void joinRequested(const QString& channel);
    void refreshRequested();

private:
    QStandardItemModel* fModel;
    QSortFilterProxyModel* fProxy;
    QTableView* fView;
    QLabel* fStatus;
    QLineEdit* fFilter;
};

// Toggle common channel modes and key/limit.
class ChannelModesDialog : public QDialog {
    Q_OBJECT
public:
    ChannelModesDialog(const QString& channel, QWidget* parent = nullptr);
    QString channel() const { return fChannel; }
    void setModes(const QString& modes, const QString& key, const QString& limit);

signals:
    void applyModes(const QString& channel, const QString& modeString);

private:
    void apply();

    QString fChannel;
    QString fInitialModes;
    QString fInitialKey;
    QString fInitialLimit;
    QVector<QPair<QChar, QCheckBox*>> fFlags;
    QLineEdit *fKey, *fLimit;
};

#endif
