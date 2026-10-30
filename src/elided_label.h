#pragma once

#include <QLabel>
#include <QPainter>
#include <QStyle>

// A label that shows "start…end" when its text does not fit, instead of
// cutting it off, and never asks for more room than it is given.
class ElidedLabel : public QLabel {
  Q_OBJECT

public:
  using QLabel::QLabel;

  // Show the full text as the tooltip while it is shortened.
  void setAutoToolTip(bool on) { mAutoToolTip = on; }

  QSize minimumSizeHint() const override {
    return QSize(0, QLabel::minimumSizeHint().height());
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    const QRect r = contentsRect();
    const QString shown =
        fontMetrics().elidedText(text(), Qt::ElideMiddle, r.width());
    style()->drawItemText(&p, r, alignment(), palette(), isEnabled(), shown,
                          foregroundRole());
    if (mAutoToolTip) {
      const QString tip = shown != text() ? text() : QString();
      if (tip != toolTip()) {
        setToolTip(tip);
      }
    }
  }

private:
  bool mAutoToolTip = false;
};
