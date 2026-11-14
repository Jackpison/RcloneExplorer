#pragma once

#include "pch.h"

// The light/dark switch as an animation: a snapshot of the window in the old
// theme covers it while the new theme is applied underneath (which hides the
// restyling pause), then a circle growing from the switch button reveals the
// new theme. Turned off when the system asks for fewer animations.
namespace ThemeReveal {

// Runs `change` (which switches the theme) behind the animation. `from` is
// the button the circle grows from. Without animations `change` simply runs.
void run(QWidget *from, const std::function<void()> &change);

// The button's new icon turns in by a quarter turn while it grows to full size.
void spinIcon(QToolButton *button, int ms = 400);

// False when the user turned animations off in the system settings.
bool animationsWanted();

} // namespace ThemeReveal
