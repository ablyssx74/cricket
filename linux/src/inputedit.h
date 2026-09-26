/*
 * Cricket IRC Client - Linux port
 * Distributed under the terms of the MIT license.
 */
#ifndef CRICKET_INPUTEDIT_H
#define CRICKET_INPUTEDIT_H

#include <QPlainTextEdit>
#include <QStringList>
#include <QSyntaxHighlighter>
#include <functional>

class SpellHighlighter : public QSyntaxHighlighter {
public:
    explicit SpellHighlighter(QTextDocument* doc) : QSyntaxHighlighter(doc) {}
    void setEnabled(bool on);

protected:
    void highlightBlock(const QString& text) override;

private:
    bool fEnabled = false;
};

// Single-line chat input with history (Up/Down), nick tab-completion and
// aspell underlining. Enter emits submitted().
class InputEdit : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit InputEdit(QWidget* parent = nullptr);

    QString text() const { return toPlainText(); }
    void setText(const QString& text);
    void setSpellCheckEnabled(bool on);
    void setCompletionSource(std::function<QStringList()> source) { fCompletionSource = std::move(source); }
    void insertAtCursor(const QString& text);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void submitted(const QString& text);

protected:
    void keyPressEvent(QKeyEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;
    void insertFromMimeData(const QMimeData* source) override;

private:
    void completeNick();
    int lineHeight() const;

    QStringList fHistory;
    int fHistoryIndex = 0;
    QString fDraft;
    std::function<QStringList()> fCompletionSource;
    SpellHighlighter* fHighlighter;
    // Tab-completion cycling state
    QString fCompletePrefix;
    QStringList fCompleteMatches;
    int fCompleteIndex = -1;
    int fCompleteStart = -1;
};

#endif
