#include <vk_images.h>

#include <vk_initializers.h>

void vkutil::transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout)
{
    //VkImageMemoryBarrier2 是Vulkan1.3引入的扩展（VK_KHR_synchronization2）中定义的同步原语，用于更精细地控制图像内存访问顺序和布局转换
    VkImageMemoryBarrier2 imageBarrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    imageBarrier.pNext = nullptr;

    //VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT 是一个特殊的管线阶段标志，用于表示所有可能的管线阶段
    imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;//指定哪些管线阶段必须在屏障前完成
    imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;//确保内存写入在屏障前完成
    imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;//指定屏障后允许继续执行的管线阶段
    imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;//指定屏障后允许对资源进行的具体访问操作（如读、写），确保数据一致性

    imageBarrier.oldLayout = currentLayout;
    imageBarrier.newLayout = newLayout;

    VkImageAspectFlags aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    //指定图像的哪些子资源（Mip 层级、数组层）受屏障影响，要尽量缩小subresourceRange的范围（避免全局屏障）
    imageBarrier.subresourceRange = vkinit::image_subresource_range(aspectMask); 

    imageBarrier.image = image;

    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.pNext = nullptr;

    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &imageBarrier;

    //在命令缓冲区中插入显式屏障，或者说在命令缓冲区中记录屏障指令，实际屏障的执行发生在GPU运行该命令缓冲区时（提交到队列后）
    //vkCmdPipelineBarrier2强制GPU在屏障前完成srcStageMask指定的所有操作，并阻塞dstStageMask之前的操作，直到屏障完成
    //过度使用屏障会破坏 GPU 并行性，应尽量合并多个屏障到一次调用
    vkCmdPipelineBarrier2(cmd, &depInfo);
}