/*
 * Cricket IRC Client - Linux port
 * Distributed under the terms of the MIT license.
 */
#include "dialogs.h"

#include "services.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSlider>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QSslCertificate>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QTableView>
#include <QTableWidget>
#include <QVBoxLayout>

static const QStringList kLanguages = {
    "Arabic", "Bengali", "Chinese (Simplified)", "Chinese (Traditional)", "Czech", "Danish", "Dutch",
    "English", "Finnish", "French", "German", "Greek", "Hebrew", "Hindi", "Hungarian", "Indonesian",
    "Italian", "Japanese", "Korean", "Norwegian", "Persian", "Polish", "Portuguese", "Romanian",
    "Russian", "Spanish", "Swedish", "Thai", "Turkish", "Ukrainian", "Vietnamese",
};

static QString joinLines(const QStringList& list)
{
    QStringList clean;
    for (const QString& s : list)
        if (!s.trimmed().isEmpty())
            clean << s.trimmed();
    return clean.join('\n');
}

static QStringList splitLines(const QString& text)
{
    QStringList out;
    for (const QString& s : text.split('\n'))
        if (!s.trimmed().isEmpty())
            out << s.trimmed();
    return out;
}

static QComboBox* languageCombo(const QString& current)
{
    auto* combo = new QComboBox;
    combo->setEditable(true);
    combo->addItems(kLanguages);
    combo->setCurrentText(current);
    return combo;
}

static QSpinBox* spin(int min, int max, int value, const QString& suffix = QString())
{
    auto* s = new QSpinBox;
    s->setRange(min, max);
    s->setValue(value);
    s->setSuffix(suffix);
    return s;
}

// ---------------------------------------------------------------------------
// ServerDialog
// ---------------------------------------------------------------------------

