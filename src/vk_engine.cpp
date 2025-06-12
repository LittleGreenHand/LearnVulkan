//> includes
#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_initializers.h>
#include <vk_types.h>

#include "VkBootstrap.h"

#include <chrono>
#include <thread>

VulkanEngine* loadedEngine = nullptr;

#ifdef NDEBUG
constexpr bool bUseValidationLayers = false;
#else
constexpr bool bUseValidationLayers = true;
#endif

VulkanEngine& VulkanEngine::Get() { return *loadedEngine; }
void VulkanEngine::init()
{
    assert(loadedEngine == nullptr);
    loadedEngine = this;
        
    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

    _window = SDL_CreateWindow(
        "Vulkan Engine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        _windowExtent.width,
        _windowExtent.height,
        window_flags);

    init_vulkan();

    init_swapchain();

    init_commands();

    init_sync_structures();

    _isInitialized = true;
}

void VulkanEngine::init_vulkan()
{
    //创建Vulkan实例
    vkb::InstanceBuilder builder;
    builder.set_app_name("Example Vulkan Application");
    builder.request_validation_layers(bUseValidationLayers);
    builder.use_default_debug_messenger();
    builder.require_api_version(1, 3, 0);
    auto inst_ret = builder.build();    
    vkb::Instance vkb_inst = inst_ret.value();
    _instance = vkb_inst.instance;
    _debug_messenger = vkb_inst.debug_messenger;

    //创建窗口Surface，需要在实例创建之后立即创建，因为它会影响物理设备的选择
    SDL_Vulkan_CreateSurface(_window, _instance, &_surface);
    
    VkPhysicalDeviceVulkan13Features features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    features.dynamicRendering = true;
    features.synchronization2 = true;
    VkPhysicalDeviceVulkan12Features features12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;

	//查找支持所需Feature的物理设备并获取其句柄
    vkb::PhysicalDeviceSelector selector{ vkb_inst };
    selector.set_minimum_version(1, 3);
    selector.set_required_features_13(features);
    selector.set_required_features_12(features12);
    selector.set_surface(_surface);
    vkb::PhysicalDevice physicalDevice = selector.select().value();
    vkb::DeviceBuilder deviceBuilder{ physicalDevice };
    vkb::Device vkbDevice = deviceBuilder.build().value();
    _device = vkbDevice.device;
    _chosenGPU = physicalDevice.physical_device;

    //获取图形类型队列及队列族索引
    _graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    _graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
}

void VulkanEngine::init_swapchain()
{
    create_swapchain(_windowExtent.width, _windowExtent.height);
}

void VulkanEngine::create_swapchain(uint32_t width, uint32_t height)
{
    vkb::SwapchainBuilder swapchainBuilder{ _chosenGPU,_device,_surface };
    _swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    swapchainBuilder.set_desired_format(VkSurfaceFormatKHR{ .format = _swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR });
    swapchainBuilder.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR);
    swapchainBuilder.set_desired_extent(width, height);
    swapchainBuilder.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    vkb::Swapchain vkbSwapchain = swapchainBuilder.build().value();
    
    _swapchainExtent = vkbSwapchain.extent;
    _swapchain = vkbSwapchain.swapchain;
    _swapchainImages = vkbSwapchain.get_images().value();
    _swapchainImageViews = vkbSwapchain.get_image_views().value();
}

void VulkanEngine::destroy_swapchain()
{
    vkDestroySwapchainKHR(_device, _swapchain, nullptr);

    for (int i = 0; i < _swapchainImageViews.size(); i++) {

        vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
    }
}

void VulkanEngine::init_commands()
{
    VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (int i = 0; i < FRAME_OVERLAP; i++) {

        VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_frames[i]._commandPool));

        VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1);

        VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_frames[i]._mainCommandBuffer));
    }
}

void VulkanEngine::init_sync_structures()
{
    
}

void VulkanEngine::cleanup()
{
    if (_isInitialized) 
    {
		vkDeviceWaitIdle(_device);//确保GPU已完成所有操作

        for (int i = 0; i < FRAME_OVERLAP; i++) {
            vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);
        }

        destroy_swapchain();

        vkDestroySurfaceKHR(_instance, _surface, nullptr);
        vkDestroyDevice(_device, nullptr);

        vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
        vkDestroyInstance(_instance, nullptr);
        SDL_DestroyWindow(_window);
    }

    loadedEngine = nullptr;
}

void VulkanEngine::draw()
{
    
}

void VulkanEngine::run()
{
    SDL_Event e;
    bool bQuit = false;
        
    while (!bQuit) 
    {
        while (SDL_PollEvent(&e) != 0) 
        {
            if (e.type == SDL_QUIT)
                bQuit = true;

            if (e.type == SDL_WINDOWEVENT) 
            {
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED)//窗口最小化时触发 
                {
                    stop_rendering = true;
                }
				if (e.window.event == SDL_WINDOWEVENT_RESTORED)//窗口恢复时触发
                {
                    stop_rendering = false;
                }
            }
        }

        if (stop_rendering) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        draw();
    }
}