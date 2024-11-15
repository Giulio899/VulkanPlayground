#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <vector>
#include <iostream>
#include <set>
#include <algorithm>
#include <array>

#include "Utilities.h"

class VulkanRenderer
{
public:
	VulkanRenderer();

	int init(GLFWwindow* newWindow);
	void cleanup();

	~VulkanRenderer();

private:
	GLFWwindow* window;

	//Vulkan Components
	//-Core
	VkInstance instance;
	struct {
		VkPhysicalDevice physicalDevice;
		VkDevice logicalDevice;
	} mainDevice;
	VkQueue graphicsQueue;
	VkQueue presentationQueue;
	VkSurfaceKHR surface;
	VkSwapchainKHR swapChain;
	
	std::vector<SwapChainImage> swapChainImages;
	std::vector<VkFramebuffer> swapChainFrameBuffers;
	std::vector<VkCommandBuffer> commandBuffers;

	//-Pipeline
	VkPipeline graphicsPipeline;
	VkPipelineLayout pipelineLayout;
	VkRenderPass renderPass;

	//-Pools
	VkCommandPool graphicsCommandPool;

	//-Utils
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;

	//-Validation layer
	const std::vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};
	//This macro help to activate validations layer only in debug mode
	#ifdef NDEBUG
		const bool enableValidationLayers = false;
	#else
		const bool enableValidationLayers = true;
	#endif
	VkDebugUtilsMessengerEXT debugMessenger;


	//Vulkan Functions
	//-Create Functions
	void createInstance();
	void createLogicalDevice();
	void createDebugMessenger();
	void createSurface();
	void createSwapChain();
	void createRenderPass();
	void createGraphicsPipeline();
	void createFrameBuffers();
	void createCommandPool();
	void createCommandBuffers();

	//-Record Functions
	void recordCommands();

	//-Get Functions
	void getPhysicalDevice();
	//Return the required list of extensions based on whether validation layers are enabled or not
	std::vector<const char*> getRequiredExtensions();

	//-Support Functions
	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
	
	//--Checker Functions
	bool checkInstanceExtensionsSupport(std::vector<const char*>* checkExtensions);
	bool checkDeviceExtensionsSupport(VkPhysicalDevice device);
	bool checkDeviceSuitable(VkPhysicalDevice device);
	bool checkValidationLayerSupport();

	//--Getter Functions
	QueueFamilyIndices getQueueFamiliesIndices(VkPhysicalDevice device);
	SwapChainDetails getSwapChainDetails(VkPhysicalDevice device);

	//--Choose function
	VkSurfaceFormatKHR chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &formats);
	VkPresentModeKHR chooseBestPresentationMode(const std::vector<VkPresentModeKHR> &presentationModes);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &surfaceCapabilities);

	//--Create functions
	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
	VkShaderModule createShaderModule(const std::vector<char> &code);
	
	//Static functions
	//-Debug Callback
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);
	
};

