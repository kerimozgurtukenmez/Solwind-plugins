// SAFETY: talks only to 127.0.0.1. What the server sends is checked against the sizes it states before it is used.
#include "OrgbClient.h"

#include <QtEndian>

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

using namespace Qt::StringLiterals;

namespace Orgb {
namespace {
enum Packet : quint32 {
    ControllerCount = 0, ControllerData = 1, ProtocolVersion = 40, ClientName = 50, UpdateLeds = 1050, SetCustomMode = 1100,
};
constexpr qint64 headerSize = 16;
constexpr quint32 largestPacket = 16 * 1024 * 1024;   // (a matrix map of a big keyboard is some kB)

class Reader
{
public:
    explicit Reader(const QByteArray &d) : m_d(d) {}
    bool ok() const { return m_ok; }
    void skip(qint64 n) { if (n < 0 || n > m_d.size() - m_at) m_ok = false; else m_at += n; }
    template <typename T> T get()
    {
        if (m_d.size() - m_at < qint64(sizeof(T))) { m_ok = false; return T(); }
        const T v = qFromLittleEndian<T>(m_d.constData() + m_at);
        m_at += sizeof(T);
        return v;
    }
    QString string()                      // unsigned short length, then the text with its null
    {
        const quint16 n = get<quint16>();
        if (!m_ok || n > m_d.size() - m_at) { m_ok = false; return QString(); }
        QString s = QString::fromUtf8(m_d.constData() + m_at, n ? n - 1 : 0);
        m_at += n;
        return s;
    }
private:
    const QByteArray &m_d;
    qint64 m_at = 0;
    bool m_ok = true;
};

void put32(QByteArray &to, quint32 v)
{
    char b[4];
    qToLittleEndian<quint32>(v, b);
    to.append(b, 4);
}
}

bool Controller::isKeyboard() const
{
    if (name.contains(u"keyboard"_s, Qt::CaseInsensitive)) return true;
    for (const QString &z : zones) if (z.contains(u"keyboard"_s, Qt::CaseInsensitive)) return true;
    return false;
}

std::optional<Controller> parseController(const QByteArray &data, quint32 protocol)
{
    Reader r(data);
    Controller c;
    r.skip(4);                            // type
    c.name = r.string();
    for (int i = 0; i < 5; ++i) r.string();   // vendor, description, version, serial, location
    const quint16 modes = r.get<quint16>();
    r.skip(4);                            // active_mode
    for (int i = 0; i < modes && r.ok(); ++i) {
        r.string();
        r.skip(4 * 12);                   // value, flags, speed min/max, brightness min/max, colors min/max, speed, brightness, direction, color mode
        const quint16 colors = r.get<quint16>();
        r.skip(qint64(colors) * 4);
    }
    const quint16 zones = r.get<quint16>();
    for (int i = 0; i < zones && r.ok(); ++i) {
        c.zones << r.string();
        r.skip(4 * 4);                    // type, leds min/max/count
        r.skip(r.get<quint16>());         // matrix map
        if (protocol >= 4) {
            const quint16 segments = r.get<quint16>();
            for (int s = 0; s < segments && r.ok(); ++s) { r.string(); r.skip(4 * 3); }
        }
        if (protocol >= 5) r.skip(4);     // flags
    }
    c.leds = r.get<quint16>();
    return r.ok() && c.leds > 0 ? std::optional(c) : std::nullopt;
}

QByteArray colorsPacket(int leds, quint8 r, quint8 g, quint8 b)
{
    QByteArray d;
    put32(d, quint32(4 + 2 + leds * 4));
    char n[2];
    qToLittleEndian<quint16>(quint16(leds), n);
    d.append(n, 2);
    for (int i = 0; i < leds; ++i) { d.append(char(r)); d.append(char(g)); d.append(char(b)); d.append('\0'); }
    return d;
}

bool Client::open()
{
    close();
    m_fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (m_fd < 0) return false;
    timeval tv{ 0, 300000 };              // (a total limit is kept in readAll)
    ::setsockopt(m_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    ::setsockopt(m_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
    const int on = 1;
    ::setsockopt(m_fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof on);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(m_port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    limit(2000);
    if (::connect(m_fd, reinterpret_cast<sockaddr *>(&a), sizeof a) != 0) { close(); return false; }
    QByteArray v;
    put32(v, ourProtocol);
    if (!send(0, ProtocolVersion, v)) { close(); return false; }
    const auto reply = answer(ProtocolVersion);
    if (!reply || reply->size() < 4) { close(); return false; }
    m_protocol = qMin(ourProtocol, qFromLittleEndian<quint32>(reply->constData()));
    if (m_protocol < 3) { close(); return false; }
    send(0, ClientName, QByteArrayLiteral("Solwind") + '\0');
    return connected();
}

void Client::close()
{
    if (m_fd >= 0) ::close(m_fd);
    m_fd = -1;
    m_protocol = 0;
}

bool Client::stillOpen()
{
    char waste[4096];
    while (connected()) {
        const ssize_t n = ::recv(m_fd, waste, sizeof waste, MSG_DONTWAIT);
        if (n > 0) { m_listChanged = true; continue; }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return true;
        if (n < 0 && errno == EINTR) continue;
        close();
    }
    return false;
}

bool Client::takeListChanged()
{
    const bool was = m_listChanged;
    m_listChanged = false;
    return was;
}

void Client::limit(int ms)
{
    m_limitMs = ms;
    m_time.start();
}

bool Client::send(quint32 device, quint32 id, const QByteArray &data)
{
    QByteArray p("ORGB", 4);
    put32(p, device);
    put32(p, id);
    put32(p, quint32(data.size()));
    p += data;
    for (qint64 sent = 0; sent < p.size();) {
        const ssize_t n = ::send(m_fd, p.constData() + sent, size_t(p.size() - sent), MSG_NOSIGNAL);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { close(); return false; }
        sent += n;
    }
    return true;
}

bool Client::readAll(char *to, qint64 size)
{
    for (qint64 got = 0; got < size;) {
        if (m_time.hasExpired(m_limitMs)) { close(); return false; }
        const ssize_t n = ::recv(m_fd, to + got, size_t(size - got), 0);
        if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        if (n <= 0) { close(); return false; }
        got += n;
    }
    return true;
}

std::optional<QByteArray> Client::answer(quint32 id)
{
    for (int skipped = 0; skipped < 64 && connected(); ++skipped) {
        char h[headerSize];
        if (!readAll(h, headerSize)) return std::nullopt;
        if (std::memcmp(h, "ORGB", 4) != 0) { close(); return std::nullopt; }
        const quint32 got = qFromLittleEndian<quint32>(h + 8), size = qFromLittleEndian<quint32>(h + 12);
        if (size > largestPacket) { close(); return std::nullopt; }
        QByteArray d(qsizetype(size), Qt::Uninitialized);
        if (size && !readAll(d.data(), size)) return std::nullopt;
        if (got == id) return d;
    }
    return std::nullopt;
}

QList<Controller> Client::controllers()
{
    QList<Controller> out;
    if (!connected() || !send(0, ControllerCount)) return out;
    const auto n = answer(ControllerCount);
    if (!n || n->size() < 4) return out;
    const quint32 count = qFromLittleEndian<quint32>(n->constData());
    for (quint32 i = 0; i < count && i < 256; ++i) {
        QByteArray v;
        put32(v, m_protocol);
        if (!send(i, ControllerData, v)) return {};
        const auto d = answer(ControllerData);
        if (!d || d->size() < 4) return {};
        const auto c = parseController(d->mid(4), m_protocol);
        out << c.value_or(Controller());      // (kept: the index is the controller's id)
    }
    return out;
}

bool Client::setDirect(int index) { return connected() && send(quint32(index), SetCustomMode); }

bool Client::setColor(int index, int leds, quint8 r, quint8 g, quint8 b)
{
    return connected() && send(quint32(index), UpdateLeds, colorsPacket(leds, r, g, b));
}
}
