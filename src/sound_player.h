#pragma once

// Short notification sounds. They are bundled (sounds/*.wav) and played with
// what the system already has, so no extra Qt module has to ship:
//   Windows: PlaySound from memory
//   Linux:   paplay, pw-play, aplay or play, whichever is installed
// and the system beep when nothing else is available.
namespace SoundPlayer {

enum Sound { Finished, Failed };

void play(Sound sound);

} // namespace SoundPlayer
