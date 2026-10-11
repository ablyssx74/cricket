/*
 * Cricket IRC Client - Linux port
 * Distributed under the terms of the MIT license.
 */
#ifndef CRICKET_IRCMESSAGE_H
#define CRICKET_IRCMESSAGE_H

#include <QMap>
#include <QString>
#include <QStringList>

// One parsed IRC protocol line: [@tags] [:prefix] COMMAND [params...] [:trailing]
// The trailing parameter, if any, is the last element of params.
struct IrcMessage {
    QMap<QString, QString> tags;
    QString prefix;
    QString command;
    QStringList params;
    bool hasTrailing = false;
    QString raw;

    static IrcMessage parse(const QString& line);

    QString nick() const;        // nick part of prefix
    QString userHost() const;    // user@host part of prefix
    QString param(int i) const { return i >= 0 && i < params.size() ? params.at(i) : QString(); }
    QString last() const { return params.isEmpty() ? QString() : params.last(); }
    bool isNumeric() const;
};

// Case-insensitive shell-style wildcard match (* and ?), as used for ignore
// lists and nick colour rules.
bool matchWildcard(const QString& text, const QString& pattern);

// Strip leading channel-status prefixes (~&@%+) from a nickname.
QString stripNickPrefix(const QString& nick);

bool isChannelName(const QString& target);

#endif
