#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// the importer from the assimp library
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <stdexcept>
#include <vector>
#include <memory>
#include <cstring>
#include <iostream>
#include <set>
#include <algorithm>
#include <array>
#include <stdlib.h>
#include <stdio.h>

#include "stb_image.h"

#include "MyWindow.h"
#include "Utilities.h"
#include "Mesh.h"
#include "MeshModel.h"
#include "Camera.h"

// all IMGUI stuff
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

class VulkanRenderer
{
public:

	VulkanRenderer();

	int init(std::shared_ptr<MyWindow> window, glm::vec3 eye, float near_plane, float far_plane,
					glm::vec3 light_dir, glm::vec3 view_dir, bool raytracing);

	void init_rasterizer();

	int create_mesh_model(std::string model_file, bool flip_y);

	void update_model(int model_id, glm::mat4 new_model);
	void update_view(glm::mat4 view);
	void update_directions(glm::vec3 light_dir, glm::vec3 view_dir);

	void hot_reload_all_shader();

	void rasterize();
	void raytrace();

	void clean_up_swapchain();
	void clean_up();

	~VulkanRenderer();

private:

	// ----- VULKAN CORE COMPONENTS ----- BEGIN
	VkInstance instance;

	struct {

		VkPhysicalDevice physical_device;
		VkDevice logical_device;

	} MainDevice;

	// available queues
	VkQueue graphics_queue;
	VkQueue presentation_queue;
	VkQueue compute_queue;

	// surface defined on windows as WIN32 window system, Linux f.e. X11, MacOS also their own
	VkSurfaceKHR surface;

	// all regarding swapchain
	VkSwapchainKHR swapchain;
	std::vector<SwapChainImage> swap_chain_images;
	std::vector<VkFramebuffer> swap_chain_framebuffers;
	std::vector<VkCommandBuffer> command_buffers;
	VkFormat swap_chain_image_format;
	VkExtent2D swap_chain_extent;
	void recreate_swap_chain();

	// core create functions
	void create_instance();
	void create_logical_device();
	void create_surface();
	void create_swap_chain();

