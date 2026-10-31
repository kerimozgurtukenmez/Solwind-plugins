// SAFETY: only the LED class's brightness file; its path only ever comes from detection, and the driver refuses a value
// above its maximum.
#include "MsiEcKeyboard.h"

using namespace Qt::StringLiterals;

QList<KbdLight> MsiEcKeyboard::match(const KbdCandidates &c) const
{
    for (const KbdLight &l : c.leds) if (l.name == u"msiacpi::kbd_backlight"_s) return { l };
    return {};
}

bool MsiEcKeyboard::writable(const QList<KbdLight> &l) const
{
    return kbd::every(l, [](const KbdLight &x) { return x.w.contains(u"brightness"_s); });
}