ServerDialog::ServerDialog(const ServerConfig& profile, bool isNew, QWidget* parent)
    : QDialog(parent), fProfile(profile)
{
    setWindowTitle(isNew ? tr("Add Server") : tr("Configure %1").arg(profile.name));
    resize(560, 520);

    auto* tabs = new QTabWidget;

    // --- General ---
    auto* general = new QWidget;
    auto* gf = new QFormLayout(general);
    fName = new QLineEdit(profile.name);
    fHost = new QLineEdit(profile.host);
    fPort = spin(1, 65535, profile.port);
    fUseTLS = new QCheckBox(tr("Use TLS encryption"));
    fUseTLS->setChecked(profile.useTLS);
    fVerifyTLS = new QCheckBox(tr("Verify server certificate"));
    fVerifyTLS->setChecked(profile.verifyTLS);
    connect(fUseTLS, &QCheckBox::toggled, fVerifyTLS, &QWidget::setEnabled);
    fVerifyTLS->setEnabled(profile.useTLS);
    fNick = new QLineEdit(profile.nick);
    fAltNick = new QLineEdit(profile.altNick);
    fAltNick2 = new QLineEdit(profile.altNick2);
    fPass = new QLineEdit(profile.pass);
    fPass->setEchoMode(QLineEdit::Password);
    fPass->setPlaceholderText(tr("Server / NickServ password"));
    fAutoConnect = new QCheckBox(tr("Connect on startup"));
    fAutoConnect->setChecked(profile.autoConnect);
    fAutoReconnect = new QCheckBox(tr("Reconnect automatically when dropped"));
    fAutoReconnect->setChecked(profile.autoReconnect);
    fHideStatus = new QCheckBox(tr("Hide join/part/quit messages"));
    fHideStatus->setChecked(profile.hideStatusMessages);
    fNickAlert = new QCheckBox(tr("Desktop notification when my nick is mentioned"));
    fNickAlert->setChecked(profile.nickAlert);
    fLogToFile = new QCheckBox(tr("Log chats to ~/.local/share/cricket/logs"));
    fLogToFile->setChecked(profile.logChatsToFile);
    gf->addRow(tr("Network name:"), fName);
    gf->addRow(tr("Host:"), fHost);
    gf->addRow(tr("Port:"), fPort);
    gf->addRow(QString(), fUseTLS);
    gf->addRow(QString(), fVerifyTLS);
    gf->addRow(tr("Nickname:"), fNick);
    gf->addRow(tr("Alt nick 1:"), fAltNick);
    gf->addRow(tr("Alt nick 2:"), fAltNick2);
    gf->addRow(tr("Password:"), fPass);
    gf->addRow(QString(), fAutoConnect);
    gf->addRow(QString(), fAutoReconnect);
    gf->addRow(QString(), fHideStatus);
    gf->addRow(QString(), fNickAlert);
    gf->addRow(QString(), fLogToFile);
    tabs->addTab(general, tr("General"));

    // --- Authentication ---
    auto* auth = new QWidget;
    auto* af = new QFormLayout(auth);
    fUseSASL = new QCheckBox(tr("Enable SASL authentication"));
    fUseSASL->setChecked(profile.useSASL);
    fSaslUser = new QLineEdit(profile.saslUser);
    fSaslUser->setPlaceholderText(tr("Account name (defaults to nickname)"));
    fUseCertFP = new QCheckBox(tr("Enable TLS client certificate (CertFP)"));
    fUseCertFP->setChecked(profile.useCertFP);
    fCertProfile = new QLineEdit(profile.certProfileName);
    fCertProfile->setPlaceholderText(tr("Combined .pem (optional)"));
    fCertFile = new QLineEdit(profile.certFileName);
    fCertFile->setPlaceholderText("nick.crt");
    fKeyFile = new QLineEdit(profile.keyFileName);
    fKeyFile->setPlaceholderText("nick.key");
    auto* genButton = new QPushButton(tr("Generate Fresh CertFP Keypair"));
    connect(genButton, &QPushButton::clicked, this, &ServerDialog::generateCertificate);
    fFingerprint = new QLabel;
    fFingerprint->setTextInteractionFlags(Qt::TextSelectableByMouse);
    fFingerprint->setWordWrap(true);
    auto* authNote = new QLabel(tr("With SASL and CertFP both enabled, Cricket logs in with SASL EXTERNAL. "
        "With SASL only, it uses SASL PLAIN with the password from the General tab. "
        "Certificate files are looked up in %1.").arg(certsDir()));
    authNote->setWordWrap(true);
    af->addRow(fUseSASL);
    af->addRow(tr("SASL username:"), fSaslUser);
    af->addRow(fUseCertFP);
    af->addRow(tr("Cert profile (.pem):"), fCertProfile);
    af->addRow(tr("Public cert (.crt):"), fCertFile);
    af->addRow(tr("Private key (.key):"), fKeyFile);
    af->addRow(genButton);
    af->addRow(fFingerprint);
    af->addRow(authNote);
    tabs->addTab(auth, tr("Authentication"));

    // --- Channels ---
    auto* chans = new QWidget;
    auto* cl = new QVBoxLayout(chans);
    fAutojoin = new QPlainTextEdit(joinLines(profile.autojoin));
    fAutojoin->setPlaceholderText("#linux\n#cachyos");
    fAutocmd = new QPlainTextEdit(joinLines(profile.autocmdlist));
    fAutocmd->setPlaceholderText("/msg NickServ IDENTIFY password\nMODE mynick +i");
    cl->addWidget(new QLabel(tr("Auto-join channels (one per line):")));
    cl->addWidget(fAutojoin);
    cl->addWidget(new QLabel(tr("Commands to run after connecting (one per line):")));
    cl->addWidget(fAutocmd);
    tabs->addTab(chans, tr("Channels"));

    // --- Appearance ---
    auto* look = new QWidget;
    auto* lf = new QFormLayout(look);
    fEmoticons = new QCheckBox(tr("Show emoticons as emoji"));
    fEmoticons->setChecked(profile.enableEmoticons);
    fColorCodes = new QCheckBox(tr("Render mIRC colour and formatting codes"));
    fColorCodes->setChecked(profile.enableColorCodes);
    fBgImage = new QLineEdit(profile.backgroundImagePath);
    auto* browse = new QPushButton(tr("Browse…"));
    connect(browse, &QPushButton::clicked, this, &ServerDialog::browseBackground);
    auto* bgRow = new QHBoxLayout;
    bgRow->addWidget(fBgImage);
    bgRow->addWidget(browse);
    fBgOpacity = new QSlider(Qt::Horizontal);
    fBgOpacity->setRange(0, 100);
    fBgOpacity->setValue(profile.backgroundOpacity);
    fTimestamp = spin(0, 1440, profile.timestampInterval, tr(" min"));
    fTimestamp->setSpecialValueText(tr("Every line"));
    fServerFont = spin(6, 48, profile.serverListFontSize, " pt");
    fChatFont = spin(6, 48, profile.chatLogFontSize, " pt");
    fUserFont = spin(6, 48, profile.userListFontSize, " pt");
    lf->addRow(fEmoticons);
    lf->addRow(fColorCodes);
    lf->addRow(tr("Background image:"), bgRow);
    lf->addRow(tr("Background opacity:"), fBgOpacity);
    lf->addRow(tr("Timestamp interval:"), fTimestamp);
    lf->addRow(tr("Server list font:"), fServerFont);
    lf->addRow(tr("Chat font:"), fChatFont);
    lf->addRow(tr("User list font:"), fUserFont);
    tabs->addTab(look, tr("Appearance"));

    // --- Ignore & colours ---
    auto* people = new QWidget;
    auto* pl = new QVBoxLayout(people);
    fIgnored = new QPlainTextEdit(joinLines(profile.ignoredNicks));
    fIgnored->setPlaceholderText("spammer*\n*bot");
    pl->addWidget(new QLabel(tr("Ignored nicks (wildcards * and ? allowed, one per line):")));
    pl->addWidget(fIgnored);
    pl->addWidget(new QLabel(tr("Nick colour rules:")));
    fNickColors = new QTableWidget(0, 2);
    fNickColors->setHorizontalHeaderLabels({tr("Nick pattern"), tr("Colour")});
    fNickColors->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    fNickColors->verticalHeader()->hide();
    auto addColorRow = [this](const QString& pattern, const QColor& color) {
        int row = fNickColors->rowCount();
        fNickColors->insertRow(row);
        fNickColors->setItem(row, 0, new QTableWidgetItem(pattern));
        auto* c = new QTableWidgetItem(color.name());
        c->setBackground(color);
        c->setData(Qt::UserRole, color);
        c->setFlags(c->flags() & ~Qt::ItemIsEditable);
        fNickColors->setItem(row, 1, c);
    };
    for (int i = 0; i < profile.nickColors.size(); ++i)
        addColorRow(profile.nickColors.at(i), profile.nickColorValues.value(i, QColor(Qt::red)));
    connect(fNickColors, &QTableWidget::cellDoubleClicked, this, [this](int row, int col) {
        if (col != 1)
            return;
        QColor c = QColorDialog::getColor(fNickColors->item(row, 1)->data(Qt::UserRole).value<QColor>(), this);
        if (!c.isValid())
            return;
        fNickColors->item(row, 1)->setBackground(c);
        fNickColors->item(row, 1)->setText(c.name());
        fNickColors->item(row, 1)->setData(Qt::UserRole, c);
    });
    auto* colorButtons = new QHBoxLayout;
    auto* addColor = new QPushButton(tr("Add rule"));
    auto* removeColor = new QPushButton(tr("Remove rule"));
    connect(addColor, &QPushButton::clicked, this, [this, addColorRow]() {
        QColor c = QColorDialog::getColor(Qt::red, this);
        if (c.isValid()) {
            addColorRow("nick*", c);
            fNickColors->editItem(fNickColors->item(fNickColors->rowCount() - 1, 0));
        }
    });
    connect(removeColor, &QPushButton::clicked, this, [this]() {
        int row = fNickColors->currentRow();
        if (row >= 0)
            fNickColors->removeRow(row);
    });
    colorButtons->addWidget(addColor);
    colorButtons->addWidget(removeColor);
    colorButtons->addStretch();
    pl->addWidget(fNickColors);
    pl->addLayout(colorButtons);
    tabs->addTab(people, tr("Ignore && Colours"));

    // --- Translation ---
    auto* tr_ = new QWidget;
    auto* tf = new QFormLayout(tr_);
    fApiKey = new QLineEdit(profile.geminiApiKey);
    fApiKey->setEchoMode(QLineEdit::Password);
    fModel = new QLineEdit(profile.geminiModel);
    fInbound = new QCheckBox(tr("Translate incoming messages"));
    fInbound->setChecked(profile.enableInboundTranslation);
    fTargetLang = languageCombo(profile.targetLanguage);
    fOutbound = new QCheckBox(tr("Translate what I type"));
    fOutbound->setChecked(profile.enableOutboundTranslation);
    fOutLang = languageCombo(profile.outboundTargetLanguage);
    fAutoSend = new QCheckBox(tr("Send translations immediately (otherwise review first)"));
    fAutoSend->setChecked(profile.autoSendTranslatedOutbound);
    tf->addRow(tr("Gemini API key:"), fApiKey);
    tf->addRow(tr("Model:"), fModel);
    tf->addRow(fInbound);
    tf->addRow(tr("Translate incoming to:"), fTargetLang);
    tf->addRow(fOutbound);
    tf->addRow(tr("Translate outgoing to:"), fOutLang);
    tf->addRow(fAutoSend);
    tabs->addTab(tr_, tr("Translation"));

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &ServerDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(tabs);
    layout->addWidget(buttons);
}

