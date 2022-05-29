#include "VulkanRenderer.hpp"

#include <iostream>
#include <vector>
#include <algorithm>

#include "File.h"

#ifndef VMA_IMPLEMENTATION
#define VMA_IMPLEMENTATION
#endif // !VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include <ShaderHelper.h>
#include <PushConstantPost.h>

VulkanRenderer::VulkanRenderer(	Window* window, 
								Scene*	scene,
								GUI*	gui,
								Camera* camera) :

									window(window),
									scene(scene),
									gui(gui)

{

	update_uniforms(scene, camera, window);

	try {

		instance		= VulkanInstance();

		create_surface();

		device			= std::make_unique<VulkanDevice>(&instance,
												&surface);

		allocator		= Allocator(device->getLogicalDevice(), 
									device->getPhysicalDevice(), 
									instance.getVulkanInstance());

		create_command_pool();

		vulkanSwapChain.initVulkanContext(	device.get(),
											window,
											surface);

		create_uniform_buffers();
		create_command_buffers();

		create_descriptor_set_layouts();
		std::vector<VkDescriptorSetLayout> descriptor_set_layouts_rasterizer = { descriptor_set_layout, sampler_set_layout };
		rasterizer.init(device.get(), &vulkanSwapChain, descriptor_set_layouts_rasterizer, graphics_command_pool);
		create_post_descriptor();
		std::vector<VkDescriptorSetLayout> descriptor_set_layouts_post = { post_descriptor_set_layout };
		postStage.init(device.get(), &vulkanSwapChain, descriptor_set_layouts_post);
		createDescriptors();
		update_post_descriptor_set();

		std::stringstream modelFile;
		modelFile << CMAKELISTS_DIR;
		modelFile << "/Resources/Model/crytek-sponza/";
		modelFile << "sponza_triag.obj";

		//std::string modelFile = "../Resources/Model/crytek-sponza/sponza_triag.obj";
		//std::string modelFile = "../Resources/Model/Dinosaurs/dinosaurs.obj";
		//std::string modelFile = "../Resources/Model/Pillum/PilumPainting_export.obj";
		//std::string modelFile = "../Resources/Model/sibenik/sibenik.obj";
		//std::string modelFile = "../Resources/Model/sportsCar/sportsCar.obj";
		//std::string modelFile = "../Resources/Model/StanfordDragon/dragon.obj";
		//std::string modelFile = "../Resources/Model/CornellBox/CornellBox-Sphere.obj";
		//std::string modelFile = "../Resources/Model/bunny/bunny.obj";
		//std::string modelFile = "../Resources/Model/buddha/buddha.obj";
		//std::string modelFile = "../Resources/Model/bmw/bmw.obj";
		//std::string modelFile = "../Resources/Model/testScene.obj";
		//std::string modelFile = "../Resources/Model/San_Miguel/san-miguel-low-poly.obj";
		scene->loadModel(device.get(), compute_command_pool, modelFile.str());

		create_sampler_array_descriptor_set();

		glm::mat4 dragon_model(1.0f);
		//dragon_model = glm::translate(dragon_model, glm::vec3(0.0f, -40.0f, -50.0f));
		dragon_model = glm::scale(dragon_model, glm::vec3(1000.0f, 1000.0f, 1000.0f));
		/*dragon_model = glm::rotate(dragon_model, glm::radians(-90.f), glm::vec3(1.0f, 0.0f, 0.0f));
		dragon_model = glm::rotate(dragon_model, glm::radians(angle), glm::vec3(0.0f, 0.0f, 1.0f));*/
		scene->update_model_matrix(dragon_model, 0);

		asManager.createASForScene(device.get(), compute_command_pool, scene, tlas, blas);

		create_raytracing_descriptor_pool();
		create_object_description_buffer();
		create_raytracing_descriptor_set_layouts();
		create_raytracing_descriptor_sets();

		std::vector<VkDescriptorSetLayout> layouts;
		layouts.push_back(descriptor_set_layout);
		layouts.push_back(raytracing_descriptor_set_layout);

		raytracingStage.init(device.get(), &vulkanSwapChain, layouts);

		create_synchronization();

		gui->initializeVulkanContext(	device.get(),
										instance.getVulkanInstance(),
										postStage.getRenderPass(),
										graphics_command_pool);

	}
	catch (const std::runtime_error& e) {

		printf("ERROR: %s\n", e.what());

	}


}

void VulkanRenderer::update_uniforms(	Scene* scene, 
										Camera* camera,
										Window* window)
{

	const GUISceneSharedVars guiSceneSharedVars = scene->getGuiSceneSharedVars();


	globalUBO.view						= camera->calculate_viewmatrix();
	globalUBO.projection				= glm::perspective(	glm::radians(camera->get_fov()),
															(float)window->get_width() / (float)window->get_height(),
															camera->get_near_plane(),
															camera->get_far_plane());

	sceneUBO.view_dir					= glm::vec4(camera->get_camera_direction(),1.0f);

	sceneUBO.light_dir					= glm::vec4(guiSceneSharedVars.directional_light_direction[0],
															guiSceneSharedVars.directional_light_direction[1],
															guiSceneSharedVars.directional_light_direction[2], 
															1.0f);

	sceneUBO.cam_pos					= glm::vec4(camera->get_camera_position(),1.0f);

}

void VulkanRenderer::updateStateDueToUserInput(GUI* gui)
{

	this->guiRendererSharedVars	= gui->getGuiRendererSharedVars();

	if (guiRendererSharedVars.shader_hot_reload_triggered) {
		hot_reload_all_shader();
	}

}

void VulkanRenderer::hot_reload_all_shader()
{

	// wait until no actions being run on device before destroying
	vkDeviceWaitIdle(device->getLogicalDevice());

	rasterizer.cleanUp();
	std::vector<VkDescriptorSetLayout> descriptor_set_layouts = { descriptor_set_layout, sampler_set_layout };
	rasterizer.init(device.get(), &vulkanSwapChain, descriptor_set_layouts, graphics_command_pool);

	postStage.cleanUp();
	std::vector<VkDescriptorSetLayout> descriptor_set_layouts_post = { post_descriptor_set_layout };
	postStage.init(device.get(), &vulkanSwapChain, descriptor_set_layouts_post);

	raytracingStage.cleanUp();
	std::vector<VkDescriptorSetLayout> layouts;
	layouts.push_back(descriptor_set_layout);
	layouts.push_back(raytracing_descriptor_set_layout);
	raytracingStage.init(device.get(), &vulkanSwapChain, layouts);

}

