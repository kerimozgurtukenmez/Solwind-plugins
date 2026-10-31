#pragma once

#include "OrgbClient.h"
#include "plugins/KeyboardPlugin.h"

#include <QColor>
#include <QElapsedTimer>
#include <QString>

#include <atomic>

// OpenRGB/openrgb: an RGB keyboard OpenRGB can drive and the kernel doesn't expose (MSI's SteelSeries keyboards, among
// others), through OpenRGB's SDK server on this computer. Lights the kernel exposes go first: this plugin is asked last.
// If nothing answers and OpenRGB is installed, its server is started once, for this user, on 127.0.0.1 only (it goes
// when solwindd's service stops). Reading its devices takes OpenRGB some seconds: nothing waits for that here, the core
// looks again meanwhile (externalPending). On a laptop with a keyboard light in /sys this plugin isn't asked at all.
// One color on all the keys, and software brightness (OpenRGB has none for every device): the color is written
// darker; it isn't kept when solwindd restarts. Software effects stay off: they write many times a second and that
// wasn't tested. Not tested on hardware.
class OpenRgbKeyboard : public KeyboardPlugin
{
public:
    explicit OpenRgbKeyboard(quint16 port = Orgb::defaultPort) : m_port(port), m_client(port) {}

    QString id() const override { return QStringLiteral("openrgb"); }
    bool rgb() const override { return true; }
    QList<KbdLight> match(const KbdCandidates &c) const override;
    bool writable(const QList<KbdLight> &l) const override { return !l.isEmpty(); }
    AtStart atStart() const override { return AtStart::Always; }
    QString colorPath(const QString &zone) const override { return zone + QStringLiteral("/color"); }
    QString colorValue(const QColor &c, double level, const KbdFormat &f) const override;

    QList<KbdLight> externalLights() const override;
    bool externalPending() const override { return m_pending; }
    bool ownsPath(const QString &path) const override { return path.startsWith(QLatin1String(prefix)); }
    QString writeExternal(const QString &path, const QString &value) const override;

    static constexpr const char *prefix = "openrgb:";

private:
    bool reach() const;                   // connected, or connects (starting the server once)
    bool select() const;                  // finds the keyboard among OpenRGB's devices
    QString send() const;
    void waitFor(int ms) const;

    quint16 m_port;
    mutable Orgb::Client m_client;
    mutable bool m_started = false, m_modeSet = false;
    mutable QElapsedTimer m_clock;
    mutable qint64 m_retryUntil = 0;
    mutable std::atomic<bool> m_pending{ false };
    mutable int m_index = -1, m_leds = 0;
    mutable QColor m_color{ Qt::white };
    mutable QString m_name;
    mutable int m_brightness = 255;
};
