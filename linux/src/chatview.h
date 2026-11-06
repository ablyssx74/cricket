/*
 * Cricket IRC Client - Linux port
 * Distributed under the terms of the MIT license.
 */
#ifndef CRICKET_CHATVIEW_H
#define CRICKET_CHATVIEW_H

#include <QColor>
#include <QPixmap>
#include <QTextBrowser>
#include <QWidget>

// Is a wallpaper picture light, as it is seen: its average color drawn at the given opacity over the base color. It decides
// whether the text over it is dark or light, so the opacity slider changes the text color as the picture fades into the base.
// (The average of the picture is remembered per picture and file time, so asking is cheap.)
namespace WallpaperContrast {
bool isLight(const QString& path, int opacityPercent, const QColor& base);
}

// The widget the server list, the chat and the user list lie on. It paints the wallpaper, one picture across the
// rectangle of the canvas widget (the splitter that holds the three), blended over the base color at the configured
// opacity; the three show it through their own backgrounds, which are made see-through while there is a picture.
class WallpaperHost : public QWidget {
    Q_OBJECT
public:
    explicit WallpaperHost(QWidget* parent = nullptr);

    void setCanvas(QWidget* canvas);
    void setWallpaper(const QString& path, int opacityPercent);
    bool hasWallpaper() const { return !fImage.isNull(); }
    // The picture on show is light (dark text reads better over it)
    bool isLight() const;

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    QWidget* fCanvas = nullptr;
    QString fPath;
    QPixmap fImage;
    QPixmap fScaled;        // the picture scaled to fill the canvas, made again only when the canvas changes size
    QSize fScaledFor;
    int fOpacity = 30;
};

// Read-only chat log. Lines are appended as HTML paragraphs.
class ChatView : public QTextBrowser {
    Q_OBJECT
public:
    explicit ChatView(QWidget* parent = nullptr);

    void setLines(const QStringList& htmlLines);
    void appendLine(const QString& html);
    // The background the text lies on is dark: the picture's when there is one (see setBackgroundDark), else the palette's
    bool isDark() const;
    bool paletteIsDark() const;
    // -1: the palette decides; 0 or 1: the wallpaper under the text is light (0) or dark (1)
    void setBackgroundDark(int dark) { fBackgroundDark = dark; }

signals:
    void searchRequested(const QString& text);
    void translateRequested(const QString& text);

protected:
    void resizeEvent(QResizeEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;

private:
    bool atBottom() const;
    void scrollToBottom();
    int fBackgroundDark = -1;

};

#endif
