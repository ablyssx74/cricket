/*
 * Cricket IRC Client - Linux port
 * Distributed under the terms of the MIT license.
 */
#include "chatview.h"

#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QImageReader>
#include <QtMath>
#include <QMenu>
#include <QPaintEvent>
#include <QPainter>
#include <QScrollBar>

ChatView::ChatView(QWidget* parent) : QTextBrowser(parent)
{
    setOpenLinks(false);
    setOpenExternalLinks(false);
    setUndoRedoEnabled(false);
    setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse | Qt::TextSelectableByKeyboard);
    setFocusPolicy(Qt::ClickFocus);
    document()->setDocumentMargin(6);
    connect(this, &QTextBrowser::anchorClicked, this, [](const QUrl& url) { QDesktopServices::openUrl(url); });
}

bool ChatView::paletteIsDark() const
{
    return palette().color(QPalette::Base).lightness() < 128;
}

bool ChatView::isDark() const
{
    return fBackgroundDark >= 0 ? fBackgroundDark == 1 : paletteIsDark();
}

bool ChatView::atBottom() const
{
    const QScrollBar* bar = verticalScrollBar();
    return bar->value() >= bar->maximum() - 4;
}

void ChatView::scrollToBottom()
{
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

void ChatView::setLines(const QStringList& htmlLines)
{
    setHtml(htmlLines.isEmpty() ? QString() : "<p style=\"margin:0\">" + htmlLines.join("</p><p style=\"margin:0\">") + "</p>");
    scrollToBottom();
}

void ChatView::appendLine(const QString& html)
{
    bool follow = atBottom();
    int keep = verticalScrollBar()->value();
    append(html);
    if (follow)
        scrollToBottom();
    else
        verticalScrollBar()->setValue(keep);
}

void ChatView::resizeEvent(QResizeEvent* e)
{
    bool follow = atBottom();
    QTextBrowser::resizeEvent(e);
    if (follow)
        scrollToBottom();
}

namespace WallpaperContrast {

// sRGB channel (0..255) to linear light
static double linear(double v)
{
    v /= 255.0;
    return v <= 0.04045 ? v / 12.92 : qPow((v + 0.055) / 1.055, 2.4);
}

bool isLight(const QString& path, int opacityPercent, const QColor& base)
{
    if (path.isEmpty())
        return false;
    // The average color of the picture is worked out once (the key has the file's time: a picture that is written over is looked at
    // again); the blend with the base color is cheap, so the opacity slider can ask at every step.
    struct Mean { double r, g, b; bool ok; };
    static QHash<QString, Mean> cache;
    const QFileInfo info(path);
    const QString key = QString("%1|%2").arg(path).arg(info.lastModified().toMSecsSinceEpoch());
    auto found = cache.constFind(key);
    if (found == cache.constEnd()) {
        // A small copy is enough for the average (the decoder scales it down while it reads)
        QImageReader reader(path);
        const QSize full = reader.size();
        if (full.isValid())
            reader.setScaledSize(full.scaled(64, 64, Qt::KeepAspectRatio));
        const QImage image = reader.read().convertToFormat(QImage::Format_RGB32);
        Mean mean = { 0, 0, 0, false };
        if (!image.isNull()) {
            for (int y = 0; y < image.height(); ++y) {
                const QRgb* row = reinterpret_cast<const QRgb*>(image.constScanLine(y));
                for (int x = 0; x < image.width(); ++x) {
                    mean.r += qRed(row[x]);
                    mean.g += qGreen(row[x]);
                    mean.b += qBlue(row[x]);
                }
            }
            const double n = double(image.width()) * image.height();
            mean.r /= n;
            mean.g /= n;
            mean.b /= n;
            mean.ok = true;
        }
        found = cache.insert(key, mean);
    }
    if (!found->ok)
        return false;
    // the picture over the base color, as the host paints it
    const double a = qBound(0, opacityPercent, 100) / 100.0;
    const double sr = found->r * a + base.red() * (1 - a);
    const double sg = found->g * a + base.green() * (1 - a);
    const double sb = found->b * a + base.blue() * (1 - a);
    // relative luminance: above a quarter, dark text has the better contrast
    return 0.2126 * linear(sr) + 0.7152 * linear(sg) + 0.0722 * linear(sb) > 0.25;
}

}   // namespace WallpaperContrast

bool WallpaperHost::isLight() const
{
    return !fImage.isNull() && WallpaperContrast::isLight(fPath, fOpacity, palette().color(QPalette::Base));
}

WallpaperHost::WallpaperHost(QWidget* parent) : QWidget(parent)
{
}

void WallpaperHost::setCanvas(QWidget* canvas)
{
    fCanvas = canvas;
}

void WallpaperHost::setWallpaper(const QString& path, int opacityPercent)
{
    const int opacity = qBound(0, opacityPercent, 100);
    if (path == fPath && opacity == fOpacity && (fPath.isEmpty() || !fImage.isNull()))
        return;     // the same picture again (every switch of buffer asks) costs nothing
    fPath = path;
    fOpacity = opacity;
    fImage = path.isEmpty() ? QPixmap() : QPixmap(path);
    fScaled = QPixmap();
    update();
}

void WallpaperHost::paintEvent(QPaintEvent* e)
{
    QPainter p(this);
    p.fillRect(e->rect(), palette().color(QPalette::Window));
    if (fImage.isNull() || !fCanvas)
        return;
    const QRect canvas = fCanvas->geometry();
    if (canvas.isEmpty())
        return;
    if (fScaled.isNull() || fScaledFor != canvas.size()) {
        fScaled = fImage.scaled(canvas.size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        fScaledFor = canvas.size();
    }
    p.setClipRect(canvas);
    p.fillRect(canvas, palette().color(QPalette::Base));
    p.setOpacity(fOpacity / 100.0);
    p.drawPixmap(canvas.left() + (canvas.width() - fScaled.width()) / 2,
                 canvas.top() + (canvas.height() - fScaled.height()) / 2, fScaled);
}

void ChatView::contextMenuEvent(QContextMenuEvent* e)
{
    QMenu* menu = createStandardContextMenu(e->pos());
    QString selected = textCursor().selectedText().trimmed();
    QString link = anchorAt(e->pos());
    if (!link.isEmpty()) {
        menu->insertSeparator(menu->actions().value(0));
        QAction* open = new QAction(tr("Open Link in Browser"), menu);
        connect(open, &QAction::triggered, this, [link]() { QDesktopServices::openUrl(QUrl(link)); });
        menu->insertAction(menu->actions().value(0), open);
    }
    if (!selected.isEmpty()) {
        menu->addSeparator();
        QString shown = selected.length() > 30 ? selected.left(30) + "…" : selected;
        menu->addAction(tr("Search for \"%1\"").arg(shown), this, [this, selected]() { emit searchRequested(selected); });
        menu->addAction(tr("Translate \"%1\"").arg(shown), this, [this, selected]() { emit translateRequested(selected); });
    }
    menu->exec(e->globalPos());
    delete menu;
}
