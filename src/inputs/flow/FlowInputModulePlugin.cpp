/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "FlowInputModulePlugin.h"
#include "CoreRegistry.h"
#include "InputStreamManager.h"
#include <Corrade/PluginManager/AbstractManager.h>

CORRADE_PLUGIN_REGISTER(VisorInputFlow, visor::input::flow::FlowInputModulePlugin,
    "visor.module.input/1.0")

namespace visor::input::flow {

void FlowInputModulePlugin::setup_routes([[maybe_unused]] HttpServer *svr)
{
}

std::unique_ptr<InputStream> FlowInputModulePlugin::instantiate(const std::string name, const Configurable *config)
{
    auto input_stream = std::make_unique<FlowInputStream>(name);
    input_stream->config_merge(*config);
    return input_stream;
}

}
