// SAFETY: one color to OpenRGB's server on 127.0.0.1; OpenRGB does the USB side. Nothing in /sys.
#include "OpenRgbKeyboard.h"

#include <QProcess>
#include <QStandardPaths>

#include <algorithm>

using namespace Qt::StringLiterals;

QList<KbdLight> OpenRgbKeyboard::match(const KbdCandidates &c) const
{
    for (const KbdLight &l : c.leds) if (l.driver == u"openrgb"_s) return { l };
    return {};
}

QString OpenRgbKeyboard::colorValue(const QColor &c, double level, const KbdFormat &) const
{
    const QColor x = QColor::fromRgbF(float(c.redF() * level), float(c.greenF() * level), float(c.blueF() * level));
    return x.name().mid(1);
}

void OpenRgbKeyboard::waitFor(int ms) const
{
    if (!m_clock.isValid()) m_clock.start();
    m_retryUntil = qMax(m_retryUntil, m_clock.elapsed() + ms);
}

bool OpenRgbKeyboard::reach() const
{
    if (m_client.stillOpen() || m_client.open()) return true;
    // (not in a fake world: the tests never start a real one)
    if (m_started || qEnvironmentVariableIsSet("SOLWIND_SYS") || QStandardPaths::findExecutable(u"openrgb"_s).isEmpty()) return false;
    m_started = true;
    if (QProcess::startDetached(u"openrgb"_s, { u"--server"_s, u"--server-host"_s, u"127.0.0.1"_s, u"--noautoconnect"_s })) waitFor(30000);
    return false;
}

// Laptops first: an RGB keyboard on USB can be plugged in too
bool OpenRgbKeyboard::select() const
{
    m_client.limit(3000);
    const QList<Orgb::Controller> list = m_client.controllers();
    int best = -1;
    for (int i = 0; i < list.size(); ++i) {
        if (!list[i].isKeyboard() || list[i].leds == 0) continue;
        const bool laptop = list[i].name.contains(u"laptop"_s, Qt::CaseInsensitive) || list[i].name.contains(u"msi"_s, Qt::CaseInsensitive);
        if (laptop) { best = i; break; }
        if (best < 0) best = i;
    }
    m_client.takeListChanged();
    m_modeSet = false;
    m_index = best;
    m_leds = best < 0 ? 0 : list[best].leds;
    if (best >= 0) m_name = list[best].name;
    return best >= 0;
}

QList<KbdLight> OpenRgbKeyboard::externalLights() const
{
    const bool had = m_index >= 0;
    QList<KbdLight> out;
    if (reach() && select()) {
        KbdLight l;
        l.name = m_name;
        l.driver = u"openrgb"_s;
        l.path = QLatin1String(prefix) + QString::number(m_index);
        l.v = { { u"brightness"_s, QString::number(m_brightness) }, { u"max_brightness"_s, u"255"_s } };
        l.w = { u"brightness"_s, u"color"_s };
        out << l;
    } else {
        m_index = -1;
        if (had) waitFor(15000);          // it was there: a moment's trouble, or OpenRGB restarting
    }
    m_pending = out.isEmpty() && m_clock.isValid() && m_clock.elapsed() < m_retryUntil;
    return out;
}

QString OpenRgbKeyboard::send() const
{
    if (m_index < 0) return u"OpenRGB has no keyboard now"_s;
    const bool fresh = !m_client.stillOpen();
    if (fresh && !m_client.open()) return u"OpenRGB's server doesn't answer"_s;
    // OpenRGB restarted or its devices changed: the keyboard may have another index now
    if ((fresh || m_client.takeListChanged()) && !select()) return u"OpenRGB has no keyboard now"_s;
    if (!m_modeSet) {
        if (!m_client.setDirect(m_index)) return u"OpenRGB's server doesn't answer"_s;
        m_modeSet = true;
    }
    const double f = m_brightness / 255.0;
    const int r = qRound(m_color.red() * f), g = qRound(m_color.green() * f), b = qRound(m_color.blue() * f);
    return m_client.setColor(m_index, m_leds, quint8(r), quint8(g), quint8(b)) ? QString() : u"OpenRGB's server doesn't answer"_s;
}

QString OpenRgbKeyboard::writeExternal(const QString &path, const QString &value) const
{
    if (path.endsWith(u"/brightness"_s)) {
        bool ok = false;
        const int b = value.toInt(&ok);
        if (!ok) return u"not a brightness"_s;
        m_brightness = std::clamp(b, 0, 255);
        return send();
    }
    const QColor c = QColor::fromString(u"#"_s + value);
    if (!c.isValid()) return u"not a color"_s;
    m_color = c;
    return send();
}