void ServerDialog::accept()
{
    if (fName->text().trimmed().isEmpty() || fHost->text().trimmed().isEmpty() || fNick->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, windowTitle(), tr("Network name, host and nickname are required."));
        return;
    }
    QDialog::accept();
}

ServerConfig ServerDialog::result() const
{
    ServerConfig s = fProfile;
    s.name = fName->text().trimmed();
    s.host = fHost->text().trimmed();
    s.port = static_cast<quint16>(fPort->value());
    s.useTLS = fUseTLS->isChecked();
    s.verifyTLS = fVerifyTLS->isChecked();
    s.nick = fNick->text().trimmed();
    s.altNick = fAltNick->text().trimmed();
    s.altNick2 = fAltNick2->text().trimmed();
    s.pass = fPass->text();
    s.autoConnect = fAutoConnect->isChecked();
    s.autoReconnect = fAutoReconnect->isChecked();
    s.hideStatusMessages = fHideStatus->isChecked();
    s.nickAlert = fNickAlert->isChecked();
    s.logChatsToFile = fLogToFile->isChecked();
    s.useSASL = fUseSASL->isChecked();
    s.saslUser = fSaslUser->text().trimmed();
    s.useCertFP = fUseCertFP->isChecked();
    s.certProfileName = fCertProfile->text().trimmed();
    s.certFileName = fCertFile->text().trimmed();
    s.keyFileName = fKeyFile->text().trimmed();
    s.autojoin = splitLines(fAutojoin->toPlainText());
    s.autocmdlist = splitLines(fAutocmd->toPlainText());
    s.ignoredNicks = splitLines(fIgnored->toPlainText());
    s.enableEmoticons = fEmoticons->isChecked();
    s.enableColorCodes = fColorCodes->isChecked();
    s.backgroundImagePath = fBgImage->text().trimmed();
    s.backgroundOpacity = fBgOpacity->value();
    s.timestampInterval = fTimestamp->value();
    s.serverListFontSize = fServerFont->value();
    s.chatLogFontSize = fChatFont->value();
    s.userListFontSize = fUserFont->value();
    s.nickColors.clear();
    s.nickColorValues.clear();
    for (int r = 0; r < fNickColors->rowCount(); ++r) {
        QString pattern = fNickColors->item(r, 0) ? fNickColors->item(r, 0)->text().trimmed() : QString();
        if (pattern.isEmpty())
            continue;
        s.nickColors << pattern;
        s.nickColorValues << fNickColors->item(r, 1)->data(Qt::UserRole).value<QColor>();
    }
    s.geminiApiKey = fApiKey->text().trimmed();
    s.geminiModel = fModel->text().trimmed().isEmpty() ? QString("gemini-3.5-flash-lite") : fModel->text().trimmed();
    s.enableInboundTranslation = fInbound->isChecked();
    s.targetLanguage = fTargetLang->currentText();
    s.enableOutboundTranslation = fOutbound->isChecked();
    s.outboundTargetLanguage = fOutLang->currentText();
    s.autoSendTranslatedOutbound = fAutoSend->isChecked();
    return s;
}

