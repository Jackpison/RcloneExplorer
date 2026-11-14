#include "theme_reveal.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

// Covers the window with the old look, minus a growing circle.
class RevealOverlay : public QWidget {
public:
  RevealOverlay(QWidget *window, const QPixmap &before, const QPointF &center)
      : QWidget(window), mBefore(before), mCenter(center) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setGeometry(window->rect());
    // the circle must reach the farthest corner
    const QRectF r = rect();
    for (const QPointF &c : {r.topLeft(), r.topRight(), r.bottomLeft(), r.bottomRight()}) {
      mEnd = qMax(mEnd, QLineF(center, c).length());
    }
    window->installEventFilter(this);
  }

  void start(int ms) {
    auto *anim = new QVariantAnimation(this);
    anim->setStartValue(0.0);
    anim->setEndValue(mEnd + 2.0);
    anim->setDuration(ms);
    anim->setEasingCurve(QEasingCurve::InOutCubic);
    QObject::connect(anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
      mRadius = v.toReal();
      // only the part of the window the circle has grown over changes
      const qreal r = mRadius + 2;
      update(QRectF(mCenter.x() - r, mCenter.y() - r, 2 * r, 2 * r).toAlignedRect() & rect());
    });
    QObject::connect(anim, &QVariantAnimation::finished, this, &QObject::deleteLater);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
  }

protected:
  void paintEvent(QPaintEvent *e) override {
    // The old window with an antialiased hole, for the area being repainted:
    // drawn into a buffer first, because clearing straight on the window
    // would leave a black hole.
    const QRect area = e->rect();
    const qreal dpr = mBefore.devicePixelRatio();
    const QSize px = (QSizeF(area.size()) * dpr).toSize();
    if (mFrame.width() < px.width() || mFrame.height() < px.height()) {
      mFrame = QImage(px, QImage::Format_ARGB32_Premultiplied);
    }
    mFrame.setDevicePixelRatio(dpr);
    {
      QPainter p(&mFrame);
      p.setCompositionMode(QPainter::CompositionMode_Source);
      p.translate(-area.topLeft());
      p.drawPixmap(area, mBefore, QRectF(QPointF(area.topLeft()) * dpr, QSizeF(area.size()) * dpr));
      p.setCompositionMode(QPainter::CompositionMode_Clear);
      p.setRenderHint(QPainter::Antialiasing);
      p.setPen(Qt::NoPen);
      p.setBrush(Qt::black);
      p.drawEllipse(mCenter, mRadius, mRadius);
    }
    QPainter w(this);
    w.drawImage(area, mFrame, QRectF(QPointF(0, 0), QSizeF(area.size()) * dpr));
  }

  bool eventFilter(QObject *watched, QEvent *e) override {
    // a resized window no longer matches the snapshot: just finish
    if (watched == parent() && (e->type() == QEvent::Resize || e->type() == QEvent::Hide)) {
      hide();
      deleteLater();
    }
    return false;
  }

private:
  QPixmap mBefore;
  QImage mFrame;
  QPointF mCenter;
  qreal mRadius = 0;
  qreal mEnd = 0;
};

} // namespace

namespace ThemeReveal {

bool animationsWanted() {
#ifdef Q_OS_WIN
  // Settings, Accessibility, Visual effects, "Animation effects"
  BOOL on = TRUE;
  if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &on, 0)) {
    return on != FALSE;
  }
#endif
  return !qEnvironmentVariableIsSet("RCLONE_EXPLORER_NO_ANIMATIONS");
}

void spinIcon(QToolButton *button, int ms) {
  if (!button || !animationsWanted()) {
    return;
  }
  const QIcon target = button->icon();
  const QSize size = button->iconSize();
  const qreal dpr = button->devicePixelRatioF();
  const QPixmap still = target.pixmap(size, dpr);
  auto *anim = new QVariantAnimation(button);
  anim->setStartValue(0.0);
  anim->setEndValue(1.0);
  anim->setDuration(ms);
  anim->setEasingCurve(QEasingCurve::OutCubic);
  QObject::connect(anim, &QVariantAnimation::valueChanged, button,
                   [button, still, size, dpr](const QVariant &v) {
                     const qreal t = v.toReal();
                     QPixmap frame(size * dpr);
                     frame.setDevicePixelRatio(dpr);
                     frame.fill(Qt::transparent);
                     QPainter p(&frame);
                     p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
                     p.translate(size.width() / 2.0, size.height() / 2.0);
                     p.rotate(-90.0 * (1.0 - t));
                     const qreal s = 0.55 + 0.45 * t;
                     p.scale(s, s);
                     p.setOpacity(0.3 + 0.7 * t);
                     p.drawPixmap(QPointF(-size.width() / 2.0, -size.height() / 2.0), still);
                     p.end();
                     button->setIcon(QIcon(frame));
                   });
  QObject::connect(anim, &QVariantAnimation::finished, button,
                   [button, target]() { button->setIcon(target); });
  anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void run(QWidget *from, const std::function<void()> &change) {
  QWidget *window = from ? from->window() : nullptr;
  if (!window || !window->isVisible() || window->isMinimized() || !animationsWanted()) {
    change();
    return;
  }
  // an earlier switch still running: end it so the snapshot is current
  for (QWidget *old : window->findChildren<QWidget *>(QStringLiteral("ThemeReveal"),
                                                      Qt::FindDirectChildrenOnly)) {
    delete old;
  }
  const QPixmap before = window->grab();
  const QPointF center = QRectF(from->mapTo(window, QPoint(0, 0)), from->size()).center();
  auto *overlay = new RevealOverlay(window, before, center);
  overlay->setObjectName(QStringLiteral("ThemeReveal"));
  overlay->show();
  overlay->raise();
  change(); // restyles everything underneath; the overlay still shows the old look
  overlay->raise();
  overlay->start(400);
}

} // namespace ThemeReveal
