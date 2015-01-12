#include "GSFPlugin.h"
namespace chipmachine {
static ChipPlugin::RegisterMe registerMe([](const std::string &configDir) -> std::shared_ptr<ChipPlugin> { return std::make_shared<GSFPlugin>(); });
}
