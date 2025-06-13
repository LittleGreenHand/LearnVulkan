//> includes
#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_initializers.h>
#include <vk_types.h>
#include <vk_images.h>

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
    VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);//设置Fence的初始状态为已触发
    VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

    for (int i = 0; i < FRAME_OVERLAP; i++) {
        VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i]._renderFence));

        VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._swapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._renderSemaphore));
    }
}

void VulkanEngine::cleanup()
{
    if (_isInitialized) 
    {
		vkDeviceWaitIdle(_device);//确保GPU已完成所有操作
        for (int i = 0; i < FRAME_OVERLAP; i++) {
            vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);
            vkDestroyFence(_device, _frames[i]._renderFence, nullptr);
            vkDestroySemaphore(_device, _frames[i]._renderSemaphore, nullptr);
            vkDestroySemaphore(_device, _frames[i]._swapchainSemaphore, nullptr);
        }
        destroy_swapchain();
        vkDestroySurfaceKHR(_instance, _surface, nullptr);
        vkDestroyDevice(_device, nullptr);
        vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
        vkDestroyInstance(_instance, nullptr);
        SDL_DestroyWindow(_window);
    }
}

void VulkanEngine::draw()
{
    //等待GPU完成任务
    VK_CHECK(vkWaitForFences(_device, 1, &get_current_frame()._renderFence, true, 1000000000));
    VK_CHECK(vkResetFences(_device, 1, &get_current_frame()._renderFence));

    uint32_t swapchainImageIndex;
    //从交换链请求图像索引，如果交换链没有任何可以使用的图像，vkAcquireNextImageKHR将阻塞线程，最大超时时间为 1 秒
    VK_CHECK(vkAcquireNextImageKHR(_device, _swapchain, 1000000000, get_current_frame()._swapchainSemaphore, nullptr, &swapchainImageIndex));

    VkCommandBuffer cmd = get_current_frame()._mainCommandBuffer;

    //重置命令缓冲区
    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    /*VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT表示命令缓冲区仅用于单次提交，允许驱动程序进行潜在优化（如减少内部状态跟踪）
    提交后，命令缓冲区不能再被录制或提交（除非先重置）*/
    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    //在渲染之前把交换链图像布局转换为通用布局，以便执行渲染操作
    //VK_IMAGE_LAYOUT_UNDEFINED：表示图像的初始布局，通常用于新创建的图像或不关心原有内容的情况，不保证图像数据的有效性（可能是垃圾数据），但允许驱动程序进行优化（如避免冗余内存拷贝）
    //VK_IMAGE_LAYOUT_GENERAL：通用布局，支持所有操作（采样、存储图像、颜色附件等），但性能通常不是最优。当其他专用布局（如 COLOR_ATTACHMENT_OPTIMAL）不适用时使用
    vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    VkImageSubresourceRange clearRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

    //以120帧为周期构建具有闪烁效果的颜色
    VkClearColorValue clearValue;
    float flash = std::abs(std::sin(_frameNumber / 120.f));
    clearValue = { { 0.0f, 0.0f, flash, 1.0f } };

    //将交换链图像初始化为指定颜色
    vkCmdClearColorImage(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);

    //把交换链图像布局转换为可呈现状态
    //VK_IMAGE_LAYOUT_PRESENT_SRC_KHR：专用于交换链图像，表示图像已准备好呈现到屏幕，通常由Vulkan自动管理，开发者只需在渲染完成后将图像转换到此布局
    vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);

    //定义等待信号量，命令缓冲区将在颜色附件输出阶段（COLOR_ATTACHMENT_OUTPUT）暂停，直到 _swapchainSemaphore 变为 Signaled 状态（通常由 vkAcquireNextImageKHR 触发）
    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, get_current_frame()._swapchainSemaphore);
    //定义触发信号量，命令缓冲区在所有图形管线阶段（ALL_GRAPHICS）完成后，将 _renderSemaphore 设置为 Signaled 状态（通常用于通知present或下一帧的依赖）
    VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame()._renderSemaphore);
    //把command buffer提交到队列并执行
    VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);
    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, get_current_frame()._renderFence));

    
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.pSwapchains = &_swapchain;
    presentInfo.swapchainCount = 1;
    presentInfo.pWaitSemaphores = &get_current_frame()._renderSemaphore;//指定呈现操作需要等待的信号量（确保图像已完成渲染）
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pImageIndices = &swapchainImageIndex; //指定要呈现的交换链图像索引（由vkAcquireNextImageKHR获取）

    // 提交present请求到图形队列
    // 注意：vkQueuePresentKHR会隐式触发交换链图像的布局转换（从COLOR_ATTACHMENT_OPTIMAL→PRESENT_SRC）
    VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));

    _frameNumber++;
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