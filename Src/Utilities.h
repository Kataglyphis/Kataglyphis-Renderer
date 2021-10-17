#pragma once

#include <fstream>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

const int MAX_FRAME_DRAWS = 2;
const int MAX_OBJECTS = 20;

const std::vector<const char*> device_extensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// vertex data representation (layout)
struct Vertex {

	glm::vec3 pos; 
	alignas(16) glm::vec2 texture_coords; 
	glm::vec3 normal;

};

// Indices (locations) of Queue families (if they exist at all)
struct QueueFamilyIndices {

	int graphics_family = -1;																// location of graphics family
	int presentation_family = -1;														// location of presentation queue family

	//check if queue families are valid 
	bool is_valid() {

		return graphics_family >= 0 && presentation_family >= 0;

	}

};

struct SwapChainDetails {

	VkSurfaceCapabilitiesKHR surface_capabilities;				// surface properties, e.g. image size/extent
	std::vector<VkSurfaceFormatKHR> formats;					// surface image formats, e.g. RGBA and size of each color
	std::vector<VkPresentModeKHR> presentation_mode;  // how images should be presented to screen

};

struct SwapChainImage {

	VkImage image;
	VkImageView image_view;

};

static std::vector<char> read_file(const std::string& filename) {

	// open stream from given file 
	// std::ios::binary tells stream to read file as binary
	// std::ios:ate tells stream to start reading from end of file
	std::ifstream file(filename, std::ios::binary | std::ios::ate);

	// check if file stream sucessfully opened
	if (!file.is_open()) {

		throw std::runtime_error("Failed to open a file!");

	}

	size_t file_size = (size_t) file.tellg();
	std::vector<char> file_buffer(file_size);

	// move read position to start of file 
	file.seekg(0);

	// read the file data into the buffer (stream "file_size" in total)
	file.read(file_buffer.data(), file_size);

	file.close();

	return file_buffer;
}

static uint32_t find_memory_type_index(VkPhysicalDevice physical_device, uint32_t allowed_types, VkMemoryPropertyFlags properties)
{

	// get properties of physical device memory
	VkPhysicalDeviceMemoryProperties memory_properties{};
	vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

	for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {

		if ((allowed_types & (1 << i))																																// index of memory type must match corresponding bit in allowedTypes
			&& (memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {					// desired property bit flags are part of memory type's property flags																			

			// this memory type is valid, so return its index
			return i;

		}

	}

	return -1;

}

static void create_buffer(VkPhysicalDevice physical_device, VkDevice device, VkDeviceSize buffer_size, VkBufferUsageFlags buffer_usage_flags, 
											VkMemoryPropertyFlags buffer_propertiy_flags, VkBuffer* buffer, VkDeviceMemory* buffer_memory) {

	// create vertex buffer
	// information to create a buffer (doesn't include assigning memory)
	VkBufferCreateInfo buffer_info{};
	buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_info.size = buffer_size;																			// size of buffer (size of 1 vertex * #vertices)
	buffer_info.usage = buffer_usage_flags;															// multiple types of buffer possible, e.g. vertex buffer		
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;						// similar to swap chain images, can share vertex buffers

	VkResult result = vkCreateBuffer(device, &buffer_info, nullptr, buffer);

	if (result != VK_SUCCESS) {

		throw std::runtime_error("Failed to create a buffer!");

	}

	// get buffer memory requirements
	VkMemoryRequirements memory_requirements{};
	vkGetBufferMemoryRequirements(device, *buffer, &memory_requirements);

	// allocate memory to buffer
	VkMemoryAllocateInfo memory_alloc_info{};
	memory_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memory_alloc_info.allocationSize = memory_requirements.size;

	uint32_t memory_type_index = find_memory_type_index(physical_device, memory_requirements.memoryTypeBits,
																											buffer_propertiy_flags);
																											//VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |		/* memory is visible to CPU side */
																											//VK_MEMORY_PROPERTY_HOST_COHERENT_BIT	/* data is placed straight into buffer */);
	if (memory_type_index < 0) {

		throw std::runtime_error("Failed to find auitable memory type!");

	}

	memory_alloc_info.memoryTypeIndex = memory_type_index;

	// allocate memory to VkDeviceMemory
	result = vkAllocateMemory(device, &memory_alloc_info, nullptr, buffer_memory);

	if (result != VK_SUCCESS) {

		throw std::runtime_error("Failed to allocate memory for vertex buffer!");

	}

	// allocate memory to given vertex buffer
	vkBindBufferMemory(device, *buffer, *buffer_memory, 0);

}

static VkCommandBuffer begin_command_buffer(VkDevice device, VkCommandPool command_pool) {

	// command buffer to hold transfer commands
	VkCommandBuffer command_buffer;

	// command buffer details
	VkCommandBufferAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandPool = command_pool;
	alloc_info.commandBufferCount = 1;

	// allocate command buffer from pool
	vkAllocateCommandBuffers(device, &alloc_info, & command_buffer);

	// infromation to begin the command buffer record
	VkCommandBufferBeginInfo begin_info{};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;											// we are only using the command buffer once, so set up for one time submit
	
	// begin recording transfer commands
	vkBeginCommandBuffer(command_buffer, &begin_info);

	return command_buffer;

}

static void end_and_submit_command_buffer(VkDevice device, VkCommandPool command_pool, VkQueue queue, VkCommandBuffer command_buffer) {

	// end commands
	vkEndCommandBuffer(command_buffer);

	// queue submission information 
	VkSubmitInfo submit_info{};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffer;

	// submit transfer command to transfer queue and wait until it finishes
	vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
	vkQueueWaitIdle(queue);

	// free temporary command buffer back to pool
	vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);

}

static void copy_buffer(VkDevice device, VkQueue transfer_queue, VkCommandPool transfer_command_pool,
											VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize buffer_size) {

	// create buffer
	VkCommandBuffer command_buffer = begin_command_buffer(device, transfer_command_pool);

	// region of data to copy from and to
	VkBufferCopy buffer_copy_region{};
	buffer_copy_region.srcOffset = 0;
	buffer_copy_region.dstOffset = 0;
	buffer_copy_region.size = buffer_size;
	
	// command to copy src buffer to dst buffer
	vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &buffer_copy_region);

	end_and_submit_command_buffer(device, transfer_command_pool, transfer_queue, command_buffer);

}

