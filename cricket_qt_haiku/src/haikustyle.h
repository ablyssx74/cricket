/*
 * Cricket IRC Client - Haiku Qt port
 * Distributed under the terms of the MIT license.
 *
 * Qt's Haiku style draws very thin grab areas (a splitter handle about one pixel wide, a scroll bar about nine),
 * which are hard to hit with a mouse. This wraps whatever the default style is and makes them easier to grab.
 */
#ifndef CRICKET_HAIKUSTYLE_H
#define CRICKET_HAIKUSTYLE_H

#include <QProxyStyle>

class HaikuTweakStyle : public QProxyStyle {
public:
    HaikuTweakStyle() : QProxyStyle() {}

    int pixelMetric(PixelMetric metric, const QStyleOption* option = nullptr,
                    const QWidget* widget = nullptr) const override
    {
        switch (metric) {
        case PM_SplitterWidth:
            return 7;
        case PM_ScrollBarExtent:
            return qMax(QProxyStyle::pixelMetric(metric, option, widget), 15);
        case PM_ScrollBarSliderMin:
            return qMax(QProxyStyle::pixelMetric(metric, option, widget), 24);
        default:
            return QProxyStyle::pixelMetric(metric, option, widget);
        }
    }
};

#endif
