#pragma once

#include "plugins/FanPlugin.h"
#include "plugins/PluginHost.h"

#include <QTimer>

#include <array>
#include <limits>

// The two fans of HP Victus / Omen laptops through the hp-wmi driver's hwmon "hp": pwm1_enable (0 max, 1 manual,
// 2 automatic, one mode for both fans), pwm1 and pwm2 (0-255). Reads and writes are short sysfs accesses, done in place,
// so commands simply run in the order they come.
//
// What the driver and the firmware do, and what follows from it (measured on board 8BBD, the rest from hp-wmi.c):
// - The driver has no lower limit on this board and rounds a low pwm down to 0, which means "automatic" for that fan
//   (pwm 1 with both fans: the firmware's own curve, which stands them still when cool). Between that and the firmware table's
//   lowest row (pwm 106, about 2000 RPM) nothing was tried, so a speed never goes below that row. A pwm above 255
//   overflows in the driver: it is never written.
// - Manual mode is shared by both fans. A fan left on the driver's per-fan automatic (pwm 1) next to a fixed one stood
//   still on 8BBD while the CPU went to 96 °C, so that is never written: a fan on automatic next to a fixed one runs
//   at that fan's speed, and with it up to 100% when the core's safety limit acts.
// - The driver repeats the command every 90 s for as long as manual mode is on, also after solwindd has gone, and
//   nobody watches the temperature then: handBackCommand() writes automatic. The files belong to root until the
//   permission is given (solwind fan-permission).
// - Going to manual reads the current RPM as the first speed, which is 0 (automatic) for a standing fan, so both pwm
//   files are written right after it.
// - NBFC must not run next to it: both would drive the same fans (readOnly() and blockedReason() say so).
class HpWmiFan : public FanPlugin
{
    Q_OBJECT
public:
    static constexpr int FloorPwm = 106;

    // The driver's hwmon folder with both fans; "" if there is none
    static QString find(const Paths &paths);
    static int pwmOf(int percent);        // the speed a percentage is written as, floor included

    explicit HpWmiFan(const PluginHost &host, QObject *parent = nullptr);

    QString id() const override { return QStringLiteral("hp-wmi-fan"); }
    // The driver opens pwm1 and pwm2 only on the boards of its own list, so find() finding them is the board check.
    // A board whose firmware ignores the command (reported on one) simply stays on automatic.
    Trust trust() const override { return Trust::WriteTested; }
    void start() override;
    void setFast(bool fast) override;
    int interval() const override { return m_timer.interval(); }

    bool running() const override { return !m_dir.isEmpty(); }
    bool readOnly() const override { return !m_writable || m_nbfc; }
    bool holding() const override { return false; }
    bool quiet() const override { return false; }
    QString configId() const override { return m_config; }
    double temp() const override { return m_temp; }
    const QList<Fan> &fans() const override { return m_fans; }
    quint64 pollSeq() const override { return m_seq; }
    quint64 statusSeq() const override { return m_seq; }
    QString blockedReason(bool withHolding = true) const override;

    void setPercent(int fan, int pct, Done done) override;
    void setAuto(int fan, Done done) override;
    void releaseAll(Done done) override;
    QStringList handBackCommand() const override;
    void willRestoreFixedSpeeds(std::function<void(bool)> done) override { if (done) done(false); }

private:
    void poll();
    bool nbfcRuns() const;
    bool canWriteFiles() const;
    QString apply();                       // writes m_pct to the driver; "" or what went wrong
    bool put(const QString &file, int value, QString *error);
    int readInt(const QString &path) const;
    void finish(const Done &done, const QString &error);

    const PluginHost &m_host;
    Paths m_paths;
    QString m_dir, m_tempFile, m_config;
    QTimer m_timer;
    QList<Fan> m_fans;
    std::array<int, 2> m_pct{ { -1, -1 } };  // what was asked, per fan: -1 automatic
    int m_mode = 2;                        // pwm1_enable as last read
    double m_temp = std::numeric_limits<double>::quiet_NaN();
    bool m_writable = false;
    bool m_nbfc = false;
    bool m_up = false;
    quint64 m_seq = 0;
};
