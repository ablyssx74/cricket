/*
 * Cricket IRC Client - Haiku Qt port
 * Distributed under the terms of the MIT license.
 *
 * On Haiku, Qt's platform plugin can report mouse moves with no buttons held while a button is down (seen with the
 * pointer in a VM). Qt then thinks every drag stopped at once, so splitter handles and scroll bar thumbs can be
 * pressed but not dragged. This remembers which buttons were pressed and puts them back on moves that arrive
 * without any, until the button is released.
 */
#ifndef CRICKET_HAIKUMOUSEFIX_H
#define CRICKET_HAIKUMOUSEFIX_H

#include <QCoreApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QObject>
#include <QWindow>

class HaikuMouseFix : public QObject {
public:
    explicit HaikuMouseFix(QObject* parent = nullptr) : QObject(parent) {}

    bool eventFilter(QObject* obj, QEvent* event) override
    {
        switch (event->type()) {
        case QEvent::MouseButtonPress:
            fHeld = static_cast<QMouseEvent*>(event)->buttons();
            break;
        case QEvent::MouseButtonRelease:
            fHeld = static_cast<QMouseEvent*>(event)->buttons();
            break;
        case QEvent::ApplicationDeactivate:
            fHeld = Qt::NoButton;
            break;
        case QEvent::MouseMove: {
            auto* m = static_cast<QMouseEvent*>(event);
            // only fix the event as it reaches the window; the widgets below get the corrected copy
            if (fHeld != Qt::NoButton && m->buttons() == Qt::NoButton && qobject_cast<QWindow*>(obj)) {
                QMouseEvent fixed(QEvent::MouseMove, m->position(), m->scenePosition(), m->globalPosition(),
                                  Qt::NoButton, fHeld, m->modifiers(), m->pointingDevice());
                QCoreApplication::sendEvent(obj, &fixed);
                return true;
            }
            break;
        }
        default:
            break;
        }
        return false;
    }

private:
    Qt::MouseButtons fHeld = Qt::NoButton;
};

#endif
