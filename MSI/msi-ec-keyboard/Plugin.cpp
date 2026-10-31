// The plugin file solwindd loads (MSI/msi-ec-keyboard.so)
#include "MsiEcKeyboard.h"
#include "plugins/SolwindPlugin.h"

class MsiEcKeyboardPlugin : public QObject, public SolwindPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID SolwindPlugin_iid FILE "plugin.json")
    Q_INTERFACES(SolwindPlugin)
public:
    QList<const KeyboardPlugin *> keyboardPlugins() const override { return { &m_light }; }

private:
    MsiEcKeyboard m_light;
};

#include "Plugin.moc"