void ServerDialog::browseBackground()
{
    QString file = QFileDialog::getOpenFileName(this, tr("Choose background image"), fBgImage->text(),
        tr("Images (*.png *.jpg *.jpeg *.webp *.bmp *.gif)"));
    if (!file.isEmpty())
        fBgImage->setText(file);
}

void ServerDialog::generateCertificate()
{
    QString token = fName->text().trimmed().toLower();
    token.replace(' ', '_');
    token.replace('/', '-');
    if (token.isEmpty())
        token = "generic_irc_network";
    QString nick = fNick->text().trimmed();
    if (nick.isEmpty())
        nick = "cricket_user";

    const QString dir = certsDir();
    const QString crt = QString("cert_%1.crt").arg(token);
    const QString key = QString("cert_%1.key").arg(token);
    const QString pem = QString("cert_%1.pem").arg(token);

    if (QFile::exists(dir + "/" + crt)) {
        auto answer = QMessageBox::question(this, tr("CertFP"),
            tr("A certificate for this network already exists. Replace it?\n\n"
               "You will need to register the new fingerprint with NickServ."));
        if (answer != QMessageBox::Yes)
            return;
    }

    QProcess proc;
    proc.start("openssl", {"req", "-new", "-newkey", "rsa:2048", "-days", "3650", "-nodes", "-x509",
        "-subj", QString("/CN=%1/O=CricketClient/OU=IRC_Network_%2").arg(nick, token),
        "-keyout", dir + "/" + key, "-out", dir + "/" + crt});
    if (!proc.waitForFinished(60000) || proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        QMessageBox::critical(this, tr("CertFP"),
            tr("Failed to generate the certificate. Make sure the 'openssl' package is installed.\n\n%1")
                .arg(QString::fromLocal8Bit(proc.readAllStandardError())));
        return;
    }
    QFile::setPermissions(dir + "/" + key, QFileDevice::ReadOwner | QFileDevice::WriteOwner);

    QFile crtFile(dir + "/" + crt), keyFile(dir + "/" + key), pemFile(dir + "/" + pem);
    if (crtFile.open(QIODevice::ReadOnly) && keyFile.open(QIODevice::ReadOnly) && pemFile.open(QIODevice::WriteOnly)) {
        QByteArray crtData = crtFile.readAll();
        pemFile.write(crtData);
        pemFile.write(keyFile.readAll());
        pemFile.close();
        pemFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);

        QList<QSslCertificate> certs = QSslCertificate::fromData(crtData, QSsl::Pem);
        if (!certs.isEmpty()) {
            QString sha1 = QString::fromLatin1(certs.first().digest(QCryptographicHash::Sha1).toHex());
            QString sha512 = QString::fromLatin1(certs.first().digest(QCryptographicHash::Sha512).toHex());
            fFingerprint->setText(tr("SHA-1: %1\nSHA-512: %2").arg(sha1, sha512));
            emit registerFingerprint(sha1, sha512);
        }
    }

    fCertProfile->setText(pem);
    fCertFile->setText(crt);
    fKeyFile->setText(key);
    fUseCertFP->setChecked(true);

    QMessageBox::information(this, tr("CertFP"),
        tr("Generated a 2048-bit CertFP keypair for [%1].\n\n"
           "If you are connected to this network, Cricket has sent NickServ a CERT ADD with the new fingerprint.\n\n"
           "Click Save and reconnect for the certificate to be used.").arg(token));
}

