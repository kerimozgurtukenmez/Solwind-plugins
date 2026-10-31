// The plugin file solwindd loads (MSI/msi-fan.so)
#include "MsiFan.h"
#include "plugins/SolwindPlugin.h"

class MsiFanFile : public QObject, public SolwindPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID SolwindPlugin_iid FILE "plugin.json")
    Q_INTERFACES(SolwindPlugin)
public:
    // No MSI driver with fans here: no fan plugin, so another one (or none) takes the fans
    FanPlugin *createFan(const PluginHost &host, QObject *parent) override
    {
        return MsiFan::find(host.paths()).found() ? new MsiFan(host, parent) : nullptr;
    }
};

#include "Plugin.moc"