static void copy_image_buffer(VkDevice device, VkQueue transfer_queue, VkCommandPool transfer_command_pool, 
													VkBuffer src_buffer, VkImage image, uint32_t width, uint32_t height) {

	// create buffer
	VkCommandBuffer transfer_command_buffer = begin_command_buffer(device, transfer_command_pool);

	VkBufferImageCopy image_region{};
	image_region.bufferOffset = 0;																						// offset into data
	image_region.bufferRowLength = 0;																				// row length of data to calculate data spacing
	image_region.bufferImageHeight = 0;																			// image height to calculate data spacing
	image_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;		// which aspect of image to copy
	image_region.imageSubresource.mipLevel = 0;
	image_region.imageSubresource.baseArrayLayer = 0;
	image_region.imageSubresource.layerCount = 1;
	image_region.imageOffset = {0, 0, 0};																			// offset into image 
	image_region.imageExtent = {width, height, 1};

	// copy buffer to given image
	vkCmdCopyBufferToImage(transfer_command_buffer, src_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_region);

	end_and_submit_command_buffer(device, transfer_command_pool, transfer_queue, transfer_command_buffer);

}

static void transition_image_layout(VkDevice device, VkQueue queue, VkCommandPool command_pool, VkImage image, VkImageLayout old_layout,
															VkImageLayout new_layout, uint32_t mip_levels) {

	VkCommandBuffer command_buffer = begin_command_buffer(device, command_pool);

	VkImageMemoryBarrier memory_barrier{};
	memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	memory_barrier.oldLayout = old_layout;
	memory_barrier.newLayout = new_layout;
	memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;							// Queue family to transition from 
	memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;							// Queue family to transition to
	memory_barrier.image = image;																							// image being accessed and modified as part of barrier
	memory_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;			// aspect of image being altered
	memory_barrier.subresourceRange.baseMipLevel = 0;															// first mip level to start alterations on
	memory_barrier.subresourceRange.levelCount = mip_levels;																// number of mip levels to alter starting from baseMipLevel
	memory_barrier.subresourceRange.baseArrayLayer = 0;														// first layer to start alterations on
	memory_barrier.subresourceRange.layerCount = 1;																// number of layers to alter starting from baseArrayLayer

	VkPipelineStageFlags src_stage;
	VkPipelineStageFlags dst_stage;

	// if transitioning from new image to image ready to receive data
	if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {

		memory_barrier.srcAccessMask = 0;																					// memory access stage transition must after ...
		memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;						// memory access stage transition must before ...

		src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;

	}
	else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {

		memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;						// memory access stage transition must after ...
		memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;								// memory access stage transition must before ...

		src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

	}

	vkCmdPipelineBarrier(
	
		command_buffer,
		src_stage, dst_stage,				// pipeline stages (match to src and dst accessmask)
		0,												// no dependency flags
		0, nullptr,									// memory barrier count + data
		0, nullptr,									// buffer memory barrier count + data
		1, &memory_barrier				// image memory barrier count + data

	);

	end_and_submit_command_buffer(device, command_pool, queue, command_buffer);

}

// we have to create mipmap levels in staging buffers by our own
static void generate_mipmaps(VkPhysicalDevice physical_device, VkDevice device, VkCommandPool command_pool,
														VkQueue queue, VkImage image, VkFormat image_format,
														int32_t width, int32_t height, uint32_t mip_levels) {

	// Check if image format supports linear blitting
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(physical_device, image_format, &formatProperties);

	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
		throw std::runtime_error("texture image format does not support linear blitting!");
	}

	VkCommandBuffer command_buffer = begin_command_buffer(device, command_pool);

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = image;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;
	
	// TEMP VARS needed for decreasing step by step for factor 2
	int32_t tmp_width = width;
	int32_t tmp_height = height;

	// -- WE START AT 1 ! 
	for (uint32_t i = 1; i < mip_levels; i++) {

		// WAIT for previous mip map level for being ready
		barrier.subresourceRange.baseMipLevel = i - 1;
		// HERE we TRANSITION for having a SRC format now 
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(command_buffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);
		
		// when barrier over we can now blit :)
		VkImageBlit blit{};

		// -- OFFSETS describing the 3D-dimesnion of the region 
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { tmp_width, tmp_height, 1 };
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		// copy from previous level
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		// -- OFFSETS describing the 3D-dimesnion of the region 
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { tmp_width > 1 ? tmp_width / 2 : 1, tmp_height > 1 ? tmp_height / 2 : 1, 1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		// -- COPY to next mipmap level
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;

		vkCmdBlitImage(command_buffer,
			image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit,
			VK_FILTER_LINEAR);

		// REARRANGE image formats for having the correct image formats again
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(command_buffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		if (tmp_width > 1) tmp_width /= 2;
		if (tmp_height > 1) tmp_height /= 2;

	}

	barrier.subresourceRange.baseMipLevel = mip_levels - 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(command_buffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
		0, nullptr,
		0, nullptr,
		1, &barrier);

	end_and_submit_command_buffer(device, command_pool, queue, command_buffer);

}