// ---------------------------------------------------------------------------
// SettingsDialog
// ---------------------------------------------------------------------------

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Preferences"));
    auto* form = new QFormLayout;
    fQuit = new QLineEdit(cfg.quitMessage);
    fQuit->setPlaceholderText(tr("(version is appended automatically)"));
    fAway = new QLineEdit(cfg.awayMessage);
    fSearch = new QLineEdit(cfg.searchEngine);
    fSpell = new QCheckBox(tr("Check spelling while typing"));
    fSpell->setChecked(cfg.enableSpellCheck);
    fSpell->setEnabled(SpellChecker::instance().isAvailable() || !cfg.enableSpellCheck);
    fSpellLang = new QLineEdit(cfg.spellLanguage);
    fSpellLang->setPlaceholderText("en_US");
    fUpdates = new QCheckBox(tr("Notify me when a new version is available"));
    fUpdates->setChecked(cfg.showUpdateNotifications);
    fDebug = new QCheckBox(tr("Print raw IRC traffic to the terminal (debug)"));
    fDebug->setChecked(cfg.debugEnable);

    form->addRow(tr("Quit message:"), fQuit);
    form->addRow(tr("Away message:"), fAway);
    form->addRow(tr("Search engine:"), fSearch);
    form->addRow(fSpell);
    form->addRow(tr("Spelling language:"), fSpellLang);
    if (!SpellChecker::instance().isAvailable()) {
        auto* note = new QLabel(tr("Spell checking needs aspell and a dictionary (e.g. aspell-en)."));
        note->setWordWrap(true);
        form->addRow(note);
    }
    form->addRow(fUpdates);
    form->addRow(fDebug);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
    resize(460, sizeHint().height());
}

