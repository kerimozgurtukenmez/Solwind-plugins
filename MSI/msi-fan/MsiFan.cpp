// SAFETY: only pwm*_enable, pwm*_auto_point*_pwm of the msi_wmi_platform hwmon and cooler_boost of msi-ec, found under
// /sys here; speeds are kept to FloorPwm..255. No embedded controller access, no fan_mode.
#include "MsiFan.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;

namespace {
QString readText(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.read(256)).trimmed() : QString();
}
QString pointFile(const QString &dir, int fan, int point)
{
    return dir + u"/pwm%1_auto_point%2_pwm"_s.arg(fan + 1).arg(point);
}
QString enableFile(const QString &dir, int fan) { return dir + u"/pwm%1_enable"_s.arg(fan + 1); }
}

MsiFan::Found MsiFan::find(const Paths &paths)
{
    Found out;
    const QDir d(paths.sys(u"/class/hwmon"_s));
    for (const QString &e : d.entryList({ u"hwmon*"_s }, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        const QString h = d.filePath(e);
        if (readText(h + u"/name"_s) != u"msi_wmi_platform"_s) continue;
        if (QFileInfo::exists(enableFile(h, 0)) && QFileInfo::exists(enableFile(h, 1))
            && QFileInfo::exists(pointFile(h, 0, 1)) && QFileInfo::exists(pointFile(h, 1, 1)))
            out.curves = h;
    }
    const QString ec = paths.sys(u"/devices/platform/msi-ec"_s);
    if (QFileInfo::exists(ec + u"/cooler_boost"_s)) out.ec = ec;
    return out;
}

int MsiFan::pwmOf(int percent)
{
    return std::clamp(int(std::lround(percent * 2.55)), int(FloorPwm), 255);
}

MsiFan::MsiFan(const PluginHost &host, QObject *parent) : FanPlugin(parent), m_host(host), m_paths(host.paths())
{
    m_found = find(m_paths);
    const QString b = readText(m_paths.sys(u"/class/dmi/id/board_name"_s));
    m_config = b.isEmpty() ? u"MSI"_s : u"MSI "_s + b;
    if (m_found.curves.isEmpty()) m_config += u" (msi-ec)"_s;
    if (!m_found.curves.isEmpty())
        while (m_points < 16 && QFileInfo::exists(pointFile(m_found.curves, 0, m_points + 1))
               && QFileInfo::exists(pointFile(m_found.curves, 1, m_points + 1))) ++m_points;
    if (m_found.ec.isEmpty()) {
        const QDir d(m_paths.sys(u"/class/hwmon"_s));
        for (const QString &e : d.entryList({ u"hwmon*"_s }, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
            const QString n = readText(d.filePath(e) + u"/name"_s);
            if (n == u"coretemp"_s || n == u"k10temp"_s) { m_tempFile = d.filePath(e) + u"/temp1_input"_s; break; }
        }
    }
    // Left by a solwindd that died without its hand-back: nothing watches these fans now. Here and not in start(): the
    // factory curves belong in the hand-back command, which is recorded before start() (the driver of a kernel that
    // restores them does it now; otherwise what's left of our curve is read, as nothing else tells the factory's).
    m_nbfc = nbfcRuns();
    m_writable = canWriteFiles();
    if (m_writable && !m_nbfc && !m_found.curves.isEmpty() && readInt(enableFile(m_found.curves, 0)) == 1) {
        QString error;
        for (int fan = 0; fan < 2; ++fan) put(enableFile(m_found.curves, fan), "2", &error);
    }
    if (readInt(enableFile(m_found.curves, 0)) != 1) readFactory();
    m_fans = { Fan{ u"CPU Fan"_s }, Fan{ u"GPU Fan"_s } };
    m_timer.setInterval(5000);
    connect(&m_timer, &QTimer::timeout, this, &MsiFan::poll);
}

void MsiFan::readFactory()
{
    if (m_found.curves.isEmpty() || m_points == 0) return;
    for (int fan = 0; fan < 2; ++fan) {
        QList<int> table;
        for (int p = 1; p <= m_points; ++p) {
            const int v = readInt(pointFile(m_found.curves, fan, p));
            if (v < 0 || v > 255) { table.clear(); break; }
            table << v;
        }
        m_factory[fan] = table;
    }
}

bool MsiFan::canWriteFiles() const
{
    if (!m_found.curves.isEmpty()) {
        for (int fan = 0; fan < 2; ++fan) {
            if (!QFileInfo(enableFile(m_found.curves, fan)).isWritable()) return false;
            for (int p = 1; p <= m_points; ++p) if (!QFileInfo(pointFile(m_found.curves, fan, p)).isWritable()) return false;
        }
        return true;
    }
    return !m_found.ec.isEmpty() && QFileInfo(m_found.ec + u"/cooler_boost"_s).isWritable();
}

void MsiFan::start()
{
    m_timer.start();
    poll();
}

void MsiFan::setFast(bool fast)
{
    const int ms = fast ? 3000 : 5000;
    if (m_timer.interval() != ms) m_timer.setInterval(ms);
}

int MsiFan::readInt(const QString &path) const
{
    bool ok = false;
    const int v = readText(path).toInt(&ok);
    return ok ? v : -1;
}

bool MsiFan::nbfcRuns() const
{
    const QString pid = readText(m_paths.nbfc(u"/run/nbfc_service.pid"_s));
    return !pid.isEmpty() && QFileInfo::exists(u"/proc/"_s + pid);
}

void MsiFan::poll()
{
    ++m_seq;
    const bool nbfc = nbfcRuns(), writable = canWriteFiles();
    const bool curves = !m_found.curves.isEmpty();
    if (const int v = curves ? readInt(enableFile(m_found.curves, 0)) : 2; v > 0) m_mode = v;
    if (m_mode != 1) m_written = { -1, -1 };   // (something else turned the curves off: they may not be ours any more)
    int t = -1;
    if (!m_found.ec.isEmpty()) t = readInt(m_found.ec + u"/cpu/realtime_temperature"_s);
    else if (!m_tempFile.isEmpty()) { t = readInt(m_tempFile); t = t < 0 ? -1 : t / 1000; }
    m_temp = t > 0 ? double(t) : std::numeric_limits<double>::quiet_NaN();
    QList<Fan> fans = m_fans;
    for (int i = 0; i < 2; ++i) {
        Fan &f = fans[i];
        f.temp = m_temp;
        const int speed = m_found.ec.isEmpty() ? -1
                        : readInt(m_found.ec + (i == 0 ? u"/cpu/realtime_fan_speed"_s : u"/gpu/realtime_fan_speed"_s));
        const int asked = m_pct[i];
        f.current = speed >= 0 ? std::min(speed, 100) : (m_mode == 1 && asked >= 0 ? double(asked) : 0);
        // As the driver is, so a hand-back that failed isn't taken for done. A fan asked to be automatic next to one
        // with a speed is on its factory curve and counts as handed back.
        f.autoControl = curves ? m_mode == 2 || (asked < 0 && m_pct[1 - i] >= 0) : asked < 0;
        f.target = f.autoControl ? f.current : asked;
    }
    const bool changedNow = nbfc != m_nbfc || writable != m_writable;
    m_nbfc = nbfc;
    m_writable = writable;
    m_fans = fans;
    emit statusRead(true);
    if (changedNow) emit changed();
}

QString MsiFan::blockedReason(bool withHolding) const
{
    Q_UNUSED(withHolding)
    if (!m_found.found()) return m_host.t(u"No MSI fan driver here"_s);
    if (m_nbfc) return m_host.t(u"The NBFC service is running and drives these fans too. Stop it first: sudo nbfc stop"_s);
    if (!m_writable) return m_host.t(u"Solwind can't write the fan speed yet. In a terminal: solwind fan-permission"_s);
    // Without the factory curves there would be no way back to them
    if (!m_found.curves.isEmpty() && (m_factory[0].isEmpty() || m_factory[1].isEmpty()))
        return m_host.t(u"The fan curves couldn't be read"_s);
    return QString();
}

bool MsiFan::put(const QString &path, const QByteArray &value, QString *error)
{
    QFile f(path);
    if (f.open(QIODevice::WriteOnly) && f.write(value) > 0) {
        f.close();
        if (f.error() == QFile::NoError) return true;
    }
    *error = f.errorString();
    return false;
}

// A fan's curve: every point at a speed, or the factory's
bool MsiFan::writeCurve(int fan, int pwm, QString *error)
{
    for (int p = 1; p <= m_points; ++p) {
        const int v = pwm >= 0 ? pwm : m_factory[fan].value(p - 1, -1);
        if (v >= 0 && !put(pointFile(m_found.curves, fan, p), QByteArray::number(v), error)) return false;
    }
    return true;
}

QString MsiFan::applyCurves()
{
    const QString &dir = m_found.curves;
    QString error;
    const bool none = m_pct[0] < 0 && m_pct[1] < 0;
    bool failed = false;
    for (int fan = 0; fan < 2; ++fan) {          // (a failure on one doesn't stop the other's way back)
        const int pwm = m_pct[fan] < 0 ? -1 : pwmOf(m_pct[fan]);
        if (pwm < 0 ? !m_ours[fan] : pwm == m_written[fan]) continue;
        if (!writeCurve(fan, pwm, &error)) { failed = true; continue; }
        m_written[fan] = pwm;
        m_ours[fan] = pwm >= 0;
    }
    if (none || failed) {
        // Automatic, also after a failure: not left on a curve nobody chose
        QString off;
        for (int fan = 0; fan < 2; ++fan) if (put(enableFile(dir, fan), "2", &off)) m_mode = 2;
        m_written = { -1, -1 };
        return failed ? error : off;
    }
    if (m_mode != 1) {
        for (int fan = 0; fan < 2; ++fan) if (!put(enableFile(dir, fan), "1", &error)) return error;
        m_mode = 1;
    }
    return QString();
}

// Only full speed exists: the core's 100% (max fan, its safety limit), everything else leaves the firmware in charge
QString MsiFan::applyBoost()
{
    QString error;
    const bool on = m_pct[0] >= 100 || m_pct[1] >= 100;
    return put(m_found.ec + u"/cooler_boost"_s, on ? "on" : "off", &error) ? QString() : error;
}

QString MsiFan::apply()
{
    // (going back to automatic only needs the permission: also with NBFC running)
    if (const QString b = blockedReason(); !b.isEmpty() && (!m_writable || m_pct[0] >= 0 || m_pct[1] >= 0)) return b;
    return !m_found.curves.isEmpty() ? applyCurves() : applyBoost();
}

void MsiFan::finish(const Done &done, const QString &error)
{
    if (done) QTimer::singleShot(0, this, [done, error] { done(error.isEmpty(), error); });
    QTimer::singleShot(0, this, &MsiFan::poll);
}

// What was refused or failed isn't kept: a later command must not carry it out
void MsiFan::change(const std::function<void()> &what, const Done &done)
{
    const auto before = m_pct;
    what();
    const QString error = apply();
    if (!error.isEmpty()) m_pct = before;
    finish(done, error);
}

void MsiFan::setPercent(int fan, int pct, Done done)
{
    if (fan < 0 || fan > 1) { finish(done, m_host.t(u"No such fan"_s)); return; }
    change([&] { m_pct[fan] = std::clamp(pct, 0, 100); }, done);
}

void MsiFan::setAuto(int fan, Done done)
{
    if (fan > 1) { finish(done, m_host.t(u"No such fan"_s)); return; }
    change([&] { if (fan < 0) m_pct = { -1, -1 }; else m_pct[fan] = -1; }, done);
}

void MsiFan::releaseAll(Done done)
{
    change([&] { m_pct = { -1, -1 }; }, done);
}

QStringList MsiFan::handBackCommand() const
{
    if (!m_found.curves.isEmpty()) {
        // The factory curves as path, value pairs, then the curves off
        QStringList c{ u"sh"_s, u"-c"_s,
                       u"a=$1; b=$2; shift 2; while [ $# -gt 0 ]; do echo \"$2\" > \"$1\"; shift 2; done; echo 2 > \"$a\" && echo 2 > \"$b\""_s,
                       u"sh"_s, enableFile(m_found.curves, 0), enableFile(m_found.curves, 1) };
        for (int fan = 0; fan < 2; ++fan)
            for (qsizetype p = 0; p < m_factory[fan].size(); ++p)
                c << pointFile(m_found.curves, fan, int(p) + 1) << QString::number(m_factory[fan][p]);
        return c;
    }
    if (!m_found.ec.isEmpty()) return { u"sh"_s, u"-c"_s, u"echo off > \"$1\""_s, u"sh"_s, m_found.ec + u"/cooler_boost"_s };
    return {};
}
