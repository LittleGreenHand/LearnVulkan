
#pragma once 

namespace vkutil {
	//����ת��ͼ��Ĳ���
	void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);


};