
#pragma once 

namespace vkutil {
	//用于转换图像的布局
	void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);


};