/*
 * Cricket IRC Client - Linux port
 * Distributed under the terms of the MIT license.
 */
#include "chatview.h"

#include <QContextMenuEvent>
#include <QDesktopServices>
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

bool ChatView::isDark() const
{
    return palette().color(QPalette::Base).lightness() < 128;
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

WallpaperHost::WallpaperHost(QWidget* parent) : QWidget(parent)
{
    fSmoothTimer.setSingleShot(true);
    fSmoothTimer.setInterval(80);
    connect(&fSmoothTimer, &QTimer::timeout, this, [this]() {
        if (fSmooth || fImage.isNull() || !fCanvas || fCanvas->geometry().isEmpty())
            return;
        fScaled = QPixmap();    // the next paint scales it smoothly
        update();
    });
}

void WallpaperHost::setCanvas(QWidget* canvas)
{
    fCanvas = canvas;
}

void WallpaperHost::setWallpaper(const QString& path, int opacityPercent)
{
    const int dimming = qBound(0, opacityPercent, 100);
    if (path == fPath && dimming == fOpacity && (fPath.isEmpty() || !fImage.isNull()))
        return;     // the same picture again (every switch of buffer asks) costs nothing
    fPath = path;
    fOpacity = dimming;
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
        // (while the window is resized the picture is scaled quickly at every step, and smoothly once when the dragging pauses)
        fSmooth = fScaled.isNull();
        fScaled = fImage.scaled(canvas.size(), Qt::KeepAspectRatioByExpanding,
            fSmooth ? Qt::SmoothTransformation : Qt::FastTransformation);
        fScaledFor = canvas.size();
        if (!fSmooth)
            fSmoothTimer.start();
    }
    p.setClipRect(canvas);
    p.fillRect(canvas, palette().color(QPalette::Base));
    // The setting is the dimming level, as in the native Cricket: higher = the picture fainter
    p.setOpacity(1.0 - fOpacity / 100.0);
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
