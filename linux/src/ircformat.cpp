/*
 * Cricket IRC Client - Linux port
 * Distributed under the terms of the MIT license.
 */
#include "ircformat.h"

#include <QHash>
#include <QRegularExpression>

#include <cctype>

namespace IrcFormat {

static const QRgb kBasePalette[16] = {
    0xFFFFFF, 0x000000, 0x00007F, 0x009300, 0xFF0000, 0x7F0000, 0x9C009C, 0xFC7F00,
    0xFFFF00, 0x00FC00, 0x009393, 0x00FFFF, 0x0000FC, 0xFF00FF, 0x7F7F7F, 0xD2D2D2,
};

// Extended colours 16-98 as defined by the modern IRC formatting spec.
static const QRgb kExtPalette[83] = {
    0x470000, 0x472100, 0x474700, 0x324700, 0x004700, 0x00472c, 0x004747, 0x002747, 0x000047, 0x2e0047, 0x470047, 0x47002a,
    0x740000, 0x743a00, 0x747400, 0x517400, 0x007400, 0x007449, 0x007474, 0x004074, 0x000074, 0x4b0074, 0x740074, 0x740045,
    0xb50000, 0xb56300, 0xb5b500, 0x7db500, 0x00b500, 0x00b571, 0x00b5b5, 0x0063b5, 0x0000b5, 0x7500b5, 0xb500b5, 0xb5006b,
    0xff0000, 0xff8c00, 0xffff00, 0xb2ff00, 0x00ff00, 0x00ffa0, 0x00ffff, 0x008cff, 0x0000ff, 0xa500ff, 0xff00ff, 0xff0098,
    0xff5959, 0xffb459, 0xffff71, 0xcfff60, 0x6fff6f, 0x65ffc9, 0x6dffff, 0x59b4ff, 0x5959ff, 0xc459ff, 0xff66ff, 0xff59bc,
    0xff9c9c, 0xffd39c, 0xffff9c, 0xe2ff9c, 0x9cff9c, 0x9cffdb, 0x9cffff, 0x9cd3ff, 0x9c9cff, 0xdc9cff, 0xff9cff, 0xff94d3,
    0x000000, 0x131313, 0x282828, 0x363636, 0x4d4d4d, 0x656565, 0x818181, 0x9f9f9f, 0xbcbcbc, 0xe2e2e2, 0xffffff,
};

QColor paletteColor(int index)
{
    if (index >= 0 && index < 16)
        return QColor(kBasePalette[index]);
    if (index >= 16 && index < 99)
        return QColor(kExtPalette[index - 16]);
    return QColor();
}

QString escape(const QString& text)
{
    return text.toHtmlEscaped();
}

const QVector<Emote>& emotes()
{
    static const QVector<Emote> list = {
        {":)", "🙂", "Smile"}, {":-)", "🙂", "Smile"},
        {"^^", "😊", "Cheerful"}, {"^_^", "😊", "Cheerful"},
        {":D", "😄", "Laughing"}, {":-D", "😄", "Laughing"}, {":d", "😄", "Laughing"},
        {"lol", "😆", "Laughing"}, {"LOL", "😆", "Laughing"},
        {":|", "😕", "Confused"}, {":-|", "😕", "Confused"}, {":/", "😕", "Confused"},
        {":-\\", "😕", "Confused"}, {"o_O", "😕", "Confused"}, {"O_o", "😕", "Confused"},
        {":(", "🙁", "Frown"}, {":-(", "🙁", "Frown"}, {":X", "🙁", "Frown"}, {":x", "🙁", "Frown"},
        {">_<", "😣", "Annoyed"}, {">_(", "😣", "Annoyed"},
        {";'(", "😢", "Crying"}, {";-'(", "😢", "Crying"}, {"T_T", "😢", "Crying"}, {";_;", "😢", "Crying"},
        {";)", "😉", "Wink"}, {";-)", "😉", "Wink"},
        {":P", "😛", "Tongue"}, {":p", "😛", "Tongue"}, {":-P", "😛", "Tongue"}, {":-p", "😛", "Tongue"},
        {":o", "😮", "Astonished"}, {":O", "😮", "Astonished"}, {":-o", "😮", "Astonished"},
        {":-O", "😮", "Astonished"}, {":0", "😮", "Astonished"}, {":-0", "😮", "Astonished"},
        {"O_O", "😳", "Wide-eyed"}, {"o_o", "😳", "Wide-eyed"},
        {"8-)", "😎", "Sunglasses"}, {"B)", "😎", "Sunglasses"}, {"8)", "😎", "Sunglasses"}, {"b)", "😎", "Sunglasses"},
        {"<3", "❤️", "Heart"}, {":*", "😘", "Kiss"}, {":-*", "😘", "Kiss"},
    };
    return list;
}

QVector<Emote> pickerEmotes()
{
    QVector<Emote> out;
    QStringList seen;
    for (const Emote& e : emotes()) {
        if (seen.contains(e.name))
            continue;
        seen << e.name;
        out << e;
    }
    return out;
}

static QString emoteFor(const QString& token)
{
    static QHash<QString, QString> map;
    if (map.isEmpty())
        for (const Emote& e : emotes())
            map.insert(QString::fromUtf8(e.trigger), QString::fromUtf8(e.emoji));
    return map.value(token);
}

// Escape a plain-text run, turning URLs into links and emoticons into emoji.
static QString renderRun(const QString& text, bool emoticons)
{
    static const QRegularExpression urlRe(
        R"((https?://|ftp://|www\.)[^\s<>"'\x00-\x1f]+)",
        QRegularExpression::CaseInsensitiveOption);

    QString out;
    int pos = 0;
    auto plain = [&](const QString& segment) {
        if (!emoticons) {
            out += escape(segment);
            return;
        }
        // Only whole whitespace-delimited tokens are replaced, so things like
        // "http://" or "a:)b" are left alone.
        int i = 0;
        while (i < segment.size()) {
            int j = i;
            while (j < segment.size() && !segment.at(j).isSpace())
                ++j;
            QString token = segment.mid(i, j - i);
            QString emoji = token.isEmpty() ? QString() : emoteFor(token);
            out += emoji.isEmpty() ? escape(token) : QString("<span title=\"%1\">%2</span>").arg(escape(token), emoji);
            int k = j;
            while (k < segment.size() && segment.at(k).isSpace())
                ++k;
            out += escape(segment.mid(j, k - j));
            i = k;
        }
    };

    auto it = urlRe.globalMatch(text);
    while (it.hasNext()) {
        auto m = it.next();
        plain(text.mid(pos, m.capturedStart() - pos));
        QString url = m.captured(0);
        // Trailing punctuation usually belongs to the sentence, not the link.
        QString tail;
        while (!url.isEmpty() && QString(".,;:!?)]}").contains(url.back())) {
            if (url.back() == ')' && url.count('(') >= url.count(')'))
                break;
            tail.prepend(url.back());
            url.chop(1);
        }
        QString href = url.startsWith("www.", Qt::CaseInsensitive) ? "https://" + url : url;
        out += QString("<a href=\"%1\">%2</a>").arg(escape(href), escape(url));
        out += escape(tail);
        pos = m.capturedEnd();
    }
    plain(text.mid(pos));
    return out;
}

// True when six hex digits start at text[pos].
static bool isHexRun(const QString& text, int pos)
{
    if (pos + 6 > text.size())
        return false;
    for (int k = pos; k < pos + 6; ++k)
        if (!isxdigit(text.at(k).toLatin1()))
            return false;
    return true;
}

struct Style {
    bool bold = false, italic = false, underline = false, strike = false, mono = false, reverse = false;
    QColor fg, bg;
    bool isPlain() const { return !bold && !italic && !underline && !strike && !mono && !reverse && !fg.isValid() && !bg.isValid(); }
};

static QString openSpan(const Style& s)
{
    if (s.isPlain())
        return QString();
    QStringList css;
    QColor fg = s.fg, bg = s.bg;
    if (s.reverse) {
        std::swap(fg, bg);
        if (!fg.isValid()) fg = QColor(Qt::white);
        if (!bg.isValid()) bg = QColor(Qt::black);
    }
    if (s.bold) css << "font-weight:bold";
    if (s.italic) css << "font-style:italic";
    QStringList deco;
    if (s.underline) deco << "underline";
    if (s.strike) deco << "line-through";
    if (!deco.isEmpty()) css << "text-decoration:" + deco.join(' ');
    if (s.mono) css << "font-family:monospace";
    if (fg.isValid()) css << "color:" + fg.name();
    if (bg.isValid()) css << "background-color:" + bg.name();
    return "<span style=\"" + css.join(';') + "\">";
}

QString toHtml(const QString& text, bool colors, bool emoticons)
{
    QString out;
    QString run;
    Style style;

    auto flush = [&]() {
        if (run.isEmpty())
            return;
        if (colors && !style.isPlain()) {
            out += openSpan(style);
            out += renderRun(run, emoticons);
            out += "</span>";
        } else {
            out += renderRun(run, emoticons);
        }
        run.clear();
    };

    const int n = text.size();
    for (int i = 0; i < n; ++i) {
        const ushort c = text.at(i).unicode();
        switch (c) {
        case 0x02: flush(); style.bold = !style.bold; break;
        case 0x1D: flush(); style.italic = !style.italic; break;
        case 0x1F: flush(); style.underline = !style.underline; break;
        case 0x1E: flush(); style.strike = !style.strike; break;
        case 0x11: flush(); style.mono = !style.mono; break;
        case 0x16: flush(); style.reverse = !style.reverse; break;
        case 0x0F: flush(); style = Style(); break;
        case 0x03: {
            flush();
            auto readNum = [&](int& idx) -> int {
                if (idx + 1 < n && text.at(idx + 1).isDigit()) {
                    int v = text.at(++idx).digitValue();
                    if (idx + 1 < n && text.at(idx + 1).isDigit())
                        v = v * 10 + text.at(++idx).digitValue();
                    return v;
                }
                return -1;
            };
            int fg = readNum(i);
            if (fg < 0) {
                style.fg = QColor();
                style.bg = QColor();
                break;
            }
            style.fg = paletteColor(fg);
            if (i + 2 < n && text.at(i + 1) == ',' && text.at(i + 2).isDigit()) {
                ++i;
                int bg = readNum(i);
                style.bg = paletteColor(bg);
            }
            break;
        }
        case 0x04: { // hex colour: \x04RRGGBB[,RRGGBB]
            flush();
            auto readHex = [&](int& idx) -> QColor {
                if (isHexRun(text, idx + 1)) {
                    QString hex = text.mid(idx + 1, 6);
                    idx += 6;
                    return QColor("#" + hex);
                }
                return QColor();
            };
            QColor fg = readHex(i);
            if (!fg.isValid()) {
                style.fg = QColor();
                style.bg = QColor();
                break;
            }
            style.fg = fg;
            if (i + 1 < n && text.at(i + 1) == ',') {
                int save = i;
                ++i;
                QColor bg = readHex(i);
                if (bg.isValid())
                    style.bg = bg;
                else
                    i = save;
            }
            break;
        }
        default:
            if (c < 0x20 && c != '\t')
                break; // drop other control characters
            run += text.at(i);
        }
    }
    flush();
    return out;
}

QString stripCodes(const QString& text)
{
    QString out;
    const int n = text.size();
    for (int i = 0; i < n; ++i) {
        ushort c = text.at(i).unicode();
        if (c == 0x03) {
            int digits = 0;
            while (digits < 2 && i + 1 < n && text.at(i + 1).isDigit()) { ++i; ++digits; }
            if (digits && i + 2 < n && text.at(i + 1) == ',' && text.at(i + 2).isDigit()) {
                ++i;
                digits = 0;
                while (digits < 2 && i + 1 < n && text.at(i + 1).isDigit()) { ++i; ++digits; }
            }
            continue;
        }
        if (c == 0x04) {
            if (isHexRun(text, i + 1)) {
                i += 6;
                if (i + 1 < n && text.at(i + 1) == ',' && isHexRun(text, i + 2))
                    i += 7;
            }
            continue;
        }
        if (c < 0x20 && c != '\t')
            continue;
        out += text.at(i);
    }
    return out;
}

QColor nickColor(const QString& nick, bool darkBackground)
{
    static const int lightBgIdx[] = {2, 3, 4, 5, 6, 7, 10, 12, 13, 14};
    static const int darkBgIdx[] = {3, 4, 7, 8, 9, 10, 11, 12, 13, 15};
    uint h = 0;
    for (QChar c : nick.toLower())
        h = h * 31 + c.unicode();
    const int* table = darkBackground ? darkBgIdx : lightBgIdx;
    QColor col = paletteColor(table[h % 10]);
    if (darkBackground && col.lightness() < 110)
        col = col.lighter(150);
    return col;
}

}
