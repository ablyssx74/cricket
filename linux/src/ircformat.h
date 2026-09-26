/*
 * Cricket IRC Client - Linux port
 * Distributed under the terms of the MIT license.
 */
#ifndef CRICKET_IRCFORMAT_H
#define CRICKET_IRCFORMAT_H

#include <QColor>
#include <QString>
#include <QVector>

namespace IrcFormat {

// The classic mIRC 16-colour palette (plus the extended 16-98 range).
QColor paletteColor(int index);

// Convert a message body containing mIRC control codes to HTML.
// colors=false strips formatting instead of rendering it.
QString toHtml(const QString& text, bool colors, bool emoticons);

// Remove all mIRC formatting codes.
QString stripCodes(const QString& text);

// Stable colour for a nick, picked from a readable subset of the palette.
QColor nickColor(const QString& nick, bool darkBackground);

QString escape(const QString& text);

struct Emote {
    const char* trigger;
    const char* emoji;
    const char* name;
};
const QVector<Emote>& emotes();
// One representative emote per face, for the picker.
QVector<Emote> pickerEmotes();

}

#endif
