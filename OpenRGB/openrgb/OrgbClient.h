#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

// The little of OpenRGB's SDK protocol this plugin needs (Documentation/OpenRGBSDK.md): a blocking client on
// 127.0.0.1. Protocol 3 to 5 use controller indexes and the layout read below; 4 is asked for, or less if the server
// has less. A server below 3 (OpenRGB before 0.7) isn't used.
namespace Orgb {

constexpr quint16 defaultPort = 6742;
constexpr quint32 ourProtocol = 4;

struct Controller {
    QString name;
    QStringList zones;
    int leds = 0;
    bool isKeyboard() const;              // by its name or a zone's: device type numbers differ between OpenRGB versions
};

// One REQUEST_CONTROLLER_DATA answer, without its leading data_size; nullopt if it isn't what the protocol says
std::optional<Controller> parseController(const QByteArray &data, quint32 protocol);
// The UPDATELEDS packet's data: every LED in one color
QByteArray colorsPacket(int leds, quint8 r, quint8 g, quint8 b);

class Client
{
public:
    explicit Client(quint16 port = defaultPort) : m_port(port) {}
    ~Client() { close(); }
    Client(const Client &) = delete;
    Client &operator=(const Client &) = delete;

    bool connected() const { return m_fd >= 0; }
    bool open();                          // connects and agrees on the protocol; false: no usable server
    void close();
    // Is the server still there? (A send to a server that went succeeds; only the next read shows it.) What it sent
    // meanwhile, device changes it tells of, is thrown away.
    bool stillOpen();
    bool takeListChanged();               // it told something meanwhile: indexes may have changed, list them again
    // Reading stops after this long from now, however slowly the server sends (each call starts its own)
    void limit(int ms);
    quint32 protocol() const { return m_protocol; }
    QList<Controller> controllers();      // empty also if it fails
    bool setDirect(int index);            // SETCUSTOMMODE: the mode that takes colors from us (it can change the light)
    bool setColor(int index, int leds, quint8 r, quint8 g, quint8 b);

private:
    bool send(quint32 device, quint32 id, const QByteArray &data = {});
    std::optional<QByteArray> answer(quint32 id);    // the data of the next packet with this id (others skipped)
    bool readAll(char *to, qint64 size);

    quint16 m_port;
    int m_fd = -1;
    quint32 m_protocol = 0;
    QElapsedTimer m_time;
    int m_limitMs = 2000;
    bool m_listChanged = false;
};
}
