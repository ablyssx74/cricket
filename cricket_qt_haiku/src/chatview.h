/*
 * Cricket IRC Client - Linux port
 * Distributed under the terms of the MIT license.
 */
#ifndef CRICKET_CHATVIEW_H
#define CRICKET_CHATVIEW_H

#include <QPixmap>
#include <QTextBrowser>
#include <QTimer>
#include <QWidget>

// The widget the server list, the chat and the user list lie on. It paints the wallpaper, one picture across the
// rectangle of the canvas widget (the splitter that holds the three), dimmed by the configured level (0 = full
// picture, 100 = none); the three show it through their own backgrounds, which are made see-through while there is a picture.
class WallpaperHost : public QWidget {
    Q_OBJECT
public:
    explicit WallpaperHost(QWidget* parent = nullptr);

    void setCanvas(QWidget* canvas);
    void setWallpaper(const QString& path, int dimmingPercent);
    bool hasWallpaper() const { return !fImage.isNull(); }

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    QWidget* fCanvas = nullptr;
    QString fPath;
    QPixmap fImage;
    QPixmap fScaled;        // the picture scaled to fill the canvas, made again only when the canvas changes size
    QSize fScaledFor;
    bool fSmooth = true;
    QTimer fSmoothTimer;    // the picture is scaled smoothly when the dragging of the window's edge pauses
    int fOpacity = 30;
};

// Read-only chat log. Lines are appended as HTML paragraphs.
class ChatView : public QTextBrowser {
    Q_OBJECT
public:
    explicit ChatView(QWidget* parent = nullptr);

    void setLines(const QStringList& htmlLines);
    void appendLine(const QString& html);
    bool isDark() const;

signals:
    void searchRequested(const QString& text);
    void translateRequested(const QString& text);

protected:
    void resizeEvent(QResizeEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;

private:
    bool atBottom() const;
    void scrollToBottom();

};

#endif
