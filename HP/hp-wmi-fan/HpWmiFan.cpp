#include "HpWmiFan.h"

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
QString board(const Paths &paths) { return readText(paths.sys(u"/class/dmi/id/board_name"_s)); }
}

QString HpWmiFan::find(const Paths &paths)
{
    const QDir d(paths.sys(u"/class/hwmon"_s));
    for (const QString &e : d.entryList({ u"hwmon*"_s }, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        const QString h = d.filePath(e);
        if (readText(h + u"/name"_s) == u"hp"_s && QFileInfo::exists(h + u"/pwm1_enable"_s)
            && QFileInfo::exists(h + u"/pwm1"_s) && QFileInfo::exists(h + u"/pwm2"_s))
            return h;
    }
    return QString();
}

int HpWmiFan::pwmOf(int percent)
{
    return std::clamp(int(std::lround(percent * 2.55)), int(FloorPwm), 255);
}

HpWmiFan::HpWmiFan(const PluginHost &host, QObject *parent) : FanPlugin(parent), m_host(host), m_paths(host.paths())
{
    m_dir = find(m_paths);
    const QString b = board(m_paths);
    m_config = b.isEmpty() ? u"HP"_s : u"HP "_s + b;
    const QDir d(m_paths.sys(u"/class/hwmon"_s));
    for (const QString &e : d.entryList({ u"hwmon*"_s }, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        const QString n = readText(d.filePath(e) + u"/name"_s);
        if (n == u"coretemp"_s || n == u"k10temp"_s) { m_tempFile = d.filePath(e) + u"/temp1_input"_s; break; }
    }
    m_fans = { Fan{ u"CPU Fan"_s }, Fan{ u"GPU Fan"_s } };
    m_timer.setInterval(5000);
    connect(&m_timer, &QTimer::timeout, this, &HpWmiFan::poll);
}

void HpWmiFan::start()
{
    m_nbfc = nbfcRuns();
    m_writable = canWriteFiles();
    // Left by a solwindd that died without its hand-back: nothing watches these fans now. (Not max: it only cools more.)
    if (m_writable && !m_nbfc && readInt(m_dir + u"/pwm1_enable"_s) == 1) {
        QString error;
        put(u"pwm1_enable"_s, 2, &error);
    }
    m_timer.start();
    poll();
}

void HpWmiFan::setFast(bool fast)
{
    const int ms = fast ? 3000 : 5000;
    if (m_timer.interval() != ms) m_timer.setInterval(ms);
}

int HpWmiFan::readInt(const QString &path) const
{
    bool ok = false;
    const int v = readText(path).toInt(&ok);
    return ok ? v : -1;
}

bool HpWmiFan::canWriteFiles() const
{
    if (m_dir.isEmpty()) return false;
    for (const char *f : { "/pwm1_enable", "/pwm1", "/pwm2" })
        if (!QFileInfo(m_dir + QLatin1StringView(f)).isWritable()) return false;
    return true;
}

bool HpWmiFan::nbfcRuns() const
{
    const QString pid = readText(m_paths.nbfc(u"/run/nbfc_service.pid"_s));
    return !pid.isEmpty() && QFileInfo::exists(u"/proc/"_s + pid);
}

void HpWmiFan::poll()
{
    ++m_seq;
    const bool nbfc = nbfcRuns();
    const bool writable = canWriteFiles();
    const bool up = !m_dir.isEmpty() && QFileInfo::exists(m_dir + u"/pwm1_enable"_s);
    m_mode = up ? readInt(m_dir + u"/pwm1_enable"_s) : 2;
    const int t = m_tempFile.isEmpty() ? -1 : readInt(m_tempFile);
    m_temp = t >= 0 ? t / 1000.0 : std::numeric_limits<double>::quiet_NaN();
    QList<Fan> fans = m_fans;
    for (int i = 0; i < fans.size(); ++i) {
        const int pwm = up ? readInt(m_dir + u"/pwm%1"_s.arg(i + 1)) : -1;
        Fan &f = fans[i];
        f.temp = m_temp;
        f.current = pwm >= 0 ? pwm / 2.55 : 0;
        // What the driver really does, so a hand-back that failed isn't taken for done, and a driver back on automatic
        // by something else is written again by the core. A fan asked to be automatic next to one with a speed counts
        // as handed back (it follows that fan); with both asked, only pwm1_enable says so.
        f.autoControl = m_mode == 2 || (m_pct[i] < 0 && m_pct[1 - i] >= 0);
        f.target = f.autoControl ? f.current : m_pct[i];
    }
    const bool changedNow = nbfc != m_nbfc || writable != m_writable || up != m_up;
    m_nbfc = nbfc;
    m_writable = writable;
    m_up = up;
    m_fans = fans;
    emit statusRead(up);
    if (changedNow) emit changed();
}

QString HpWmiFan::blockedReason(bool withHolding) const
{
    Q_UNUSED(withHolding)
    if (m_dir.isEmpty()) return m_host.t(u"The hp-wmi driver has no fans here"_s);
    if (m_nbfc) return m_host.t(u"The NBFC service is running and drives these fans too. Stop it first: sudo nbfc stop"_s);
    if (!m_writable) return m_host.t(u"Solwind can't write the fan speed yet. In a terminal: solwind fan-permission"_s);
    return QString();
}

bool HpWmiFan::put(const QString &file, int value, QString *error)
{
    QFile f(m_dir + u'/' + file);
    if (f.open(QIODevice::WriteOnly) && f.write(QByteArray::number(value)) > 0) {
        f.close();
        if (f.error() == QFile::NoError) return true;
    }
    *error = f.errorString();
    return false;
}

QString HpWmiFan::apply()
{
    if (const QString b = blockedReason(); !b.isEmpty()) return b;
    QString error;
    if (m_pct[0] < 0 && m_pct[1] < 0) {
        if (!put(u"pwm1_enable"_s, 2, &error)) return error;
        m_mode = 2;
        return QString();
    }
    if (m_mode != 1) {
        if (!put(u"pwm1_enable"_s, 1, &error)) return error;
        m_mode = 1;
    }
    for (int i = 0; i < 2; ++i) {
        if (put(u"pwm%1"_s.arg(i + 1), pwmOf(m_pct[i] < 0 ? m_pct[1 - i] : m_pct[i]), &error)) continue;
        // Not left in manual mode at a speed nobody chose
        QString ignored;
        if (put(u"pwm1_enable"_s, 2, &ignored)) m_mode = 2;
        return error;
    }
    return QString();
}

void HpWmiFan::finish(const Done &done, const QString &error)
{
    if (done) QTimer::singleShot(0, this, [done, error] { done(error.isEmpty(), error); });
    QTimer::singleShot(0, this, &HpWmiFan::poll);
}

// What was refused or failed isn't kept: a later command must not carry it out
void HpWmiFan::change(const std::function<void()> &what, const Done &done)
{
    const auto before = m_pct;
    what();
    const QString error = apply();
    if (!error.isEmpty()) m_pct = before;
    finish(done, error);
}

void HpWmiFan::setPercent(int fan, int pct, Done done)
{
    if (fan < 0 || fan > 1) { finish(done, m_host.t(u"No such fan"_s)); return; }
    change([&] { m_pct[fan] = std::clamp(pct, 0, 100); }, done);
}

void HpWmiFan::setAuto(int fan, Done done)
{
    if (fan > 1) { finish(done, m_host.t(u"No such fan"_s)); return; }
    change([&] { if (fan < 0) m_pct = { -1, -1 }; else m_pct[fan] = -1; }, done);
}

void HpWmiFan::releaseAll(Done done)
{
    change([&] { m_pct = { -1, -1 }; }, done);
}

QStringList HpWmiFan::handBackCommand() const
{
    if (m_dir.isEmpty()) return {};
    return { u"sh"_s, u"-c"_s, u"echo 2 > \"$1\""_s, u"sh"_s, m_dir + u"/pwm1_enable"_s };
}
