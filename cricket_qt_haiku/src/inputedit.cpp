/*
 * Cricket IRC Client - Linux port
 * Distributed under the terms of the MIT license.
 */
#include "inputedit.h"

#include "services.h"

#include <QAbstractTextDocumentLayout>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QRegularExpression>
#include <QTextBlock>

static const QRegularExpression& wordRe()
{
    static const QRegularExpression re(R"([\p{L}']+)");
    return re;
}

void SpellHighlighter::setEnabled(bool on)
{
    if (fEnabled == on)
        return;
    fEnabled = on;
    rehighlight();
}

void SpellHighlighter::highlightBlock(const QString& text)
{
    if (!fEnabled || !SpellChecker::instance().isAvailable() || text.startsWith('/'))
        return;
    QTextCharFormat fmt;
    fmt.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
    fmt.setUnderlineColor(Qt::red);
    auto it = wordRe().globalMatch(text);
    while (it.hasNext()) {
        auto m = it.next();
        QString word = m.captured(0);
        // Skip words still being typed at the very end of the line.
        if (m.capturedEnd() == text.size())
            continue;
        if (word.size() > 1 && !SpellChecker::instance().check(word))
            setFormat(m.capturedStart(), m.capturedLength(), fmt);
    }
}

InputEdit::InputEdit(QWidget* parent) : QPlainTextEdit(parent)
{
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setTabChangesFocus(false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setPlaceholderText(tr("Type a message or /command…"));
    fHighlighter = new SpellHighlighter(document());
    setFixedHeight(sizeHint().height());
}

int InputEdit::lineHeight() const
{
    return fontMetrics().lineSpacing();
}

QSize InputEdit::sizeHint() const
{
    int h = lineHeight() + int(document()->documentMargin() * 2) + frameWidth() * 2 + 4;
    return QSize(400, h);
}

QSize InputEdit::minimumSizeHint() const
{
    return QSize(100, sizeHint().height());
}

void InputEdit::setText(const QString& text)
{
    setPlainText(text);
    moveCursor(QTextCursor::End);
}

void InputEdit::insertAtCursor(const QString& text)
{
    textCursor().insertText(text);
    setFocus();
}

void InputEdit::setSpellCheckEnabled(bool on)
{
    fHighlighter->setEnabled(on);
}

void InputEdit::insertFromMimeData(const QMimeData* source)
{
    if (!source->hasText())
        return;
    QString text = source->text();
    text.replace("\r\n", "\n");
    if (!text.contains('\n')) {
        textCursor().insertText(text);
        return;
    }
    // Multi-line paste: keep the current line's text in front of the first
    // pasted line and hand the whole block to the window, which confirms
    // before sending several lines.
    QString combined = toPlainText().left(textCursor().position()) + text;
    while (combined.endsWith('\n'))
        combined.chop(1);
    clear();
    emit submitted(combined);
}

void InputEdit::keyPressEvent(QKeyEvent* e)
{
    if (e->key() != Qt::Key_Tab)
        fCompleteIndex = -1;

    // Leave modified keys (Alt+Up/Down buffer switching, etc.) to the window.
    if (e->modifiers() & (Qt::AltModifier | Qt::ControlModifier | Qt::MetaModifier)) {
        if (e->key() == Qt::Key_Up || e->key() == Qt::Key_Down || e->key() == Qt::Key_Tab) {
            e->ignore();
            return;
        }
    }

    switch (e->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter: {
        QString t = toPlainText();
        if (!t.isEmpty()) {
            if (fHistory.isEmpty() || fHistory.last() != t)
                fHistory << t;
            if (fHistory.size() > 200)
                fHistory.removeFirst();
        }
        fHistoryIndex = fHistory.size();
        fDraft.clear();
        clear();
        emit submitted(t);
        return;
    }
    case Qt::Key_Up:
        if (fHistory.isEmpty())
            return;
        if (fHistoryIndex == fHistory.size())
            fDraft = toPlainText();
        if (fHistoryIndex > 0)
            --fHistoryIndex;
        setText(fHistory.value(fHistoryIndex));
        return;
    case Qt::Key_Down:
        if (fHistoryIndex < fHistory.size())
            ++fHistoryIndex;
        setText(fHistoryIndex == fHistory.size() ? fDraft : fHistory.value(fHistoryIndex));
        return;
    case Qt::Key_Tab:
        completeNick();
        return;
    default:
        break;
    }
    QPlainTextEdit::keyPressEvent(e);
}

void InputEdit::completeNick()
{
    if (!fCompletionSource)
        return;
    QString t = toPlainText();
    int cursor = textCursor().position();

    if (fCompleteIndex < 0) {
        int start = cursor;
        while (start > 0 && !t.at(start - 1).isSpace())
            --start;
        fCompletePrefix = t.mid(start, cursor - start);
        if (fCompletePrefix.isEmpty())
            return;
        fCompleteStart = start;
        fCompleteMatches.clear();
        for (const QString& n : fCompletionSource())
            if (n.startsWith(fCompletePrefix, Qt::CaseInsensitive))
                fCompleteMatches << n;
        if (fCompleteMatches.isEmpty())
            return;
        std::sort(fCompleteMatches.begin(), fCompleteMatches.end(),
            [](const QString& a, const QString& b) { return a.compare(b, Qt::CaseInsensitive) < 0; });
        fCompleteIndex = 0;
    } else {
        fCompleteIndex = (fCompleteIndex + 1) % fCompleteMatches.size();
    }

    QString suffix = (fCompleteStart == 0) ? QString(": ") : QString(" ");
    QString replacement = fCompleteMatches.at(fCompleteIndex) + suffix;
    QTextCursor c = textCursor();
    c.setPosition(fCompleteStart);
    c.setPosition(cursor, QTextCursor::KeepAnchor);
    c.insertText(replacement);
    setTextCursor(c);
}

void InputEdit::contextMenuEvent(QContextMenuEvent* e)
{
    QMenu* menu = createStandardContextMenu();
    SpellChecker& sp = SpellChecker::instance();
    if (sp.isAvailable()) {
        QTextCursor c = cursorForPosition(e->pos());
        const QString line = toPlainText();
        int pos = c.position();
        auto it = wordRe().globalMatch(line);
        while (it.hasNext()) {
            auto m = it.next();
            if (pos < m.capturedStart() || pos > m.capturedEnd())
                continue;
            QString word = m.captured(0);
            if (sp.check(word))
                break;
            int start = m.capturedStart(), len = m.capturedLength();
            QAction* first = menu->actions().value(0);
            QStringList suggestions = sp.suggest(word);
            if (suggestions.isEmpty()) {
                QAction* none = new QAction(tr("(no suggestions)"), menu);
                none->setEnabled(false);
                menu->insertAction(first, none);
            }
            for (const QString& s : suggestions) {
                QAction* a = new QAction(s, menu);
                QFont f = a->font();
                f.setBold(true);
                a->setFont(f);
                connect(a, &QAction::triggered, this, [this, start, len, s]() {
                    QTextCursor tc = textCursor();
                    tc.setPosition(start);
                    tc.setPosition(start + len, QTextCursor::KeepAnchor);
                    tc.insertText(s);
                });
                menu->insertAction(first, a);
            }
            QAction* add = new QAction(tr("Add \"%1\" to dictionary").arg(word), menu);
            connect(add, &QAction::triggered, this, [this, word]() {
                SpellChecker::instance().addToPersonal(word);
                fHighlighter->rehighlight();
            });
            menu->insertAction(first, add);
            menu->insertSeparator(first);
            break;
        }
    }
    menu->exec(e->globalPos());
    delete menu;
}
