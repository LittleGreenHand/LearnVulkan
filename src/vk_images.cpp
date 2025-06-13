#include <vk_images.h>

#include <vk_initializers.h>

void vkutil::transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout)
{
    //VkImageMemoryBarrier2 ��Vulkan1.3�������չ��VK_KHR_synchronization2���ж����ͬ��ԭ����ڸ���ϸ�ؿ���ͼ���ڴ����˳��Ͳ���ת��
    VkImageMemoryBarrier2 imageBarrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    imageBarrier.pNext = nullptr;

    //VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT ��һ������Ĺ��߽׶α�־�����ڱ�ʾ���п��ܵĹ��߽׶�
    imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;//ָ����Щ���߽׶α���������ǰ���
    imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;//ȷ���ڴ�д��������ǰ���
    imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;//ָ�����Ϻ��������ִ�еĹ��߽׶�
    imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;//ָ�����Ϻ��������Դ���еľ�����ʲ����������д����ȷ������һ����

    imageBarrier.oldLayout = currentLayout;
    imageBarrier.newLayout = newLayout;

    VkImageAspectFlags aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    //ָ��ͼ�����Щ����Դ��Mip �㼶������㣩������Ӱ�죬Ҫ������СsubresourceRange�ķ�Χ������ȫ�����ϣ�
    imageBarrier.subresourceRange = vkinit::image_subresource_range(aspectMask); 

    imageBarrier.image = image;

    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.pNext = nullptr;

    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &imageBarrier;

    //����������в�����ʽ���ϣ�����˵����������м�¼����ָ�ʵ�����ϵ�ִ�з�����GPU���и��������ʱ���ύ�����к�
    //vkCmdPipelineBarrier2ǿ��GPU������ǰ���srcStageMaskָ�������в�����������dstStageMask֮ǰ�Ĳ�����ֱ���������
    //����ʹ�����ϻ��ƻ� GPU �����ԣ�Ӧ�����ϲ�������ϵ�һ�ε���
    vkCmdPipelineBarrier2(cmd, &depInfo);
}