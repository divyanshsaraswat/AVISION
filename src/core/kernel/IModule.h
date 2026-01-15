#pragma once
#include "../types/Context.h"
#include <string>

class IModule {
public:
    virtual ~IModule() = default;

#include <map>

    // Initialize the module with configuration params
    // Returns true on success
    virtual bool init(const std::map<std::string, std::string>& params) = 0;

    // Process the current context
    // Read from ctx inputs, Write to ctx results
    virtual void process(Context& ctx) = 0;

    // Module identifier
    virtual std::string getName() const = 0;
};
