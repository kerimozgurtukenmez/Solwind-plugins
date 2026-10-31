#pragma once

#include "plugins/KeyboardPlugin.h"

// MSI/msi-ec-keyboard: the single-color keyboard light of MSI laptops as the msi-ec module (BeardOverflow/msi-ec, DKMS;
// not the part of it in the official kernel) exposes it on the firmwares it knows: msiacpi::kbd_backlight, an LED class
// light with brightness 0-3. RGB keyboards are on USB, not here. Not tested on hardware.
class MsiEcKeyboard : public KeyboardPlugin
{
public:
    QString id() const override { return QStringLiteral("led-mono"); }
    bool rgb() const override { return false; }
    QList<KbdLight> match(const KbdCandidates &c) const override;
    bool writable(const QList<KbdLight> &l) const override;
};
