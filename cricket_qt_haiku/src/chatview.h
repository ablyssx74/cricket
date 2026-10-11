/*
 * Cricket IRC Client - Linux port
 * Distributed under the terms of the MIT license.
 */
#ifndef CRICKET_CHATVIEW_H
#define CRICKET_CHATVIEW_H

#include <QPixmap>
#include <QTextBrowser>

// Read-only chat log. Lines are appended as HTML paragraphs; an optional
// background image is blended behind the text at the configured opacity.
class ChatView : public QTextBrowser {
    Q_OBJECT
public:
    explicit ChatView(QWidget* parent = nullptr);

    void setLines(const QStringList& htmlLines);
    void appendLine(const QString& html);
    void setBackgroundImage(const QString& path, int opacityPercent);
    bool isDark() const;

signals:
    void searchRequested(const QString& text);
    void translateRequested(const QString& text);

protected:
    void resizeEvent(QResizeEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;
    void changeEvent(QEvent* e) override;

private:
    void updateBackground();
    bool atBottom() const;
    void scrollToBottom();

    QPixmap fImage;
    int fOpacity = 30;
};

#endif
