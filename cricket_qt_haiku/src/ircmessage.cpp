/*
 * Cricket IRC Client - Linux port
 * Distributed under the terms of the MIT license.
 */
#include "ircmessage.h"

#include <QRegularExpression>

static QString unescapeTag(const QString& value)
{
    QString out;
    out.reserve(value.size());
    for (int i = 0; i < value.size(); ++i) {
        QChar c = value.at(i);
        if (c == '\\' && i + 1 < value.size()) {
            QChar n = value.at(++i);
            if (n == ':') out += ';';
            else if (n == 's') out += ' ';
            else if (n == 'r') out += '\r';
            else if (n == 'n') out += '\n';
            else out += n;
        } else {
            out += c;
        }
    }
    return out;
}

IrcMessage IrcMessage::parse(const QString& input)
{
    IrcMessage msg;
    msg.raw = input;
    QString line = input;
    while (line.endsWith('\r') || line.endsWith('\n'))
        line.chop(1);

    int pos = 0;
    auto skipSpaces = [&]() {
        while (pos < line.size() && line.at(pos) == ' ')
            ++pos;
    };

    if (line.startsWith('@')) {
        int end = line.indexOf(' ');
        if (end < 0)
            return msg;
        const QString tagBlock = line.mid(1, end - 1);
        for (const QString& tag : tagBlock.split(';', Qt::SkipEmptyParts)) {
            int eq = tag.indexOf('=');
            if (eq < 0)
                msg.tags.insert(tag, QString());
            else
                msg.tags.insert(tag.left(eq), unescapeTag(tag.mid(eq + 1)));
        }
        pos = end + 1;
        skipSpaces();
    }

    if (pos < line.size() && line.at(pos) == ':') {
        int end = line.indexOf(' ', pos);
        if (end < 0) {
            msg.prefix = line.mid(pos + 1);
            return msg;
        }
        msg.prefix = line.mid(pos + 1, end - pos - 1);
        pos = end + 1;
        skipSpaces();
    }

    int end = line.indexOf(' ', pos);
    if (end < 0) {
        msg.command = line.mid(pos).toUpper();
        return msg;
    }
    msg.command = line.mid(pos, end - pos).toUpper();
    pos = end + 1;

    while (pos < line.size()) {
        skipSpaces();
        if (pos >= line.size())
            break;
        if (line.at(pos) == ':') {
            msg.params << line.mid(pos + 1);
            msg.hasTrailing = true;
            break;
        }
        end = line.indexOf(' ', pos);
        if (end < 0) {
            msg.params << line.mid(pos);
            break;
        }
        msg.params << line.mid(pos, end - pos);
        pos = end + 1;
    }
    return msg;
}

QString IrcMessage::nick() const
{
    int bang = prefix.indexOf('!');
    return bang >= 0 ? prefix.left(bang) : prefix;
}

QString IrcMessage::userHost() const
{
    int bang = prefix.indexOf('!');
    return bang >= 0 ? prefix.mid(bang + 1) : QString();
}

bool IrcMessage::isNumeric() const
{
    if (command.size() != 3)
        return false;
    for (QChar c : command)
        if (!c.isDigit())
            return false;
    return true;
}

bool matchWildcard(const QString& text, const QString& pattern)
{
    if (pattern.isEmpty())
        return false;
    const QString s = text.toLower();
    const QString p = pattern.toLower();
    int si = 0, pi = 0, starP = -1, starS = -1;
    while (si < s.size()) {
        if (pi < p.size() && (p.at(pi) == '?' || p.at(pi) == s.at(si))) {
            ++si;
            ++pi;
        } else if (pi < p.size() && p.at(pi) == '*') {
            starP = pi++;
            starS = si;
        } else if (starP >= 0) {
            pi = starP + 1;
            si = ++starS;
        } else {
            return false;
        }
    }
    while (pi < p.size() && p.at(pi) == '*')
        ++pi;
    return pi == p.size();
}

QString stripNickPrefix(const QString& nick)
{
    int i = 0;
    while (i < nick.size() && QString("~&@%+").contains(nick.at(i)))
        ++i;
    return nick.mid(i);
}

bool isChannelName(const QString& target)
{
    return !target.isEmpty() && QString("#&!+").contains(target.at(0));
}