void SettingsDialog::apply()
{
    cfg.quitMessage = fQuit->text().trimmed();
    cfg.awayMessage = fAway->text().trimmed();
    cfg.searchEngine = fSearch->text().trimmed().isEmpty() ? QString("https://duckduckgo.com") : fSearch->text().trimmed();
    cfg.enableSpellCheck = fSpell->isChecked();
    QString lang = fSpellLang->text().trimmed();
    if (!lang.isEmpty() && lang != cfg.spellLanguage && SpellChecker::instance().setLanguage(lang))
        cfg.spellLanguage = lang;
    cfg.showUpdateNotifications = fUpdates->isChecked();
    cfg.debugEnable = fDebug->isChecked();
    saveConfig();
}

// ---------------------------------------------------------------------------
// ChannelListDialog
// ---------------------------------------------------------------------------

class NumericSortProxy : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

protected:
    bool lessThan(const QModelIndex& l, const QModelIndex& r) const override
    {
        if (l.column() == 1)
            return l.data(Qt::UserRole).toInt() < r.data(Qt::UserRole).toInt();
        return QSortFilterProxyModel::lessThan(l, r);
    }
};

ChannelListDialog::ChannelListDialog(const QString& serverName, QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Channel List — %1").arg(serverName));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(720, 520);

    fModel = new QStandardItemModel(0, 3, this);
    fModel->setHorizontalHeaderLabels({tr("Channel"), tr("Users"), tr("Topic")});
    fProxy = new NumericSortProxy(this);
    fProxy->setSourceModel(fModel);
    fProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    fProxy->setFilterKeyColumn(-1);

    fFilter = new QLineEdit;
    fFilter->setPlaceholderText(tr("Filter channels and topics…"));
    fFilter->setClearButtonEnabled(true);
    connect(fFilter, &QLineEdit::textChanged, fProxy, &QSortFilterProxyModel::setFilterFixedString);

    fView = new QTableView;
    fView->setModel(fProxy);
    fView->setSortingEnabled(true);
    fView->setSelectionBehavior(QAbstractItemView::SelectRows);
    fView->setSelectionMode(QAbstractItemView::SingleSelection);
    fView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    fView->verticalHeader()->hide();
    fView->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    fView->sortByColumn(1, Qt::DescendingOrder);
    connect(fView, &QTableView::doubleClicked, this, [this](const QModelIndex& idx) {
        emit joinRequested(fProxy->index(idx.row(), 0).data().toString());
    });

    fStatus = new QLabel(tr("Requesting channel list…"));
    auto* join = new QPushButton(tr("Join"));
    auto* refresh = new QPushButton(tr("Refresh"));
    auto* close = new QPushButton(tr("Close"));
    connect(join, &QPushButton::clicked, this, [this]() {
        QModelIndex idx = fView->currentIndex();
        if (idx.isValid())
            emit joinRequested(fProxy->index(idx.row(), 0).data().toString());
    });
    connect(refresh, &QPushButton::clicked, this, [this]() {
        clear();
        emit refreshRequested();
    });
    connect(close, &QPushButton::clicked, this, &QDialog::close);

    auto* bottom = new QHBoxLayout;
    bottom->addWidget(fStatus, 1);
    bottom->addWidget(refresh);
    bottom->addWidget(join);
    bottom->addWidget(close);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(fFilter);
    layout->addWidget(fView);
    layout->addLayout(bottom);
}

