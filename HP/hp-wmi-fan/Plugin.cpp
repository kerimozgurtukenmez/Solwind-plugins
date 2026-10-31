// The plugin file solwindd loads (HP/hp-wmi-fan.so)
#include "HpWmiFan.h"
#include "plugins/SolwindPlugin.h"

class HpWmiFanFile : public QObject, public SolwindPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID SolwindPlugin_iid FILE "plugin.json")
    Q_INTERFACES(SolwindPlugin)
public:
    // No driver with two fans here: no fan plugin, so another one (or none) takes the fans
    FanPlugin *createFan(const PluginHost &host, QObject *parent) override
    {
        return HpWmiFan::find(host.paths()).isEmpty() ? nullptr : new HpWmiFan(host, parent);
    }
};

#include "Plugin.moc"
