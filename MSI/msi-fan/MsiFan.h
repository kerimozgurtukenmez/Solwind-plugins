#pragma once

#include "plugins/FanPlugin.h"
#include "plugins/PluginHost.h"

#include <QTimer>

#include <array>
#include <limits>

// The two fans of MSI laptops, through what the kernel offers. Never through the embedded controller's memory.
// - Speeds: the hwmon "msi_wmi_platform" with fan curves (pwm1_enable, pwm2_enable: 1 the curves, 2 automatic, one
//   setting of the firmware behind both; pwmN_auto_pointM_pwm, 0-255). Those are patches for msi-wmi-platform, in some
//   kernels (Bazzite's) and not in the official one when this was written. The firmware runs the curve; a speed here is a
//   curve with every point at that speed, so the core's Fixed and Curve work as on other fans.
// - Without them, the msi-ec module: temperatures, fan speeds, and cooler_boost for full speed. It can't set a speed
//   (fan_mode only picks a firmware mode, and "silent" turns the fans off, so it's never used): canSetSpeeds() is
//   false, and the window offers Auto and max fan.
// A speed never goes below FloorPwm (30%): factory curves seen start at 38%, and nothing lower was tried. The firmware
// runs one curve per fan, so a fan on automatic next to a driven one keeps its factory curve. Handing back: the factory
// curves as they were read (not every kernel's driver puts them back, and automatic mode still leans on them), then the
// curves off; with msi-ec cooler boost off. Saved fan settings are apart for the two ways (configId). Not tested on
// hardware.
class MsiFan : public FanPlugin
{
    Q_OBJECT
public:
    static constexpr int FloorPwm = 77;

    struct Found {
        QString curves;                    // the hwmon folder with fan curves
        QString ec;                        // /sys/devices/platform/msi-ec
        bool found() const { return !curves.isEmpty() || !ec.isEmpty(); }
    };
    static Found find(const Paths &paths);
    static int pwmOf(int percent);

    explicit MsiFan(const PluginHost &host, QObject *parent = nullptr);

    QString id() const override { return QStringLiteral("msi-fan"); }
    Trust trust() const override { return Trust::WriteTested; }
    bool canSetSpeeds() const override { return !m_found.curves.isEmpty(); }
    void start() override;
    void setFast(bool fast) override;
    int interval() const override { return m_timer.interval(); }

    bool running() const override { return m_found.found(); }
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
    void readFactory();
    QString apply();
    QString applyCurves();
    bool writeCurve(int fan, int pwm, QString *error);
    QString applyBoost();
    bool put(const QString &path, const QByteArray &value, QString *error);
    int readInt(const QString &path) const;
    void change(const std::function<void()> &what, const Done &done);
    void finish(const Done &done, const QString &error);

    const PluginHost &m_host;
    Paths m_paths;
    Found m_found;
    QString m_tempFile, m_config;
    int m_points = 0;                      // of a curve
    QTimer m_timer;
    QList<Fan> m_fans;
    std::array<int, 2> m_pct{ { -1, -1 } };  // what was asked, per fan: -1 automatic
    std::array<int, 2> m_written{ { -1, -1 } };   // the pwm of every point of the fan's curve, -1 not known
    std::array<bool, 2> m_ours{ { false, false } };   // its curve holds our speed, not the factory's
    std::array<QList<int>, 2> m_factory;   // the curves as found, empty if they couldn't be read
    int m_mode = 2;                        // pwm1_enable as last read
    double m_temp = std::numeric_limits<double>::quiet_NaN();
    bool m_writable = false;
    bool m_nbfc = false;
    quint64 m_seq = 0;
};
