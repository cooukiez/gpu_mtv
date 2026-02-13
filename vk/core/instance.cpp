//
// Created by Ludw on 4/25/2024.
//

#include "../../app.h"

std::vector<const char *> App::get_required_exts() {
    std::vector<const char *> exts;

#ifdef VALIDATION
    exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    return exts;
}

void App::create_inst() {
#ifdef VALIDATION
    if (!check_validation_support())
        throw std::runtime_error("validation layers requested, but not available.");
#endif

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = APP_NAME;
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = ENGINE_NAME;
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo inst_info{};
    inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    inst_info.pApplicationInfo = &app_info;

    const auto exts = get_required_exts();
    inst_info.enabledExtensionCount = static_cast<uint32_t>(exts.size());
    inst_info.ppEnabledExtensionNames = exts.data();

    VkDebugUtilsMessengerCreateInfoEXT debug_info{};
#ifdef VALIDATION
    inst_info.enabledLayerCount = static_cast<uint32_t>(val_layers.size());
    inst_info.ppEnabledLayerNames = val_layers.data();

    get_debug_msg_info(debug_info);
    inst_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT *) &debug_info;
#else
    inst_info.enabledLayerCount = 0;
    inst_info.pNext = nullptr;
#endif

    if (vkCreateInstance(&inst_info, nullptr, &inst) != VK_SUCCESS)
        throw std::runtime_error("failed to create instance.");
}

void App::setup_debug_msg() {
#ifdef VALIDATION
    VkDebugUtilsMessengerCreateInfoEXT debug_info;
    get_debug_msg_info(debug_info);

    if (create_debug_callback(inst, &debug_info, nullptr, &debug_msg) != VK_SUCCESS)
        throw std::runtime_error("failed to set up debug messenger.");
#endif
}
