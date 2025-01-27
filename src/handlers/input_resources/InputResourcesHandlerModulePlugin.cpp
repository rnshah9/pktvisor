/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "InputResourcesHandlerModulePlugin.h"
#include "CoreRegistry.h"
#include "InputResourcesStreamHandler.h"
#include "HandlerManager.h"
#include "InputStreamManager.h"
#include <Corrade/PluginManager/AbstractManager.h>
#include <nlohmann/json.hpp>

CORRADE_PLUGIN_REGISTER(VisorHandlerInputResources, visor::handler::resources::InputResourcesHandlerModulePlugin,
    "visor.module.handler/1.0")

namespace visor::handler::resources {

using json = nlohmann::json;

void InputResourcesHandlerModulePlugin::setup_routes(HttpServer *svr)
{
}
std::unique_ptr<StreamHandler> InputResourcesHandlerModulePlugin::instantiate(const std::string &name, InputStream *input_stream, const Configurable *config, StreamHandler *stream_handler)
{
    // TODO using config as both window config and module config
    auto handler_module = std::make_unique<InputResourcesStreamHandler>(name, input_stream, config, stream_handler);
    handler_module->config_merge(*config);
    return handler_module;
}

}