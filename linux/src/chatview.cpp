/*
 * Cricket IRC Client - Linux port
 * Distributed under the terms of the MIT license.
 */
#include "chatview.h"

#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QMenu>
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

void ChatView::setBackgroundImage(const QString& path, int opacityPercent)
{
    fImage = path.isEmpty() ? QPixmap() : QPixmap(path);
    fOpacity = qBound(0, opacityPercent, 100);
    updateBackground();
}

void ChatView::updateBackground()
{
    QPalette pal = viewport()->palette();
    QColor base = palette().color(QPalette::Base);
    if (fImage.isNull()) {
        pal.setBrush(QPalette::Base, base);
        viewport()->setPalette(pal);
        viewport()->setAutoFillBackground(true);
        return;
    }
    QSize size = viewport()->size();
    if (size.isEmpty())
        return;
    QPixmap canvas(size);
    canvas.fill(base);
    QPainter p(&canvas);
    p.setOpacity(fOpacity / 100.0);
    QPixmap scaled = fImage.scaled(size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    p.drawPixmap((size.width() - scaled.width()) / 2, (size.height() - scaled.height()) / 2, scaled);
    p.end();
    pal.setBrush(QPalette::Base, QBrush(canvas));
    viewport()->setPalette(pal);
    viewport()->setAutoFillBackground(true);
}

void ChatView::resizeEvent(QResizeEvent* e)
{
    bool follow = atBottom();
    QTextBrowser::resizeEvent(e);
    if (!fImage.isNull())
        updateBackground();
    if (follow)
        scrollToBottom();
}

void ChatView::changeEvent(QEvent* e)
{
    QTextBrowser::changeEvent(e);
    if (e->type() == QEvent::PaletteChange || e->type() == QEvent::StyleChange)
        updateBackground();
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
