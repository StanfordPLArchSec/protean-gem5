#pragma once

class Plugin;
void register_plugin(Plugin *plugin);

class Plugin {
  private:
    Plugin()
    {
        register_plugin(this);
    }

    template <class Derived>
    friend class PluginT;

  public:
    virtual void reg() = 0;
};


template <class Derived>
class PluginT : public Plugin {
    static Derived plugin;

  public:
    static Derived *
    getInstance()
    {
        return &plugin;
    }
};
