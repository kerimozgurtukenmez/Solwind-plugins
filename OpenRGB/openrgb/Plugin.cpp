// The plugin file solwindd loads (OpenRGB/openrgb.so)
#include "OpenRgbKeyboard.h"
#include "plugins/SolwindPlugin.h"

class OpenRgbPlugin : public QObject, public SolwindPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID SolwindPlugin_iid FILE "plugin.json")
    Q_INTERFACES(SolwindPlugin)
public:
    QList<const KeyboardPlugin *> keyboardPlugins() const override { return { &m_light }; }

private:
    OpenRgbKeyboard m_light;
};

#include "Plugin.moc"