	// -- support functions 
	// helper create functions
	VkImage create_image(uint32_t width, uint32_t height, uint32_t mip_levels, VkFormat format, VkImageTiling tiling, VkImageUsageFlags use_flags,
												VkMemoryPropertyFlags prop_flags, VkDeviceMemory* image_memory);
	VkImageView create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags, uint32_t mip_levels);
	VkShaderModule create_shader_module(const std::vector<char>& code);

	// texture functions 
	int create_texture(std::string filename);
	int create_texture_image(std::string filename);
	int create_texture_descriptor(VkImageView texture_image);

	// allocate functions
	void allocate_dynamic_buffer_transfer_space();

	// checker functions
	bool check_instance_extension_support(std::vector<const char*>* check_extensions);
	bool check_device_extension_support(VkPhysicalDevice device);
	bool check_device_suitable(VkPhysicalDevice device);

	// getter functions
	void get_physical_device();
	QueueFamilyIndices get_queue_families(VkPhysicalDevice device);
	SwapChainDetails get_swapchain_details(VkPhysicalDevice device);

	// choose functions
	VkSurfaceFormatKHR choose_best_surface_format(const std::vector<VkSurfaceFormatKHR>& formats);
	VkPresentModeKHR choose_best_presentation_mode(const std::vector<VkPresentModeKHR>& presentation_modes);
	VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& surface_capabilities);
	VkFormat choose_supported_format(const std::vector<VkFormat>& formats, VkImageTiling tiling, VkFormatFeatureFlags feature_flags);

	// -- debugging
	VkDebugUtilsMessengerEXT debug_messenger;
	bool check_validation_layer_support();

	// -- pools
	VkCommandPool graphics_command_pool;
	
	// -- synchronization
	std::vector<VkSemaphore> image_available;
	std::vector<VkSemaphore> render_finished;
	std::vector<VkFence> draw_fences;
	std::vector<VkFence> images_in_flight_fences;
	void create_synchronization();

	// ----- VULKAN CORE COMPONENTS ----- END

	// ----- ALL RASTERIZER SPECIFICS ----- BEGIN 
	// -- pipeline
	VkPipeline graphics_pipeline;
	VkPipelineLayout pipeline_layout;
	VkRenderPass render_pass;

	// depth
	VkImage depth_buffer_image;
	VkDeviceMemory depth_buffer_image_memory;
	VkImageView depth_buffer_image_view;
	VkFormat depth_format;

	// all create functions
	void create_render_pass();
	void create_descriptor_set_layouts();
	void create_push_constant_range();
	void create_rasterizer_graphics_pipeline();
	void create_depthbuffer_image();
	void create_framebuffers();
	void create_command_pool();
	void create_command_buffers();
	void create_texture_sampler();

	void create_uniform_buffers();
	void create_descriptor_pool_uniforms();
	void create_descriptor_pool_sampler();
	void create_descriptor_sets();

	// uniforms
	struct UboViewProjection {

		glm::mat4 projection;
		glm::mat4 view;

	} ubo_view_projection;

	struct UboDirections {

		glm::vec3 light_dir;
		glm::vec3 view_dir;

	} ubo_directions;

	// uniform buffer
	std::vector<VkBuffer> vp_uniform_buffer;
	std::vector<VkDeviceMemory> vp_uniform_buffer_memory;
	std::vector<VkBuffer> directions_uniform_buffer;
	std::vector<VkDeviceMemory> directions_uniform_buffer_memory;

	// ----- ALL RASTERIZER SPECIFICS ----- END 

	// - descriptors
	VkDescriptorSetLayout descriptor_set_layout;								// for normal uniform values
	VkDescriptorSetLayout sampler_set_layout;									// descriptor set layout for our samplers
	VkPushConstantRange push_constant_range;
	VkDescriptorPool descriptor_pool;
	VkDescriptorPool sampler_descriptor_pool;
	std::vector<VkDescriptorSet> descriptor_sets;
	std::vector<VkDescriptorSet> sampler_descriptor_sets;				// these are no swap chain dependend descriptors, doesn't change over frames


	// ----- ALL RAYTRACING SPECIFICS ----- BEGIN
	// -- en/-disable raytracing
	bool raytracing;

	void init_raytracing();

	// -- create funcs
	// -- bottom level acceleration structure
	void create_BLAS(MeshModel mesh_list, uint32_t index);
	// -- top level acceleration structure
	void create_TLAS();
	void create_raytracing_pipeline();
	void create_shader_binding_table();
	void create_raytracing_descriptor_pool();
	void create_raytracing_descriptor_set_layouts();
	void create_raytracing_descriptor_sets();

	VkPhysicalDeviceRayTracingPipelinePropertiesKHR raytracing_properties;

	// -- acceleration structure
	// -- bottom level
	std::vector<VkAccelerationStructureKHR> bottom_level_acceleration_structure;
	std::vector<VkBuffer> bottom_level_acceleration_structure_buffer;
	std::vector<VkDeviceMemory> bottom_level_acceleration_structure_buffer_memory;

	// -- top level
	VkAccelerationStructureKHR top_level_acceleration_structure;
	VkBuffer top_level_acceleration_structure_buffer;
	VkDeviceMemory top_level_acceleration_structure_buffer_memory;

	// -- image view
	VkImageView raytracing_image_view;
	VkImage raytracing_image;
	VkDeviceMemory ray_trace_image_memory;

	// -- descriptors
	VkDescriptorPool raytracing_descriptor_pool;
	VkDescriptorSet raytracing_descriptor_set;
	std::vector<VkDescriptorSetLayout> raytracing_descriptor_set_layouts;

	// -- pipeline
	VkPipeline raytracing_pipeline;
	VkPipelineLayout raytracing_pipeline_layout;

	// -- shader bindings
	VkBuffer shader_binding_table_buffer;
	VkDeviceMemory shader_binding_table_buffer_memory;

	// -- properties
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR ray_tracing_pipeline_properties;

	// ----- ALL RAYTRACING SPECIFICS ----- END

	// -- TEXTURE --
	VkSampler texture_sampler;
	int max_levels;
	std::vector<uint32_t> texture_mip_levels;
	std::vector<VkImage> texture_images;
	std::vector<VkDeviceMemory> texture_images_memory;
	std::vector<VkImageView> texture_image_views;

	// -- UPDATE FUNCTIONS FOR THE DRAW COMMAND
	void update_uniform_buffers(uint32_t image_index);
	void record_commands(uint32_t current_image);

	// ---- HELPER ---- BEGIN
	stbi_uc* load_texture_file(std::string file_name, int* width, int* height, VkDeviceSize* image_size);
	// ---- HELPER ---- END
	
	// ----- GUI STUFF ----- BEGIN
	VkDescriptorPool gui_descriptor_pool;
	void create_gui();
	void create_gui_context();
	void create_fonts_and_upload();
	// ----- GUI STUFF ----- END

	// ----- VARS ----- BEGIN
	std::shared_ptr<MyWindow> window;
	void check_changed_framebuffer_size();
	bool framebuffer_resized;
	// indices index into current frame
	int current_frame;
	// ----- VARS ----- END

	// ----- EVERYTHING FOR THE SCENE ----- BEGIN
	std::vector<MeshModel> model_list;
	// ----- EVERYTHING FOR THE SCENE ----- END
};