void ChannelListDialog::clear()
{
    fModel->removeRows(0, fModel->rowCount());
    fStatus->setText(tr("Requesting channel list…"));
}

void ChannelListDialog::addChannel(const QString& name, int users, const QString& topic)
{
    auto* nameItem = new QStandardItem(name);
    auto* usersItem = new QStandardItem(QString::number(users));
    usersItem->setData(users, Qt::UserRole);
    usersItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    auto* topicItem = new QStandardItem(topic);
    topicItem->setToolTip(topic);
    fModel->appendRow({nameItem, usersItem, topicItem});
    if (fModel->rowCount() % 200 == 0)
        fStatus->setText(tr("Received %1 channels…").arg(fModel->rowCount()));
}

void ChannelListDialog::finished()
{
    fStatus->setText(tr("%1 channels").arg(fModel->rowCount()));
    fProxy->sort(fProxy->sortColumn(), fProxy->sortOrder());
}

// ---------------------------------------------------------------------------
// ChannelModesDialog
// ---------------------------------------------------------------------------

ChannelModesDialog::ChannelModesDialog(const QString& channel, QWidget* parent)
    : QDialog(parent), fChannel(channel)
{
    setWindowTitle(tr("Channel Modes — %1").arg(channel));
    setAttribute(Qt::WA_DeleteOnClose);
    auto* layout = new QVBoxLayout(this);
    const QVector<QPair<QChar, QString>> flags = {
        {'m', tr("Moderated (+m)")},
        {'s', tr("Secret (+s)")},
        {'i', tr("Invite only (+i)")},
        {'t', tr("Only ops set topic (+t)")},
        {'n', tr("No external messages (+n)")},
        {'c', tr("Strip colours (+c)")},
    };
    for (const auto& f : flags) {
        auto* box = new QCheckBox(f.second);
        fFlags << qMakePair(f.first, box);
        layout->addWidget(box);
    }
    auto* form = new QFormLayout;
    fKey = new QLineEdit;
    fKey->setPlaceholderText(tr("none"));
    fLimit = new QLineEdit;
    fLimit->setPlaceholderText(tr("none"));
    fLimit->setValidator(new QIntValidator(0, 100000, fLimit));
    form->addRow(tr("Key (+k):"), fKey);
    form->addRow(tr("User limit (+l):"), fLimit);
    layout->addLayout(form);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Close);
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &ChannelModesDialog::apply);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    layout->addWidget(buttons);
}

void ChannelModesDialog::setModes(const QString& modes, const QString& key, const QString& limit)
{
    fInitialModes = modes;
    fInitialKey = key;
    fInitialLimit = limit;
    for (auto& f : fFlags)
        f.second->setChecked(modes.contains(f.first));
    fKey->setText(key);
    fLimit->setText(limit);
}

void ChannelModesDialog::apply()
{
    QString plus, minus;
    QStringList plusArgs, minusArgs;
    for (auto& f : fFlags) {
        bool was = fInitialModes.contains(f.first);
        bool now = f.second->isChecked();
        if (now && !was) plus += f.first;
        if (!now && was) minus += f.first;
    }
    QString key = fKey->text().trimmed();
    if (key != fInitialKey) {
        if (!fInitialKey.isEmpty()) { minus += 'k'; minusArgs << fInitialKey; }
        if (!key.isEmpty()) { plus += 'k'; plusArgs << key; }
    }
    QString limit = fLimit->text().trimmed();
    if (limit != fInitialLimit) {
        if (limit.isEmpty() || limit == "0") minus += 'l';
        else { plus += 'l'; plusArgs << limit; }
    }
    QString modeString;
    if (!minus.isEmpty()) modeString += "-" + minus;
    if (!plus.isEmpty()) modeString += "+" + plus;
    if (modeString.isEmpty())
        return;
    QStringList args = minusArgs + plusArgs;
    if (!args.isEmpty())
        modeString += " " + args.join(' ');
    emit applyModes(fChannel, modeString);

    QString newModes;
    for (auto& f : fFlags)
        if (f.second->isChecked())
            newModes += f.first;
    fInitialModes = newModes;
    fInitialKey = key;
    fInitialLimit = limit;
}