void VulkanRenderer::drawFrame()
{
	check_changed_framebuffer_size();

	 /*1. Get next available image to draw to and set something to signal when we're finished with the image  (a semaphore)
	 wait for given fence to signal (open) from last draw before continuing*/
	VkResult result = vkWaitForFences(device->getLogicalDevice(), 1, &in_flight_fences[current_frame], VK_TRUE, std::numeric_limits<uint64_t>::max());
	ASSERT_VULKAN(result, "Failed to wait for fences!")

	// -- GET NEXT IMAGE --
	uint32_t image_index;
	result = vkAcquireNextImageKHR(	device->getLogicalDevice(), vulkanSwapChain.getSwapChain()/*swapchain*/,
									std::numeric_limits<uint64_t>::max(), 
									image_available[current_frame], VK_NULL_HANDLE, &image_index);

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {

		recreate_swap_chain();
		return;

	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {

		throw std::runtime_error("Failed to acquire next image!");

	}

	//// check if previous frame is using this image (i.e. there is its fence to wait on)
	if (images_in_flight_fences[image_index] != VK_NULL_HANDLE) {
		vkWaitForFences(device->getLogicalDevice(), 1, &images_in_flight_fences[image_index], VK_TRUE, UINT64_MAX);
	}

	 //mark the image as now being in use by this frame
	images_in_flight_fences[image_index] = in_flight_fences[current_frame];

	VkCommandBufferBeginInfo buffer_begin_info{};
	buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	// start recording commands to command buffer
	result = vkBeginCommandBuffer(command_buffers[image_index], &buffer_begin_info);
	ASSERT_VULKAN(result, "Failed to start recording a command buffer!")

	update_uniform_buffers(image_index);

	if(guiRendererSharedVars.raytracing) update_raytracing_descriptor_set(image_index);

	record_commands(image_index);

	// stop recording to command buffer
	result = vkEndCommandBuffer(command_buffers[image_index]);
	ASSERT_VULKAN(result, "Failed to stop recording a command buffer!")

	// 2. Submit command buffer to queue for execution, making sure it waits for the image to be signalled as available before drawing
	// and signals when it has finished rendering 
	// -- SUBMIT COMMAND BUFFER TO RENDER --
	VkSubmitInfo submit_info{};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.waitSemaphoreCount = 1;													// number of semaphores to wait on
	submit_info.pWaitSemaphores = &image_available[current_frame];						// list of semaphores to wait on
	
	VkPipelineStageFlags wait_stages = {
	
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT /*|
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT |
		VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR*/

	};

	submit_info.pWaitDstStageMask = &wait_stages;										// stages to check semaphores at
	
	submit_info.commandBufferCount = 1;													// number of command buffers to submit
	submit_info.pCommandBuffers = &command_buffers[image_index];						// command buffer to submit
	submit_info.signalSemaphoreCount = 1;												// number of semaphores to signal
	submit_info.pSignalSemaphores = &render_finished[current_frame];					// semaphores to signal when command buffer finishes 

	result = vkResetFences(device->getLogicalDevice(), 1, &in_flight_fences[current_frame]);
	ASSERT_VULKAN(result, "Failed to reset fences!")

	// submit command buffer to queue
	result = vkQueueSubmit(device->getGraphicsQueue(), 1, &submit_info, in_flight_fences[current_frame]);
	ASSERT_VULKAN(result, "Failed to submit command buffer to queue!")

	// 3. Present image to screen when it has signalled finished rendering 
	// -- PRESENT RENDERED IMAGE TO SCREEN --
  	VkPresentInfoKHR present_info{};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1;												// number of semaphores to wait on
	present_info.pWaitSemaphores = &render_finished[current_frame];						// semaphores to wait on
	present_info.swapchainCount = 1;													// number of swapchains to present to
	const VkSwapchainKHR swapchain = vulkanSwapChain.getSwapChain();
	present_info.pSwapchains = &swapchain;												// swapchains to present images to 
	present_info.pImageIndices = &image_index;											// index of images in swapchain to present

	result = vkQueuePresentKHR(device->getPresentationQueue(), &present_info);

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {

		recreate_swap_chain();
		return;

	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {

		throw std::runtime_error("Failed to acquire next image!");

	}

	if (result != VK_SUCCESS) {

		throw std::runtime_error("Failed to submit to present queue!");

	}

	current_frame = (current_frame + 1) % MAX_FRAME_DRAWS;

}

void VulkanRenderer::create_surface()
{
	// create surface (creates a surface create info struct, runs the create surface function, returns result)
	ASSERT_VULKAN(glfwCreateWindowSurface(instance.getVulkanInstance(), window->get_window(), nullptr, &surface), "Failed to create a surface!");

}

void VulkanRenderer::create_post_descriptor()
{

	// UNIFORM VALUES DESCRIPTOR SET LAYOUT
	//globalUBO Binding info
	VkDescriptorSetLayoutBinding post_sampler_layout_binding{};
	post_sampler_layout_binding.binding = 0;													// binding point in shader (designated by binding number in shader)
	post_sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;		// type of descriptor (uniform, dynamic uniform, image sampler, etc)
	post_sampler_layout_binding.descriptorCount = 1;											// number of descriptors for binding
	post_sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;						// we need to say at which shader we bind this uniform to
	post_sampler_layout_binding.pImmutableSamplers = nullptr;									// for texture: can make sampler data unchangeable (immutable) by specifying in layout

	std::vector<VkDescriptorSetLayoutBinding> layout_bindings = { post_sampler_layout_binding };

	// create descriptor set layout with given bindings
	VkDescriptorSetLayoutCreateInfo layout_create_info{};
	layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_create_info.bindingCount = static_cast<uint32_t>(layout_bindings.size());			// only have 1 for the globalUBO
	layout_create_info.pBindings = layout_bindings.data();										// array of binding infos 

	// create descriptor set layout
	VkResult result = vkCreateDescriptorSetLayout(device->getLogicalDevice(), &layout_create_info, nullptr, &post_descriptor_set_layout);
	ASSERT_VULKAN(result, "Failed to create descriptor set layout!")

	VkDescriptorPoolSize post_pool_size{};
	post_pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	post_pool_size.descriptorCount = static_cast<uint32_t>(1);

	// list of pool sizes 
	std::vector<VkDescriptorPoolSize> descriptor_pool_sizes = { post_pool_size };

	VkDescriptorPoolCreateInfo pool_create_info{};
	pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_create_info.maxSets = vulkanSwapChain.getNumberSwapChainImages();							// maximum number of descriptor sets that can be created from pool
	pool_create_info.poolSizeCount = static_cast<uint32_t>(descriptor_pool_sizes.size());			// amount of pool sizes being passed
	pool_create_info.pPoolSizes = descriptor_pool_sizes.data();										// pool sizes to create pool with

	// create descriptor pool
	result = vkCreateDescriptorPool(device->getLogicalDevice(), &pool_create_info, nullptr, &post_descriptor_pool);
	ASSERT_VULKAN(result, "Failed to create a descriptor pool!")

	// resize descriptor set list so one for every buffer
	post_descriptor_set.resize(vulkanSwapChain.getNumberSwapChainImages());

	std::vector<VkDescriptorSetLayout> set_layouts(vulkanSwapChain.getNumberSwapChainImages(), post_descriptor_set_layout);

	// descriptor set allocation info
	VkDescriptorSetAllocateInfo set_alloc_info{};
	set_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	set_alloc_info.descriptorPool = post_descriptor_pool;											// pool to allocate descriptor set from
	set_alloc_info.descriptorSetCount = vulkanSwapChain.getNumberSwapChainImages();					// number of sets to allocate
	set_alloc_info.pSetLayouts = set_layouts.data();												// layouts to use to allocate sets (1:1 relationship)

	// allocate descriptor sets (multiple)
	result = vkAllocateDescriptorSets(device->getLogicalDevice(), &set_alloc_info, post_descriptor_set.data());
	ASSERT_VULKAN(result, "Failed to create descriptor sets!")

}

void VulkanRenderer::update_post_descriptor_set()
{

	// update all of descriptor set buffer bindings
	for (size_t i = 0; i < vulkanSwapChain.getNumberSwapChainImages(); i++) {

		// texture image info
		VkDescriptorImageInfo image_info{};
		image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		Texture& renderResult = rasterizer.getOffscreenTexture(i);
		image_info.imageView = renderResult.getImageView();
		image_info.sampler = texture_sampler;

		// descriptor write info
		VkWriteDescriptorSet descriptor_write{};
		descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptor_write.dstSet = post_descriptor_set[i];
		descriptor_write.dstBinding = 0;
		descriptor_write.dstArrayElement = 0;
		descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptor_write.descriptorCount = 1;
		descriptor_write.pImageInfo = &image_info;

		// update new descriptor set
		vkUpdateDescriptorSets(device->getLogicalDevice(), 1, &descriptor_write, 0, nullptr);

	}

}

void VulkanRenderer::create_raytracing_descriptor_pool()
{

	std::array<VkDescriptorPoolSize,4> descriptor_pool_sizes;

	descriptor_pool_sizes[0].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	descriptor_pool_sizes[0].descriptorCount = 1;

	descriptor_pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	descriptor_pool_sizes[1].descriptorCount = 1;

	descriptor_pool_sizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLER;
	descriptor_pool_sizes[2].descriptorCount = 2;

	descriptor_pool_sizes[3].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	descriptor_pool_sizes[3].descriptorCount = MAX_TEXTURE_COUNT;

	VkDescriptorPoolCreateInfo descriptor_pool_create_info{};
	descriptor_pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptor_pool_create_info.poolSizeCount = static_cast<uint32_t>(descriptor_pool_sizes.size());
	descriptor_pool_create_info.pPoolSizes = descriptor_pool_sizes.data();
	descriptor_pool_create_info.maxSets = vulkanSwapChain.getNumberSwapChainImages();

	VkResult result = vkCreateDescriptorPool(device->getLogicalDevice(), &descriptor_pool_create_info, nullptr, &raytracing_descriptor_pool);
	ASSERT_VULKAN(result, "Failed to create command pool!")

}

void VulkanRenderer::create_object_description_buffer()
{
	std::vector<ObjectDescription> objectDescriptions = scene->getObjectDescriptions();

	vulkanBufferManager.createBufferAndUploadVectorOnDevice(device.get(),
															compute_command_pool,
															objectDescriptionBuffer,
															VK_BUFFER_USAGE_TRANSFER_DST_BIT |
															VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
															VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
															VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
															VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
															objectDescriptions);

	// update the object description set
	// update all of descriptor set buffer bindings
	for (size_t i = 0; i < vulkanSwapChain.getNumberSwapChainImages(); i++) {

		VkDescriptorBufferInfo object_descriptions_buffer_info{};
		// image_info.sampler = VK_DESCRIPTOR_TYPE_SAMPLER;
		object_descriptions_buffer_info.buffer = objectDescriptionBuffer.getBuffer();
		object_descriptions_buffer_info.offset = 0;
		object_descriptions_buffer_info.range = VK_WHOLE_SIZE;

		VkWriteDescriptorSet descriptor_object_descriptions_writer{};
		descriptor_object_descriptions_writer.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptor_object_descriptions_writer.pNext = nullptr;
		descriptor_object_descriptions_writer.dstSet = descriptor_sets[i];
		descriptor_object_descriptions_writer.dstBinding = OBJECT_DESCRIPTION_BINDING;
		descriptor_object_descriptions_writer.dstArrayElement = 0;
		descriptor_object_descriptions_writer.descriptorCount = 1;
		descriptor_object_descriptions_writer.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptor_object_descriptions_writer.pImageInfo = nullptr;
		descriptor_object_descriptions_writer.pBufferInfo = &object_descriptions_buffer_info;
		descriptor_object_descriptions_writer.pTexelBufferView = nullptr;		// information about buffer data to bind

		std::vector<VkWriteDescriptorSet> write_descriptor_sets = { descriptor_object_descriptions_writer };

		// update the descriptor sets with new buffer/binding info
		vkUpdateDescriptorSets(device->getLogicalDevice(), static_cast<uint32_t>(write_descriptor_sets.size()),
									write_descriptor_sets.data(), 0, nullptr);
	}

}

void VulkanRenderer::create_raytracing_descriptor_set_layouts() {

	{
		std::array<VkDescriptorSetLayoutBinding, 4> descriptor_set_layout_bindings;

		// here comes the top level acceleration structure
		descriptor_set_layout_bindings[0].binding = TLAS_BINDING;
		descriptor_set_layout_bindings[0].descriptorCount = 1;
		descriptor_set_layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		descriptor_set_layout_bindings[0].pImmutableSamplers = nullptr;
		// load them into the raygeneration and chlosest hit shader
		descriptor_set_layout_bindings[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
													VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
		// here comes to previous rendered image
		descriptor_set_layout_bindings[1].binding = OUT_IMAGE_BINDING;
		descriptor_set_layout_bindings[1].descriptorCount = 1;
		descriptor_set_layout_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		descriptor_set_layout_bindings[1].pImmutableSamplers = nullptr;
		// load them into the raygeneration and chlosest hit shader
		descriptor_set_layout_bindings[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
														VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

		descriptor_set_layout_bindings[2].binding = TEXTURES_BINDING;
		descriptor_set_layout_bindings[2].descriptorCount = scene->getTextureCount(0);
		descriptor_set_layout_bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		descriptor_set_layout_bindings[2].pImmutableSamplers = nullptr;
		// load them into the raygeneration and chlosest hit shader
		descriptor_set_layout_bindings[2].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

		descriptor_set_layout_bindings[3].binding = SAMPLER_BINDING_RT;
		descriptor_set_layout_bindings[3].descriptorCount = 1;
		descriptor_set_layout_bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		descriptor_set_layout_bindings[3].pImmutableSamplers = nullptr;
		// load them into the raygeneration and chlosest hit shader
		descriptor_set_layout_bindings[3].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

		VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info{};
		descriptor_set_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptor_set_layout_create_info.bindingCount = static_cast<uint32_t>(descriptor_set_layout_bindings.size());
		descriptor_set_layout_create_info.pBindings = descriptor_set_layout_bindings.data();

		VkResult result = vkCreateDescriptorSetLayout(device->getLogicalDevice(), &descriptor_set_layout_create_info, nullptr, &raytracing_descriptor_set_layout);
		ASSERT_VULKAN(result, "Failed to create raytracing descriptor set layout!")

	}
	

}

void VulkanRenderer::create_raytracing_descriptor_sets()
{
	
	// resize descriptor set list so one for every buffer
	raytracing_descriptor_set.resize(vulkanSwapChain.getNumberSwapChainImages());

	std::vector<VkDescriptorSetLayout> set_layouts(vulkanSwapChain.getNumberSwapChainImages(), raytracing_descriptor_set_layout);

	VkDescriptorSetAllocateInfo descriptor_set_allocate_info{};
	descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;;
	descriptor_set_allocate_info.descriptorPool = raytracing_descriptor_pool;
	descriptor_set_allocate_info.descriptorSetCount = vulkanSwapChain.getNumberSwapChainImages();
	descriptor_set_allocate_info.pSetLayouts = set_layouts.data();

	VkResult result = vkAllocateDescriptorSets(device->getLogicalDevice(), &descriptor_set_allocate_info, raytracing_descriptor_set.data());
	ASSERT_VULKAN(result, "Failed to allocate raytracing descriptor set!")
		
	for (size_t i = 0; i < vulkanSwapChain.getNumberSwapChainImages(); i++) {

		VkWriteDescriptorSetAccelerationStructureKHR descriptor_set_acceleration_structure{};
		descriptor_set_acceleration_structure.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
		descriptor_set_acceleration_structure.pNext = nullptr;
		descriptor_set_acceleration_structure.accelerationStructureCount = 1;
		VkAccelerationStructureKHR& vulkanTLAS = tlas.getAS();
		descriptor_set_acceleration_structure.pAccelerationStructures = &vulkanTLAS;

		VkWriteDescriptorSet write_descriptor_set_acceleration_structure{};
		write_descriptor_set_acceleration_structure.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_descriptor_set_acceleration_structure.pNext = &descriptor_set_acceleration_structure;
		write_descriptor_set_acceleration_structure.dstSet = raytracing_descriptor_set[i];
		write_descriptor_set_acceleration_structure.dstBinding = TLAS_BINDING;
		write_descriptor_set_acceleration_structure.dstArrayElement = 0;
		write_descriptor_set_acceleration_structure.descriptorCount = 1;
		write_descriptor_set_acceleration_structure.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		write_descriptor_set_acceleration_structure.pImageInfo = nullptr;
		write_descriptor_set_acceleration_structure.pBufferInfo = nullptr;
		write_descriptor_set_acceleration_structure.pTexelBufferView = nullptr;

		VkDescriptorImageInfo image_info{};
		Texture& renderResult = rasterizer.getOffscreenTexture(i);
		image_info.imageView = renderResult.getImageView();
		image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		VkWriteDescriptorSet descriptor_image_writer{};
		descriptor_image_writer.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptor_image_writer.pNext = nullptr;
		descriptor_image_writer.dstSet = raytracing_descriptor_set[i];
		descriptor_image_writer.dstBinding = OUT_IMAGE_BINDING;
		descriptor_image_writer.dstArrayElement = 0;
		descriptor_image_writer.descriptorCount = 1;
		descriptor_image_writer.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		descriptor_image_writer.pImageInfo = &image_info;
		descriptor_image_writer.pBufferInfo = nullptr;
		descriptor_image_writer.pTexelBufferView = nullptr;

		// texture image info
		std::vector<Texture>& modelTextures = scene->getTextures(0);
		std::vector<VkDescriptorImageInfo> image_info_textures;
		image_info_textures.resize(scene->getTextureCount(0));
		for (int i = 0; i < scene->getTextureCount(0); i++) {
			image_info_textures[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			image_info_textures[i].imageView = modelTextures[i].getImageView();
			image_info_textures[i].sampler = nullptr;
		}

		// descriptor write info
		VkWriteDescriptorSet textures_descriptions_writer{};
		textures_descriptions_writer.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		textures_descriptions_writer.dstSet = raytracing_descriptor_set[i];
		textures_descriptions_writer.dstBinding = TEXTURES_BINDING;
		textures_descriptions_writer.dstArrayElement = 0;
		textures_descriptions_writer.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		textures_descriptions_writer.descriptorCount = static_cast<uint32_t>(image_info_textures.size());
		textures_descriptions_writer.pImageInfo = image_info_textures.data();

		VkDescriptorImageInfo sampler_info;
		sampler_info.imageView = nullptr;
		sampler_info.sampler = texture_sampler;

		// descriptor write info
		VkWriteDescriptorSet descriptor_write_sampler{};
		descriptor_write_sampler.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptor_write_sampler.dstSet = raytracing_descriptor_set[i];
		descriptor_write_sampler.dstBinding = SAMPLER_BINDING_RT;
		descriptor_write_sampler.dstArrayElement = 0;
		descriptor_write_sampler.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		descriptor_write_sampler.descriptorCount = 1;
		descriptor_write_sampler.pImageInfo = &sampler_info;

		std::vector<VkWriteDescriptorSet> write_descriptor_sets = { write_descriptor_set_acceleration_structure,
																	descriptor_image_writer,
																	textures_descriptions_writer, 
																	descriptor_write_sampler };

		// update the descriptor sets with new buffer/binding info
		vkUpdateDescriptorSets(device->getLogicalDevice(), static_cast<uint32_t>(write_descriptor_sets.size()),
			write_descriptor_sets.data(), 0, nullptr);

	}
	

}

void VulkanRenderer::create_descriptor_set_layouts()
{

	std::array<VkDescriptorSetLayoutBinding, 3> descriptor_set_layout_bindings;
	// UNIFORM VALUES DESCRIPTOR SET LAYOUT
	//globalUBO Binding info
	descriptor_set_layout_bindings[0].binding = globalUBO_BINDING;
	descriptor_set_layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptor_set_layout_bindings[0].descriptorCount = 1;																							
	descriptor_set_layout_bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT |
													VK_SHADER_STAGE_RAYGEN_BIT_KHR;												
	descriptor_set_layout_bindings[0].pImmutableSamplers = nullptr;																			

	// our model matrix which updates every frame for each object
	descriptor_set_layout_bindings[1].binding = sceneUBO_BINDING;
	descriptor_set_layout_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptor_set_layout_bindings[1].descriptorCount = 1;
	descriptor_set_layout_bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT |
										VK_SHADER_STAGE_FRAGMENT_BIT |
										VK_SHADER_STAGE_RAYGEN_BIT_KHR |
										VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	descriptor_set_layout_bindings[1].pImmutableSamplers = nullptr;

	descriptor_set_layout_bindings[2].binding = OBJECT_DESCRIPTION_BINDING;
	descriptor_set_layout_bindings[2].descriptorCount = 1;
	descriptor_set_layout_bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descriptor_set_layout_bindings[2].pImmutableSamplers = nullptr;
	// load them into the raygeneration and chlosest hit shader
	descriptor_set_layout_bindings[2].stageFlags =	VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT |
													VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

	// create descriptor set layout with given bindings
	VkDescriptorSetLayoutCreateInfo layout_create_info{};
	layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_create_info.bindingCount = static_cast<uint32_t>(descriptor_set_layout_bindings.size());								
	layout_create_info.pBindings = descriptor_set_layout_bindings.data();																				 

	// create descriptor set layout
	VkResult result = vkCreateDescriptorSetLayout(device->getLogicalDevice(), &layout_create_info, nullptr, &descriptor_set_layout);
	ASSERT_VULKAN(result, "Failed to create descriptor set layout!")

	// CREATE TEXTURE SAMPLER DESCRIPTOR SET LAYOUT
	// texture binding info
	VkDescriptorSetLayoutBinding sampler_layout_binding{};
	sampler_layout_binding.binding = SAMPLER_BINDING;
	sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	sampler_layout_binding.descriptorCount = 1;
	sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT |
										VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	sampler_layout_binding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding sampled_image_layout_binding{};
	sampled_image_layout_binding.binding = TEXTURES_BINDING;
	sampled_image_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	sampled_image_layout_binding.descriptorCount = MAX_TEXTURE_COUNT;
	sampled_image_layout_binding.stageFlags =	VK_SHADER_STAGE_FRAGMENT_BIT |
												VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	sampled_image_layout_binding.pImmutableSamplers = nullptr;

	std::vector<VkDescriptorSetLayoutBinding> texture_layout_bindings = {	sampler_layout_binding,
																			sampled_image_layout_binding };

	// create a descriptor set layout with given bindings for texture 
	VkDescriptorSetLayoutCreateInfo texture_layout_create_info{};
	texture_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	texture_layout_create_info.bindingCount = static_cast<uint32_t>(texture_layout_bindings.size());
	texture_layout_create_info.pBindings = texture_layout_bindings.data();

	// create descriptor set layout
	result = vkCreateDescriptorSetLayout(device->getLogicalDevice(), &texture_layout_create_info, nullptr, &sampler_set_layout);
	ASSERT_VULKAN(result, "Failed to create sampler set layout!")

}

void VulkanRenderer::create_command_pool()
{

	// get indices of queue familes from device
	QueueFamilyIndices queue_family_indices = device->getQueueFamilies();

	{
	
		VkCommandPoolCreateInfo pool_info{};
		pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;		// we are ready now to re-record our command buffers
		pool_info.queueFamilyIndex = queue_family_indices.graphics_family;		// queue family type that buffers from this command pool will use 

		// create a graphics queue family command pool
		VkResult result = vkCreateCommandPool(device->getLogicalDevice(), &pool_info, nullptr, &graphics_command_pool);
		ASSERT_VULKAN(result, "Failed to create command pool!")

	}

	{
	
		VkCommandPoolCreateInfo pool_info{};
		pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;	// we are ready now to re-record our command buffers
		pool_info.queueFamilyIndex = queue_family_indices.compute_family;	// queue family type that buffers from this command pool will use 

		// create a graphics queue family command pool
		VkResult result = vkCreateCommandPool(device->getLogicalDevice(), &pool_info, nullptr, &compute_command_pool);
		ASSERT_VULKAN(result, "Failed to create command pool!")

	}
}

void VulkanRenderer::cleanUpCommandPools()
{
	vkDestroyCommandPool(device->getLogicalDevice(), graphics_command_pool, nullptr);
	vkDestroyCommandPool(device->getLogicalDevice(), compute_command_pool, nullptr);
}

void VulkanRenderer::create_command_buffers()
{

	// resize command buffer count to have one for each framebuffer
	command_buffers.resize(vulkanSwapChain.getNumberSwapChainImages());

	VkCommandBufferAllocateInfo command_buffer_alloc_info{};
	command_buffer_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	command_buffer_alloc_info.commandPool = graphics_command_pool;
	command_buffer_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
																																													
																																								
	command_buffer_alloc_info.commandBufferCount = static_cast<uint32_t>(command_buffers.size());

	VkResult result = vkAllocateCommandBuffers(device->getLogicalDevice(), &command_buffer_alloc_info, command_buffers.data());
	ASSERT_VULKAN(result, "Failed to allocate command buffers!")

}

void VulkanRenderer::create_synchronization()
{

	image_available.resize(MAX_FRAME_DRAWS);
	render_finished.resize(MAX_FRAME_DRAWS);
	in_flight_fences.resize(MAX_FRAME_DRAWS);
	images_in_flight_fences.resize(vulkanSwapChain.getNumberSwapChainImages(), VK_NULL_HANDLE);

	// semaphore creation information
	VkSemaphoreCreateInfo semaphore_create_info{};
	semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	// fence creation information
	VkFenceCreateInfo fence_create_info{};
	fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (int i = 0; i < MAX_FRAME_DRAWS; i++) {

		if ((vkCreateSemaphore(device->getLogicalDevice(), &semaphore_create_info, nullptr, &image_available[i]) != VK_SUCCESS) ||
			(vkCreateSemaphore(device->getLogicalDevice(), &semaphore_create_info, nullptr, &render_finished[i]) != VK_SUCCESS) || 
			(vkCreateFence(device->getLogicalDevice(), &fence_create_info, nullptr, &in_flight_fences[i])		!= VK_SUCCESS)
			){

			throw std::runtime_error("Failed to create a semaphore and/or fence!");

		}

	}

}

void VulkanRenderer::createDescriptors()
{
	create_texture_sampler();
	create_descriptor_pool_uniforms();
	create_descriptor_pool_sampler();
	create_descriptor_sets();
}

void VulkanRenderer::create_texture_sampler()
{

	// sampler create info
	VkSamplerCreateInfo sampler_create_info{};
	sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_create_info.magFilter = VK_FILTER_LINEAR;
	sampler_create_info.minFilter = VK_FILTER_LINEAR;
	sampler_create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_create_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
	sampler_create_info.unnormalizedCoordinates = VK_FALSE;
	sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler_create_info.mipLodBias = 0.0f;
	sampler_create_info.minLod = 0.0f;
	sampler_create_info.maxLod = 0.0f;
	sampler_create_info.anisotropyEnable = VK_TRUE;
	sampler_create_info.maxAnisotropy = 16;									// max anisotropy sample level

	VkResult result = vkCreateSampler(device->getLogicalDevice(), &sampler_create_info, nullptr, &texture_sampler);
	ASSERT_VULKAN(result, "Failed to create a texture sampler!")

}

void VulkanRenderer::create_uniform_buffers()
{

	// one uniform buffer for each image (and by extension, command buffer)
	globalUBOBuffer.resize(vulkanSwapChain.getNumberSwapChainImages());
	sceneUBOBuffer.resize(vulkanSwapChain.getNumberSwapChainImages());

	//// temporary buffer to "stage" vertex data before transfering to GPU
	//VulkanBuffer	stagingBuffer;
	std::vector<GlobalUBO> globalUBOdata;
	globalUBOdata.push_back(globalUBO);

	std::vector<SceneUBO> sceneUBOdata;
	sceneUBOdata.push_back(sceneUBO);

	// create uniform buffers 
	for (size_t i = 0; i < vulkanSwapChain.getNumberSwapChainImages(); i++) {

		vulkanBufferManager.createBufferAndUploadVectorOnDevice(device.get(),
																compute_command_pool,
																globalUBOBuffer[i],
																VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | 
																VK_BUFFER_USAGE_TRANSFER_DST_BIT,
																VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
																globalUBOdata);

		vulkanBufferManager.createBufferAndUploadVectorOnDevice(device.get(),
																compute_command_pool,
																sceneUBOBuffer[i],
																VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
																VK_BUFFER_USAGE_TRANSFER_DST_BIT,
																VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
																sceneUBOdata);
		
	}

}

void VulkanRenderer::create_descriptor_pool_uniforms()
{

	// CREATE UNIFORM DESCRIPTOR POOL
	// type of descriptors + how many descriptors, not descriptor sets (combined makes the pool size)
	// ViewProjection Pool 
	VkDescriptorPoolSize vp_pool_size{};
	vp_pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	vp_pool_size.descriptorCount = static_cast<uint32_t>(globalUBOBuffer.size());

	// DIRECTION POOL
	VkDescriptorPoolSize directions_pool_size{};
	directions_pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	directions_pool_size.descriptorCount = static_cast<uint32_t>(sceneUBOBuffer.size());

	VkDescriptorPoolSize object_descriptions_pool_size{};
	object_descriptions_pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	object_descriptions_pool_size.descriptorCount = static_cast<uint32_t>(sizeof(ObjectDescription) * MAX_OBJECTS);

	// list of pool sizes 
	std::vector<VkDescriptorPoolSize> descriptor_pool_sizes = { vp_pool_size , 
																directions_pool_size, 
																object_descriptions_pool_size };

	VkDescriptorPoolCreateInfo pool_create_info{};
	pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_create_info.maxSets = vulkanSwapChain.getNumberSwapChainImages();					// maximum number of descriptor sets that can be created from pool
	pool_create_info.poolSizeCount = static_cast<uint32_t>(descriptor_pool_sizes.size());	// amount of pool sizes being passed
	pool_create_info.pPoolSizes = descriptor_pool_sizes.data();								// pool sizes to create pool with

	// create descriptor pool
	VkResult result = vkCreateDescriptorPool(device->getLogicalDevice(), &pool_create_info, nullptr, &descriptor_pool);
	ASSERT_VULKAN(result, "Failed to create a descriptor pool!")

}

void VulkanRenderer::create_descriptor_pool_sampler()
{

	// CREATE SAMPLER DESCRIPTOR POOL
	// TEXTURE SAMPLER POOL
	VkDescriptorPoolSize sampler_pool_size{};
	sampler_pool_size.type = VK_DESCRIPTOR_TYPE_SAMPLER;
	sampler_pool_size.descriptorCount = 2;

	VkDescriptorPoolSize sampled_image_pool_size{};
	sampled_image_pool_size.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	sampled_image_pool_size.descriptorCount = MAX_TEXTURE_COUNT;

	std::vector<VkDescriptorPoolSize> descriptor_pool_sizes = { sampler_pool_size , sampled_image_pool_size };

	VkDescriptorPoolCreateInfo sampler_pool_create_info{};
	sampler_pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	sampler_pool_create_info.maxSets = 1;
	sampler_pool_create_info.poolSizeCount = static_cast<uint32_t>(descriptor_pool_sizes.size());;
	sampler_pool_create_info.pPoolSizes = descriptor_pool_sizes.data();

	// create descriptor pool
	VkResult result = vkCreateDescriptorPool(device->getLogicalDevice(), &sampler_pool_create_info, nullptr, &sampler_descriptor_pool);
	ASSERT_VULKAN(result, "Failed to create a sampler descriptor pool!")

}

void VulkanRenderer::create_descriptor_pool_object_description()
{

	// CREATE SAMPLER DESCRIPTOR POOL
	// TEXTURE SAMPLER POOL
	VkDescriptorPoolSize object_description_pool_size{};
	object_description_pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	object_description_pool_size.descriptorCount = MAX_OBJECTS;

	VkDescriptorPoolCreateInfo object_description_pool_create_info{};
	object_description_pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	object_description_pool_create_info.maxSets = 3;//MAX_OBJECTS;
	object_description_pool_create_info.poolSizeCount = 1;
	object_description_pool_create_info.pPoolSizes = &object_description_pool_size;

	// create descriptor pool
	VkResult result = vkCreateDescriptorPool(device->getLogicalDevice(), &object_description_pool_create_info, nullptr, &object_description_pool);
	ASSERT_VULKAN(result, "Failed to create a object description descriptor pool!")

}

void VulkanRenderer::create_descriptor_sets()
{

	// resize descriptor set list so one for every buffer
	descriptor_sets.resize(vulkanSwapChain.getNumberSwapChainImages());

	std::vector<VkDescriptorSetLayout> set_layouts(vulkanSwapChain.getNumberSwapChainImages(), descriptor_set_layout);

	// descriptor set allocation info
	VkDescriptorSetAllocateInfo set_alloc_info{};
	set_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	set_alloc_info.descriptorPool = descriptor_pool;											// pool to allocate descriptor set from
	set_alloc_info.descriptorSetCount = vulkanSwapChain.getNumberSwapChainImages();				// number of sets to allocate
	set_alloc_info.pSetLayouts = set_layouts.data();											// layouts to use to allocate sets (1:1 relationship)
	
	// allocate descriptor sets (multiple)
	VkResult result = vkAllocateDescriptorSets(device->getLogicalDevice(), &set_alloc_info, descriptor_sets.data());
	ASSERT_VULKAN(result, "Failed to create descriptor sets!")

	// update all of descriptor set buffer bindings
	for (size_t i = 0; i < vulkanSwapChain.getNumberSwapChainImages(); i++) {

		// VIEW PROJECTION DESCRIPTOR
		// buffer info and data offset info
		VkDescriptorBufferInfo globalUBO_buffer_info{};
		globalUBO_buffer_info.buffer = globalUBOBuffer[i].getBuffer();					// buffer to get data from 
		globalUBO_buffer_info.offset = 0;												// position of start of data
		globalUBO_buffer_info.range = sizeof(globalUBO);								// size of data

		// data about connection between binding and buffer
		VkWriteDescriptorSet globalUBO_set_write{};
		globalUBO_set_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		globalUBO_set_write.dstSet = descriptor_sets[i];								// descriptor set to update 
		globalUBO_set_write.dstBinding = 0;												// binding to update (matches with binding on layout/shader)
		globalUBO_set_write.dstArrayElement = 0;										// index in array to update
		globalUBO_set_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;			// type of descriptor
		globalUBO_set_write.descriptorCount = 1;										// amount to update
		globalUBO_set_write.pBufferInfo = &globalUBO_buffer_info;						// information about buffer data to bind

		// VIEW PROJECTION DESCRIPTOR
		// buffer info and data offset info
		VkDescriptorBufferInfo sceneUBO_buffer_info{};
		sceneUBO_buffer_info.buffer = sceneUBOBuffer[i].getBuffer();					// buffer to get data from 
		sceneUBO_buffer_info.offset = 0;												// position of start of data
		sceneUBO_buffer_info.range = sizeof(sceneUBO);									// size of data

		// data about connection between binding and buffer
		VkWriteDescriptorSet sceneUBO_set_write{};
		sceneUBO_set_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		sceneUBO_set_write.dstSet = descriptor_sets[i];									// descriptor set to update 
		sceneUBO_set_write.dstBinding = 1;												// binding to update (matches with binding on layout/shader)
		sceneUBO_set_write.dstArrayElement = 0;											// index in array to update
		sceneUBO_set_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;			// type of descriptor
		sceneUBO_set_write.descriptorCount = 1;											// amount to update
		sceneUBO_set_write.pBufferInfo = &sceneUBO_buffer_info;							// information about buffer data to bind

		std::vector<VkWriteDescriptorSet> write_descriptor_sets = { globalUBO_set_write,
																	sceneUBO_set_write };

		// update the descriptor sets with new buffer/binding info
		vkUpdateDescriptorSets(device->getLogicalDevice(), static_cast<uint32_t>(write_descriptor_sets.size()),
													write_descriptor_sets.data(), 0, nullptr);
	}

}

void VulkanRenderer::create_sampler_array_descriptor_set()
{

	// descriptor set allocation info 
	VkDescriptorSetAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = sampler_descriptor_pool;
	alloc_info.descriptorSetCount = 1;
	alloc_info.pSetLayouts = &sampler_set_layout;

	// allocte descriptor sets
	VkResult result = vkAllocateDescriptorSets(device->getLogicalDevice(), &alloc_info, &sampler_descriptor_set);
	ASSERT_VULKAN(result, "Failed to allocate texture descriptor sets!")


	std::vector<Texture>& modelTextures = scene->getTextures(0);
	std::vector<VkDescriptorImageInfo> image_info_textures;
	image_info_textures.resize(scene->getTextureCount(0));
	for (uint32_t i = 0; i < scene->getTextureCount(0); i++) {
		image_info_textures[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		image_info_textures[i].imageView = modelTextures[i].getImageView();
		image_info_textures[i].sampler = nullptr;
	}

	// descriptor write info
	VkWriteDescriptorSet descriptor_write{};
	descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptor_write.dstSet = sampler_descriptor_set;
	descriptor_write.dstBinding = TEXTURES_BINDING;
	descriptor_write.dstArrayElement = 0;
	descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	descriptor_write.descriptorCount = static_cast<uint32_t>(image_info_textures.size());
	descriptor_write.pImageInfo = image_info_textures.data();

	VkDescriptorImageInfo sampler_info;
	sampler_info.imageView = nullptr;
	sampler_info.sampler = texture_sampler;

	// descriptor write info
	VkWriteDescriptorSet descriptor_write_sampler{};
	descriptor_write_sampler.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptor_write_sampler.dstSet = sampler_descriptor_set;
	descriptor_write_sampler.dstBinding = SAMPLER_BINDING;
	descriptor_write_sampler.dstArrayElement = 0;
	descriptor_write_sampler.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	descriptor_write_sampler.descriptorCount = 1;
	descriptor_write_sampler.pImageInfo = &sampler_info;

	std::vector<VkWriteDescriptorSet> write_descriptor_sets = { descriptor_write,
																descriptor_write_sampler };

	// update new descriptor set
	vkUpdateDescriptorSets(device->getLogicalDevice(), static_cast<uint32_t>(write_descriptor_sets.size()),
							write_descriptor_sets.data(), 0, nullptr);

}

void VulkanRenderer::cleanUpUBOs()
{
	for (VulkanBuffer vulkanBuffer : globalUBOBuffer) {
		vulkanBuffer.cleanUp();
	}

	for (VulkanBuffer vulkanBuffer : globalUBOBuffer) {
		vulkanBuffer.cleanUp();
	}
}

void VulkanRenderer::update_uniform_buffers(uint32_t image_index)
{
	auto usage_stage_flags = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | 
							VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		 

	VkBufferMemoryBarrier before_barrier_uvp;
	before_barrier_uvp.pNext = nullptr;
	before_barrier_uvp.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	before_barrier_uvp.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	before_barrier_uvp.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	before_barrier_uvp.buffer = globalUBOBuffer[image_index].getBuffer();
	before_barrier_uvp.offset = 0;
	before_barrier_uvp.size = sizeof(globalUBO);
	before_barrier_uvp.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	before_barrier_uvp.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	VkBufferMemoryBarrier before_barrier_directions;
	before_barrier_directions.pNext = nullptr;
	before_barrier_directions.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	before_barrier_directions.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	before_barrier_directions.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	before_barrier_directions.buffer = globalUBOBuffer[image_index].getBuffer();
	before_barrier_directions.offset = 0;
	before_barrier_directions.size = sizeof(sceneUBO);
	before_barrier_directions.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	before_barrier_directions.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	vkCmdPipelineBarrier(command_buffers[image_index], usage_stage_flags,
		VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &before_barrier_uvp, 0, nullptr);
	vkCmdPipelineBarrier(command_buffers[image_index], usage_stage_flags,
		VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &before_barrier_directions, 0, nullptr);

	vkCmdUpdateBuffer(command_buffers[image_index], globalUBOBuffer[image_index].getBuffer(), 0, sizeof(GlobalUBO), &globalUBO);
	vkCmdUpdateBuffer(command_buffers[image_index], sceneUBOBuffer[image_index].getBuffer(), 0, sizeof(SceneUBO), &sceneUBO);

	VkBufferMemoryBarrier after_barrier_uvp;
	after_barrier_uvp.pNext = nullptr;
	after_barrier_uvp.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	after_barrier_uvp.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	after_barrier_uvp.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	after_barrier_uvp.buffer = globalUBOBuffer[image_index].getBuffer();
	after_barrier_uvp.offset = 0;
	after_barrier_uvp.size = sizeof(GlobalUBO);
	after_barrier_uvp.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	after_barrier_uvp.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	VkBufferMemoryBarrier after_barrier_directions;
	after_barrier_directions.pNext = nullptr;
	after_barrier_directions.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	after_barrier_directions.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	after_barrier_directions.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	after_barrier_directions.buffer = globalUBOBuffer[image_index].getBuffer();
	after_barrier_directions.offset = 0;
	after_barrier_directions.size = sizeof(SceneUBO);
	after_barrier_directions.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	after_barrier_directions.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	vkCmdPipelineBarrier(command_buffers[image_index], VK_PIPELINE_STAGE_TRANSFER_BIT,
		usage_stage_flags, 0, 0, nullptr, 1, &after_barrier_uvp, 0, nullptr);
	vkCmdPipelineBarrier(command_buffers[image_index], VK_PIPELINE_STAGE_TRANSFER_BIT,
			usage_stage_flags, 0, 0, nullptr, 1, &after_barrier_directions, 0, nullptr);

}

void VulkanRenderer::recreate_swap_chain()
{

	// capture case of minification
	int width = 0, height = 0;
	glfwGetFramebufferSize(window->get_window(), &width, &height);
	while (width == 0 || height == 0) {
		glfwGetFramebufferSize(window->get_window(), &width, &height);
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(device->getLogicalDevice());
	vkQueueWaitIdle(device->getGraphicsQueue());

	clean_up_swapchain();

	vulkanSwapChain = VulkanSwapChain();
	vulkanSwapChain.initVulkanContext(	device.get(),
										window,
										surface);
	create_uniform_buffers();
	create_descriptor_pool_uniforms();
	create_descriptor_sets();
	create_command_buffers();
	std::vector<VkDescriptorSetLayout> descriptor_set_layouts = { descriptor_set_layout, sampler_set_layout };
	rasterizer.init(device.get(), &vulkanSwapChain, descriptor_set_layouts, graphics_command_pool);

	// all post
	std::vector<VkDescriptorSetLayout> descriptorSets = { post_descriptor_set_layout };
	postStage.init(device.get(), &vulkanSwapChain, descriptorSets);
	create_post_descriptor();
	update_post_descriptor_set();

	images_in_flight_fences.resize(vulkanSwapChain.getNumberSwapChainImages(), VK_NULL_HANDLE);

}

void VulkanRenderer::update_raytracing_descriptor_set(uint32_t image_index)
{

	VkWriteDescriptorSetAccelerationStructureKHR descriptor_set_acceleration_structure{};
	descriptor_set_acceleration_structure.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
	descriptor_set_acceleration_structure.pNext = nullptr;
	descriptor_set_acceleration_structure.accelerationStructureCount = 1;
	VkAccelerationStructureKHR& tlasAS = tlas.getAS();
	descriptor_set_acceleration_structure.pAccelerationStructures = &tlasAS;

	VkWriteDescriptorSet write_descriptor_set_acceleration_structure{};
	write_descriptor_set_acceleration_structure.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write_descriptor_set_acceleration_structure.pNext = &descriptor_set_acceleration_structure;
	write_descriptor_set_acceleration_structure.dstSet = raytracing_descriptor_set[image_index];
	write_descriptor_set_acceleration_structure.dstBinding = TLAS_BINDING;
	write_descriptor_set_acceleration_structure.dstArrayElement = 0;
	write_descriptor_set_acceleration_structure.descriptorCount = 1;
	write_descriptor_set_acceleration_structure.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	write_descriptor_set_acceleration_structure.pImageInfo = nullptr;
	write_descriptor_set_acceleration_structure.pBufferInfo = nullptr;
	write_descriptor_set_acceleration_structure.pTexelBufferView = nullptr;

	VkDescriptorBufferInfo object_description_buffer_info{};
	object_description_buffer_info.buffer = objectDescriptionBuffer.getBuffer();
	object_description_buffer_info.offset = 0;
	object_description_buffer_info.range = VK_WHOLE_SIZE;

	VkWriteDescriptorSet object_description_buffer_write{};
	object_description_buffer_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	object_description_buffer_write.dstSet = descriptor_sets[image_index];
	object_description_buffer_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	object_description_buffer_write.dstBinding = OBJECT_DESCRIPTION_BINDING;
	object_description_buffer_write.pBufferInfo = &object_description_buffer_info;
	object_description_buffer_write.descriptorCount = 1;

	std::vector<VkWriteDescriptorSet> write_descriptor_sets = {	write_descriptor_set_acceleration_structure,
																object_description_buffer_write };

	vkUpdateDescriptorSets(device->getLogicalDevice(), static_cast<uint32_t>(write_descriptor_sets.size()),
		write_descriptor_sets.data(), 0, nullptr);

}

void VulkanRenderer::record_commands(uint32_t image_index)
{

	PFN_vkGetBufferDeviceAddressKHR pvkGetBufferDeviceAddressKHR = (PFN_vkGetBufferDeviceAddressKHR)
												vkGetDeviceProcAddr(device->getLogicalDevice(), "vkGetBufferDeviceAddress");

	if (guiRendererSharedVars.raytracing) {
		
		std::vector<VkDescriptorSet> sets = {	descriptor_sets[image_index],
												raytracing_descriptor_set[image_index] };
		raytracingStage.recordCommands(command_buffers[image_index], sets);

	}
	else {

		std::vector<VkDescriptorSet> descriptorSets = {	descriptor_sets[image_index],
														sampler_descriptor_set};

		rasterizer.recordCommands(command_buffers[image_index], image_index, scene, descriptorSets);

	}

	Texture& renderResult = rasterizer.getOffscreenTexture(image_index);
	VulkanImage& vulkanImage = renderResult.getVulkanImage();
	vulkanImage.transitionImageLayout(	command_buffers[image_index],
										VK_IMAGE_LAYOUT_GENERAL,
										VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
										VK_IMAGE_ASPECT_COLOR_BIT);

	std::vector<VkDescriptorSet> descriptorSets = { post_descriptor_set[image_index] };
	postStage.recordCommands(command_buffers[image_index], image_index, descriptorSets);

	vulkanImage.transitionImageLayout(	command_buffers[image_index],
										VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
										VK_IMAGE_LAYOUT_GENERAL, 1,
										VK_IMAGE_ASPECT_COLOR_BIT);
	
}

void VulkanRenderer::check_changed_framebuffer_size()
{

	if (window->framebuffer_size_has_changed()) {

		int width, height;
		glfwGetFramebufferSize(window->get_window(), &width, &height);

		globalUBO.projection = glm::perspective(glm::radians(40.0f), (float)width / (float)height,
															0.1f, 1000.f);

		window->reset_framebuffer_has_changed();
		framebuffer_resized = true;

	}
}

void VulkanRenderer::clean_up_swapchain()
{

	vkDeviceWaitIdle(device->getLogicalDevice());

	rasterizer.cleanUp();

	vkDestroyDescriptorSetLayout(device->getLogicalDevice(), post_descriptor_set_layout, nullptr);
	vkDestroyDescriptorPool(device->getLogicalDevice(), post_descriptor_pool, nullptr);
	vkDestroyDescriptorPool(device->getLogicalDevice(), descriptor_pool, nullptr);

}

void VulkanRenderer::clean_up_raytracing()
{

	raytracingStage.cleanUp();

	vkDestroyDescriptorSetLayout(device->getLogicalDevice(), raytracing_descriptor_set_layout, nullptr);

	vkDestroyDescriptorPool(device->getLogicalDevice(), object_description_pool, nullptr);
	vkDestroyDescriptorPool(device->getLogicalDevice(), raytracing_descriptor_pool, nullptr);

	tlas.cleanUp();
	
	for (size_t index = 0; index < blas.size(); index++) {

		blas[index].cleanUp();

	}

}

void VulkanRenderer::clean_up()
{

	// wait until no actions being run on device before destroying
	vkDeviceWaitIdle(device->getLogicalDevice());

	cleanUpUBOs();

	// -- SUBSUMMARIZE ALL SWAPCHAIN DEPENDEND THINGS
	clean_up_swapchain();

	// -- CLEAN UP RAYTRACING STUFF
	 clean_up_raytracing();

	 //instead of recreate command pool from scretch empty command buffers
	vkFreeCommandBuffers(device->getLogicalDevice(), graphics_command_pool, 
												static_cast<uint32_t>(command_buffers.size()), command_buffers.data());

	// -- DESTROY ALL LAYOUTS
	vkDestroyDescriptorSetLayout(device->getLogicalDevice(), descriptor_set_layout, nullptr);
	vkDestroyDescriptorSetLayout(device->getLogicalDevice(), sampler_set_layout, nullptr);

	// -- TEXTURE REALTED
	vkDestroyDescriptorPool(device->getLogicalDevice(), sampler_descriptor_pool, nullptr);
	vkDestroySampler(device->getLogicalDevice(), texture_sampler, nullptr);

	for (int i = 0; i < MAX_FRAME_DRAWS; i++) {

		vkDestroySemaphore(device->getLogicalDevice(), render_finished[i], nullptr);
		vkDestroySemaphore(device->getLogicalDevice(), image_available[i], nullptr);
		vkDestroyFence(device->getLogicalDevice(), in_flight_fences[i], nullptr);

	}

	cleanUpCommandPools();

	vulkanSwapChain.cleanUp();
	vkDestroySurfaceKHR(instance.getVulkanInstance(), surface, nullptr);
	device->cleanUp();
	instance.cleanUp();

}

VulkanRenderer::~VulkanRenderer()
{
}