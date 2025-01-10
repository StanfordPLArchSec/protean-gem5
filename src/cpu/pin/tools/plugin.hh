#pragma once

#include <vector>
#include <string>

struct Plugin
{
    Plugin();
    Plugin(const Plugin &) = delete;

    virtual bool enabled() const = 0;
    virtual bool reg() = 0;
    virtual bool command(const std::string &cmd, const std::vector<std::string> &args, std::string &result) { return false; }
};

extern std::vector<Plugin *> plugins;
