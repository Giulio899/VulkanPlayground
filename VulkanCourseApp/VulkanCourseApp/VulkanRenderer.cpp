#include "VulkanRenderer.h"

VulkanRenderer::VulkanRenderer()
{
}

int VulkanRenderer::init(GLFWwindow* newWindow)
{
	window = newWindow;

	try {
		//The order counts!!
		createInstance();
		createDebugMessenger();
		createSurface();
		getPhysicalDevice();
		createLogicalDevice();
		createSwapChain();
		createRenderPass();
		createDescriptorSetLayout();
		createPushConstantRange();
		createGraphicsPipeline();
		createDepthBufferImage();
		createFrameBuffers();
		createCommandPool();
		createCommandBuffers();
		createTextureSampler();
		//NOT IN USE UBOD
		//allocateDynamicBufferTransferSpace();
		createUniformBuffers();
		createDescriptorPool();
		createDescriptorSets();
		createSynchronization();

		uboViewProjection.projection = glm::perspective(glm::radians(45.0f), (float)swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 100.0f);
		uboViewProjection.view = glm::lookAt(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

		uboViewProjection.projection[1][1] *= -1; //Vulkan by default invert y coordinate
		
		//Create a mesh
		//Vertex data
		std::vector<Vertex> meshVertices = {
			{ { -0.4, 0.4, 0.0 },{ 1.0f, 0.0f, 0.0f }, {1.0f,1.0f} },	// 0
			{ { -0.4, -0.4, 0.0 },{ 1.0f, 0.0f, 0.0f }, {1.0f,0.0f} },	    // 1
			{ { 0.4, -0.4, 0.0 },{ 1.0f, 0.0f, 0.0f }, {0.0f,0.0f} },    // 2
			{ { 0.4, 0.4, 0.0 },{ 1.0f, 0.0f, 0.0f }, {0.0f,1.0f} },   // 3
		};

		std::vector<Vertex> meshVertices2 = {
			{ { -0.25, 0.6, 0.0 },{ 0.0f, 0.0f, 1.0f }, {1.0f,1.0f} },	// 0
			{ { -0.25, -0.6, 0.0 },{ 0.0f, 0.0f, 1.0f }, {1.0f,0.0f} },	    // 1
			{ { 0.25, -0.6, 0.0 },{ 0.0f, 0.0f, 1.0f }, {0.0f,0.0f} },    // 2
			{ { 0.25, 0.6, 0.0 },{ 0.0f, 0.0f, 1.0f }, {0.0f,1.0f} },   // 3
		};

		//Index data
		std::vector<uint32_t> meshIndices = {
			0, 1, 2,
			2, 3, 0
		};
		
		Mesh firstMesh = Mesh(mainDevice.physicalDevice, mainDevice.logicalDevice,
			graphicsQueue, graphicsCommandPool, //Graphics queue are also transfer queue in vulkan
			&meshVertices, &meshIndices,
			createTexture("smile.png"));

		Mesh secondMesh = Mesh(mainDevice.physicalDevice, mainDevice.logicalDevice,
			graphicsQueue, graphicsCommandPool, //Graphics queue are also transfer queue in vulkan
			&meshVertices2, &meshIndices,
			createTexture("nosmile.png"));

		meshList.push_back(firstMesh);
		meshList.push_back(secondMesh);
	}
	catch (const std::runtime_error& e) {
		printf("ERROR: %s", e.what());
		return EXIT_FAILURE;
	}


	return EXIT_SUCCESS;
}

void VulkanRenderer::updateModel(int modelId, glm::mat4 newModel)
{
	if(modelId >= meshList.size()) return;

	meshList[modelId].setModel(newModel);
}

void VulkanRenderer::draw()
{
	//1. Get the next available image to draw and set something to signal when we are finished with image (semaphore)
	//--GET NEXT IMAGE--
	//Wait for given fence to signal (open) from last draw before continuing
	vkWaitForFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());
	//Manually reset (close) fence
	vkResetFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame]);
	
	//Get index of the next image to be drawn to, and signal semaphore when ready to be drawn to
	uint32_t imageIndex;
	vkAcquireNextImageKHR(mainDevice.logicalDevice, swapChain, std::numeric_limits<uint64_t>::max(), imageAvailable[currentFrame], VK_NULL_HANDLE, &imageIndex);
	
	recordCommands(imageIndex);
	updateUniformBuffers(imageIndex);
	
	//2. Submit command buffer to queue for execution, making sure it waits for the image to be signalled as available before drawing
	//and signal when it finished rendering
	//--SUBMIT COMMAND BUFFER TO RENDER--
	//Queue submission information
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;							//Number of semaphores to wait on
	submitInfo.pWaitSemaphores = &imageAvailable[currentFrame];	//List of semaphores to wait on
	VkPipelineStageFlags waitStages[] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};
	submitInfo.pWaitDstStageMask = waitStages;					//Stages when sync occurs
	submitInfo.commandBufferCount = 1;							//Number of commandBuffers to submit
	submitInfo.pCommandBuffers = &commandBuffers[imageIndex];	//CommandBuffers to submit
	submitInfo.signalSemaphoreCount = 1;						//Number of semaphore to signal at end
	submitInfo.pSignalSemaphores = &renderFinished[currentFrame];//Semaphores to signal when command buffers finish

	//Submit command buffer to queue
	VkResult result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, drawFences[currentFrame]);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to submit command buffers!");
	}
	
	//3. Present image to screen when it has signalled finished rendering
	//--PRESENT RENDERED IMAGE TO SCREEN--
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;							//Number of semaphores to wait on
	presentInfo.pWaitSemaphores = &renderFinished[currentFrame];//Semaphores to wait on
	presentInfo.swapchainCount = 1;								//Number of swapchains to present to
	presentInfo.pSwapchains = &swapChain;						//SwapChain to present image to
	presentInfo.pImageIndices = &imageIndex;					//Index of images in swapchain to present
	
	//Present image
	result = vkQueuePresentKHR(presentationQueue, &presentInfo);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to present image!");
	}

	//Get next frame (use % MAX_FRAME_DRAWS to keep the nuber of frame undert that limit)
	//TODO-> fare diverso che coi nomi non si capisce
	currentFrame = (currentFrame + 1) % MAX_FRAME_DRAWS;
}

void VulkanRenderer::cleanup()
{
	//Wait until no action being run on device before destroying
	vkDeviceWaitIdle(mainDevice.logicalDevice);

	// _aligned_free(modelTransferSpace);

	vkDestroyDescriptorPool(mainDevice.logicalDevice, samplerDescriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, samplerSetLayout, nullptr);

	vkDestroySampler(mainDevice.logicalDevice, textureSampler, nullptr);

	for(size_t i = 0; i < textureImages.size(); i++)
	{
		vkDestroyImageView(mainDevice.logicalDevice, textureImageViews[i], nullptr);
		vkDestroyImage(mainDevice.logicalDevice, textureImages[i], /*Memory management TODO*/nullptr);
		vkFreeMemory(mainDevice.logicalDevice, textureImagesMemory[i], /*Memory management TODO*/nullptr);
	}

	vkDestroyImageView(mainDevice.logicalDevice, depthBufferImageView, nullptr);
	vkDestroyImage(mainDevice.logicalDevice, depthBufferImage, nullptr);
	vkFreeMemory(mainDevice.logicalDevice,depthBufferImageMemory, nullptr);

	vkDestroyDescriptorPool(mainDevice.logicalDevice, descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, descriptorSetLayout, nullptr);
	
	for(size_t i = 0; i < swapChainImages.size(); i++)
	{
		vkDestroyBuffer(mainDevice.logicalDevice, vpUniformBuffer[i], /*Memory management TODO*/nullptr);
		vkFreeMemory(mainDevice.logicalDevice, vpUniformBufferMemory[i], /*Memory management TODO*/nullptr);
		// vkDestroyBuffer(mainDevice.logicalDevice, modelDynamicUniformBuffer[i], /*Memory management TODO*/nullptr);
		// vkFreeMemory(mainDevice.logicalDevice, modelDynamicUniformBufferMemory[i], /*Memory management TODO*/nullptr);
	}
	
	for(size_t i = 0; i < meshList.size(); i++)
	{
		meshList[i].destroyBuffers();
	}
	
	//Reverse order than creation
	for(size_t i = 0; i < MAX_FRAME_DRAWS; i++)
	{
		vkDestroySemaphore(mainDevice.logicalDevice, renderFinished[i], /*Memory management TODO*/nullptr);
		vkDestroySemaphore(mainDevice.logicalDevice, imageAvailable[i], /*Memory management TODO*/nullptr);
		vkDestroyFence(mainDevice.logicalDevice, drawFences[i], /*Memory management TODO*/nullptr);
	}
	vkDestroyCommandPool(mainDevice.logicalDevice, graphicsCommandPool,/*Memory management TODO*/nullptr);
	for(auto frameBuffer : swapChainFrameBuffers)
	{
		vkDestroyFramebuffer(mainDevice.logicalDevice, frameBuffer,/*Memory management TODO*/nullptr);
	}
	vkDestroyPipeline(mainDevice.logicalDevice, graphicsPipeline,/*Memory management TODO*/nullptr);
	vkDestroyPipelineLayout(mainDevice.logicalDevice,pipelineLayout,/*Memory management TODO*/nullptr);
	vkDestroyRenderPass(mainDevice.logicalDevice, renderPass,/*Memory management TODO*/nullptr);
	for(auto image : swapChainImages)
	{
		vkDestroyImageView(mainDevice.logicalDevice, image.imageView, /*Memory management TODO*/nullptr);
	}
	vkDestroySwapchainKHR(mainDevice.logicalDevice, swapChain, /*Memory management TODO*/nullptr);
	vkDestroyDevice(mainDevice.logicalDevice,/*Memory management TODO*/nullptr);
	vkDestroySurfaceKHR(instance, surface,/*Memory management TODO*/nullptr);
	//Destroy DebugUtilsMessanger
	if (enableValidationLayers) {
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr) {
			func(instance, debugMessenger, /*Memory management TODO*/nullptr);
		}
	}
	vkDestroyInstance(instance,/*Memory management TODO*/nullptr);
}

VulkanRenderer::~VulkanRenderer()
{
}

void VulkanRenderer::createInstance()
{
	//Check validation layers
	if (enableValidationLayers && !checkValidationLayerSupport()) {
		throw std::runtime_error("validation layers requested, but not available!");
	}

	//Information about the application itself
	//Most data here doesn't affect the program and is for developer convenience
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Vulkan App";				//Custom name of the application
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);	//Custom version of the application
	appInfo.pEngineName = "No Engine";						//Custom engine name
	appInfo.engineVersion= VK_MAKE_VERSION(1, 0, 0);		//Custom version of the engine
	appInfo.apiVersion = VK_API_VERSION_1_0;				//Vulkan api version

	//Creation information for a VkInstance
	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	//Create list to hold instance extensions
	std::vector<const char*> instanceExtensions = getRequiredExtensions();

	//Check Instance extensions supported...
	if (!checkInstanceExtensionsSupport(&instanceExtensions)) {
		throw std::runtime_error("vkInstance does not support required extensions!");
	}

	createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
	createInfo.ppEnabledExtensionNames = instanceExtensions.data();

	//Enable validation layers (for debbugging and errors)
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	if (enableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();

		//Create a separate debug utils messenger specifically for those two function calls (createInstance and destroyInstance)
		//By creating an additional debug messenger this way it will automatically be used during vkCreateInstance and vkDestroyInstance and cleaned up after that
		populateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
	}
	else {
		createInfo.enabledLayerCount = 0;
		createInfo.ppEnabledLayerNames = nullptr;
	}

	//Create instance with createInfo
	VkResult result = vkCreateInstance(&createInfo, /*Memory management TODO*/nullptr, &instance);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Vulkan instance!");
	}
}

void VulkanRenderer::createLogicalDevice()
{
	//Physical device must be created and assigned to mainDevice.physicalDevice before this function
	QueueFamilyIndices indices = getQueueFamiliesIndices(mainDevice.physicalDevice);

	//Vector for queue creation infos, and set for family indices
	std::vector< VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<int> queueFamilyIndices = { indices.graphicsFamily, indices.presentationFamily };

	//Queue the logical device needs to create and info to do so
	for (int queueFamilyIndex : queueFamilyIndices)
	{
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamilyIndex;						//The index of the family to create the queue from
		queueCreateInfo.queueCount = 1;												//Number of queue to create
		float priority = 1.0f;
		queueCreateInfo.pQueuePriorities = &priority;								//Vulkan needs to know how to handle multiples queue, so decide priority (1=highest)

		queueCreateInfos.push_back(queueCreateInfo);
	}
	//Information to create logical device (sometimes called only "device")
	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());			//Number of queue create info
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();									//List of queue create info so device can create required queues
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());		//Number of enabled logical devices extensions
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();								//List of enabled logical device extensions

	//Physical device features the logical device will be using
	VkPhysicalDeviceFeatures deviceFeatures = {};
	deviceFeatures.samplerAnisotropy = VK_TRUE;				//Enable anisotropy

	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;						//Physical device features the logical device will be using


	//Create the logical device from the given logical device
	VkResult result = vkCreateDevice(mainDevice.physicalDevice, &deviceCreateInfo, /*Memory management TODO*/nullptr, &mainDevice.logicalDevice);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a logical device!");
	}

	//Queues are created at the same time as device...
	//So we want handle to queues
	vkGetDeviceQueue(mainDevice.logicalDevice, indices.graphicsFamily, 0, &graphicsQueue);				//Store the first logical device's queue in graphicsQueue
	vkGetDeviceQueue(mainDevice.logicalDevice, indices.presentationFamily, 0, &presentationQueue);		//Store the first logical device's queue in presentationQueue
}

void VulkanRenderer::createDebugMessenger()
{
	if (!enableValidationLayers) return;

	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	populateDebugMessengerCreateInfo(createInfo);

	//This struct should be passed to the vkCreateDebugUtilsMessengerEXT function to create the VkDebugUtilsMessengerEXT object.
	//Unfortunately, because this function is an extension function, it is not automatically loaded.
	//We have to look up its address ourselves using vkGetInstanceProcAddr.
	//We're going to create our own proxy function that handles this in the background
	VkResult creationResult;
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		creationResult = func(instance, &createInfo, /*Memory management TODO*/nullptr, &debugMessenger);
	}
	else {
		creationResult = VK_ERROR_EXTENSION_NOT_PRESENT;
	}
	if (creationResult != VK_SUCCESS) {
		throw std::runtime_error("Failed to set up debug messenger!");
	}
}

void VulkanRenderer::createSurface()
{
	//Create surface (creating a surface createinfo struct, runs the create surface function system independant)
	VkResult result=glfwCreateWindowSurface(instance,window,/*Memory allocator TODO*/nullptr,&surface);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a surface!");
	}
}

void VulkanRenderer::createSwapChain()
{
	//Get Swap Chain details so we pick the best settings
	SwapChainDetails swapChainDetails = getSwapChainDetails(mainDevice.physicalDevice);

	//1. CHOOSE BEST SURFACE FORMAT
	VkSurfaceFormatKHR surfaceFormat = chooseBestSurfaceFormat(swapChainDetails.formats);
	//2. CHOOSE BEST PRESENTATION MODE
	VkPresentModeKHR presentMode = chooseBestPresentationMode(swapChainDetails.presentationModes);
	//3. CHOOSE SWAP CHAIN IMAGE RESOLUTION
	VkExtent2D extent = chooseSwapExtent(swapChainDetails.surfaceCapabilities);

	//How many images are in the swap chain? Get 1 more than the minimum to enable triple buffering
	uint32_t imageCount = swapChainDetails.surfaceCapabilities.minImageCount + 1;
	//If imageCount higher than max, clamp to max
	//If is 0 than is limitless
	if(swapChainDetails.surfaceCapabilities.maxImageCount > 0
		&& swapChainDetails.surfaceCapabilities.maxImageCount < imageCount)
	{
		imageCount=swapChainDetails.surfaceCapabilities.maxImageCount;
	}
	
	//Creation info for swap chain
	VkSwapchainCreateInfoKHR swapChainCreateInfo = {};
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;		
	swapChainCreateInfo.surface = surface;															//Swapchain surface
	swapChainCreateInfo.imageFormat = surfaceFormat.format;											//Swapchain format
	swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;									//Swapchain colourspace
	swapChainCreateInfo.presentMode = presentMode;													//Swapchain presentation mode
	swapChainCreateInfo.imageExtent = extent;														//Swapchain image extent
	swapChainCreateInfo.minImageCount = imageCount;													//Minimum images in swapchain
	swapChainCreateInfo.imageArrayLayers = 1;														//Number of layers for each image in chain
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;							//What attachment image will be used on
	swapChainCreateInfo.preTransform = swapChainDetails.surfaceCapabilities.currentTransform;		//Tranform to perform on swap chain images
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;							//How to handle blending with external graphics (e.g. other windows)
	swapChainCreateInfo.clipped = VK_TRUE;															//Whether to clip parts of image not in view (overlapping other window)

	//Get queue family indices
	QueueFamilyIndices indices = getQueueFamiliesIndices((mainDevice.physicalDevice));

	//If graphics and presentation families are different, then swapchain must let imeges be shared between families
	if(indices.graphicsFamily != indices.presentationFamily)
	{
		uint32_t queueFamilyIndices[] = {
			(uint32_t)indices.graphicsFamily,
			(uint32_t)indices.presentationFamily
		};
		
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;							//Image share handling
		swapChainCreateInfo.queueFamilyIndexCount = 2;												//Number of queues to share images between
		swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;								//Array of queues to share between
	}
	else
	{
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;							//Image share handling
		swapChainCreateInfo.queueFamilyIndexCount = 0;												//Number of queues to share images between
		swapChainCreateInfo.pQueueFamilyIndices = nullptr;											//Array of queues to share between
	}

	//If old swap chai need been destroyed and this one replace it, then link old one to quickly hand over responsibilities
	swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	//Create SwapChain
	VkResult result = vkCreateSwapchainKHR(mainDevice.logicalDevice,&swapChainCreateInfo,/*Maemory allocation TODO*/nullptr,&swapChain);

	if(result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create SwapChain!");
	}

	//Saving utils values for later
	swapChainImageFormat = surfaceFormat.format;
	swapChainExtent = extent;

	//Get swap chain images
	uint32_t swapChainImageCount;
	vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapChain, &swapChainImageCount,nullptr);
	std::vector<VkImage> images(swapChainImageCount);
	vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapChain, &swapChainImageCount,images.data());

	for(VkImage image : images)
	{
		//Store images handle
		SwapChainImage swapChainImage = {};
		swapChainImage.image = image;
		swapChainImage.imageView = createImageView(image, swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);

		//Add to swapchain image list
		swapChainImages.push_back(swapChainImage);
	}
}

void VulkanRenderer::createRenderPass()
{
	//ATTACHMENTS
	//Colour attachment of render pass
	VkAttachmentDescription colourAttachment = {};
	colourAttachment.format = swapChainImageFormat;						//Format to use for attachment
	colourAttachment.samples = VK_SAMPLE_COUNT_1_BIT;					//Number of samples to write for multisampling
	colourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;				//Describes what to do with attachment before rendering
	colourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;			//Describes what to do with attachment after rendering
	colourAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;	//Describes what to do with stencil before rendering
	colourAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;	//Describes what to do with stencil after rendering

	//Framebuffer data will be stored as an image, but images can be given different data layouts
	//to give optimal use for certain operation
	colourAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;			//Image data layout before render pass start
	colourAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;		//Image data layout after render pass (to change to)

	//Depth Attachment of render pass
	VkAttachmentDescription depthAttachment = {};
	depthAttachment.format = chooseSupportedFormat(
		{VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT},
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	
	//REFERENCES
	//Attachment reference uses in attachment index that refers to index in the attachments list passed to renderPassCreateInfo
	VkAttachmentReference colourAttachmentReference = {};
	colourAttachmentReference.attachment = 0;										//Index in the renderpass attachment list
	colourAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;	//Layout between initial and final of render pass

	//Depth Attachment reference
	VkAttachmentReference depthAttachmentReference = {};
	depthAttachmentReference.attachment = 1;
	depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	
	//Information about the paticular subpass the render pass is using
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;		//Pipeline type subpass is bound to
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colourAttachmentReference;
	subpass.pDepthStencilAttachment = &depthAttachmentReference;

	
	//Need to determine when layout transitions occur using subpasse dependencies
	std::array<VkSubpassDependency, 2> subpassDependencies;

	//Conversion from VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	//Transition must happen after...
	subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;								//Subpass index: VK_SUBPASS_EXTERNAL means anything outside renderpass
	subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;				//Pipeline stage
	subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;						//Stage access mask (memory access)
	//But must happen before...
	subpassDependencies[0].dstSubpass = 0;													
	subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	subpassDependencies[0].dependencyFlags = 0;
	
	//Conversion from VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	//Transition must happen after...
	subpassDependencies[1].srcSubpass = 0;								
	subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;	
	subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;						
	//But must happen before...
	subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;													
	subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	subpassDependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	subpassDependencies[1].dependencyFlags = 0;

	std::array<VkAttachmentDescription, 2> renderPassAttachment = {colourAttachment, depthAttachment}; //Order is important and has to match in obj description
	
	//Create info for render pass
	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(renderPassAttachment.size());
	renderPassCreateInfo.pAttachments = renderPassAttachment.data();
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpass;
	renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
	renderPassCreateInfo.pDependencies = subpassDependencies.data();

	VkResult result = vkCreateRenderPass(mainDevice.logicalDevice, &renderPassCreateInfo, /*Memory management TODO*/nullptr, &renderPass);
	if(result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create render pass!");
	}
}

void VulkanRenderer::createDescriptorSetLayout()
{
	//UNIFORM VALUES DESCRIPTOR SET LAYOUT
	//UboViewProjection Binding Info
	VkDescriptorSetLayoutBinding vpLayoutBinding = {};
	vpLayoutBinding.binding = 0;											//Binding point in shider (designated by binding point in shader)
	vpLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;		//Type of descriptor
	vpLayoutBinding.descriptorCount = 1;									//Number of descriptors for binding
	vpLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;				//Shader stage to bind to
	vpLayoutBinding.pImmutableSamplers = nullptr;							//For textures: Can make sampler data unchangeable (immutable) by specifing in layout

	// //Model binding info
	// VkDescriptorSetLayoutBinding modelLayoutBinding = {};
	// modelLayoutBinding.binding = 1;												//Binding point in shider (designated by binding point in shader)
	// modelLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;	//Type of descriptor
	// modelLayoutBinding.descriptorCount = 1;										//Number of descriptors for binding
	// modelLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;					//Shader stage to bind to
	// modelLayoutBinding.pImmutableSamplers = nullptr;								//For textures: Can make sampler data unchangeable (immutable) by specifing in layout

	std::vector<VkDescriptorSetLayoutBinding> layoutBindings = {vpLayoutBinding/*, modelLayoutBinding*/};
	
	//Create Descriptor Set Layout with given bindings
	VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutCreateInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());	//Number of binding infos
	layoutCreateInfo.pBindings = layoutBindings.data();								//Array of binding infos

	//Create Descriptor Set Layout
	VkResult result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &layoutCreateInfo, nullptr, &descriptorSetLayout);
	if(result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to crate a descriptor set layout!");
	}

	//CREATE TEXTURE SAMPLER DESCRIPTOR SET LAYOUT
	//Texture binding info
	VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
	samplerLayoutBinding.binding = 0;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding.pImmutableSamplers = nullptr;

	//Create a Descriptor set layout with given bindings for texture
	VkDescriptorSetLayoutCreateInfo samplerLayoutCreateInfo = {};
	samplerLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	samplerLayoutCreateInfo.bindingCount = 1;
	samplerLayoutCreateInfo.pBindings = &samplerLayoutBinding;

	//Create descriptor set layout
	result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &samplerLayoutCreateInfo, nullptr, &samplerSetLayout);
	if(result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to crate sampler descriptor set layout!");
	}
}

void VulkanRenderer::createPushConstantRange()
{
	//Define push constant values (no 'create' need)
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;		//Shader push constant will go to
	pushConstantRange.offset = 0;									//Offset into given data to pass to push constant
	pushConstantRange.size = sizeof(Model);							//Size of data being pass
}

void VulkanRenderer::createGraphicsPipeline()
{
	//Read in SPIR-V code of shaders
	auto vertexShaderCode = readFile("Shaders/vert.spv");
	auto fragmentShaderCode = readFile("Shaders/frag.spv");

	//Build Shader Modules to link to Graphics Pipeline
	VkShaderModule vertexShaderModule = createShaderModule(vertexShaderCode);
	VkShaderModule fragmentShaderModule = createShaderModule(fragmentShaderCode);

	//--SHADER STAGE CREATION INFORMATION
	//Vertex Stage creation information
	VkPipelineShaderStageCreateInfo vertexShaderCreateInfo = {};
	vertexShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;									//Shader stage name
	vertexShaderCreateInfo.module = vertexShaderModule;											//Shader module to be used by stage
	vertexShaderCreateInfo.pName = "main";														//First function called on the shader (entry point)

	//Fragment Stage creation information
	VkPipelineShaderStageCreateInfo fragmentShaderCreateInfo = {};
	fragmentShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;									//Shader stage name
	fragmentShaderCreateInfo.module = fragmentShaderModule;											//Shader module to be used by stage
	fragmentShaderCreateInfo.pName = "main";														//First function called on the shader (entry point)

	//Put shader stage creation info in an array
	//Graphics pipeline creation info requires an array of that type
	VkPipelineShaderStageCreateInfo shaderStages[] = { vertexShaderCreateInfo, fragmentShaderCreateInfo };
	
	//CREATE PIPELINE

	//How the data for a single vertex(position, colour, texture coords, normals, etc) is a whole
	VkVertexInputBindingDescription vertexBindingDescription = {};
	vertexBindingDescription.binding = 0;									//Can bind multiple streams of data, this defines wich one
	vertexBindingDescription.stride = sizeof(Vertex);						//Size of a single vertex object
	vertexBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;		//How to move between data after each vertex
																			//VK_VERTEX_INPUT_RATE_VERTEX mean: move on the next vertex
																			//VK_VERTEX_INPUT_RATE_Instance: mean to a vertex for the next instance

	//How the data for an attribute is defined within a vertex
	std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions;

	//Position attribute
	attributeDescriptions[0].binding = 0;									//Which binding the data is at (should be the same as above)
	attributeDescriptions[0].location = 0;									//Location in shader data will be read from
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;			//Format the data will take (also helps define size of data)
	attributeDescriptions[0].offset = offsetof(Vertex, pos);				//Where this attirbute is defined in the data for a single vertex

	//Color attribute
	attributeDescriptions[1].binding = 0;									
	attributeDescriptions[1].location = 1;									
	attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;			
	attributeDescriptions[1].offset = offsetof(Vertex, col);
	
	//Texture attribute
	attributeDescriptions[2].binding = 0;									
	attributeDescriptions[2].location = 2;									
	attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;			
	attributeDescriptions[2].offset = offsetof(Vertex, tex);
	
	//--VERTEX INPUT--
	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
	vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputCreateInfo.pVertexBindingDescriptions = &vertexBindingDescription;										//List of vertex binding descriptions (data spacing/stride information)
	vertexInputCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();									//List of vertex attributes descriptions(data format, where to bind to or from)

	//--INPUT ASSEMBLY--
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = {};
	inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;							//Primitive type to assemble vertices as
	inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;										//Allow overriding of strip topology to start new primitive
	
	//--VIEWPORT & SCISSOR--
	//Create a viewport info struct
	VkViewport viewport = {};
	viewport.x = 0.0f;									//x start coordinate
	viewport.y = 0.0f;									//y start coordinate
	viewport.width = (float)swapChainExtent.width;
	viewport.height = (float)swapChainExtent.height;
	//Depht in vulkan is between 0 and 1
	viewport.minDepth = 0.0f;							//min framebuffer depth
	viewport.maxDepth = 1.0f;							//max framebuffer depth

	//Create a scissor info struct
	VkRect2D scissor = {};
	scissor.offset = {0,0};						//Offset to use region from
	scissor.extent = swapChainExtent;					//Extent to describe region to use, starting at offset

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &scissor;

	
	//--DYNAMIC STATES--
	//Dynamic states to enable, not in use at the moment
	// std::vector<VkDynamicState> dynamicStatesEnables;
	// dynamicStatesEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);		//Dynamic viewport: can resize in command buffer with vkCmdSetViewport(commandBuffer, 0, 1, &viewport) --> tou have to RESIZE SWAPCHAIN AND BUFFER TOO
	// dynamicStatesEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);		//Dynamic scissor: can resize in command buffer with vkCmdSetScissor(commandBuffer, 0, 1, &scissor) --> tou have to RESIZE SWAPCHAIN AND BUFFER TOO
	//
	// //Dynamic state creation info
	// VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	// dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	// dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStatesEnables.size());
	// dynamicStateCreateInfo.pDynamicStates = dynamicStatesEnables.data();

	
	//--RASTERIZER--
	VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo = {};
	rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerCreateInfo.depthClampEnable = VK_FALSE;											//Change if fragments beyond near/far planes are clipped (default) or clamped to far  (if true, physical device feature must be enabled)
	rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;									//Whether to discard data and skip rasterizer. Never creates fragments, only suitable for pipeline without framebuffer output
	rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;									//How to handle filling points between vertices
	rasterizerCreateInfo.lineWidth = 1.0f;														//How thick line should be drawn (GPU feature to enable values != 1)
	rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;										//Which face of a tri to cull
	rasterizerCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;									//Winding to determine which side is front
	rasterizerCreateInfo.depthBiasEnable = VK_FALSE;											//Whether to add depth bias to fragments (good for stopping "shadow acne" in shadow mapping)

	
	//--MULTISAMPLING--
	VkPipelineMultisampleStateCreateInfo multiSamplingCreateInfo = {};
	multiSamplingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multiSamplingCreateInfo.sampleShadingEnable = VK_FALSE;										//Enable multisample shading or not
	multiSamplingCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;						//Number of sample to use per fragment

	
	//--BLENDING--
	//Blending decides how to handle a new colour being written to a fragment. with the old value

	//Blend attachment state (how blending is handled)
	VkPipelineColorBlendAttachmentState colourState = {};
	colourState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |		//Colours to apply blending to
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colourState.blendEnable = VK_TRUE;														//Enable blending

	//Blending use equation (srcColorBlendFactor * new color) colorBlendOp (dstColorBlendFactor * old colour)
	colourState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colourState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colourState.colorBlendOp = VK_BLEND_OP_ADD;

	colourState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colourState.dstAlphaBlendFactor =VK_BLEND_FACTOR_ZERO;
	colourState.alphaBlendOp = VK_BLEND_OP_ADD;
	
	VkPipelineColorBlendStateCreateInfo colourBlendingCreateInfo = {};
	colourBlendingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colourBlendingCreateInfo.logicOpEnable = VK_FALSE;											//Alterantive to calculation is to use logical operation
	colourBlendingCreateInfo.attachmentCount = 1;
	colourBlendingCreateInfo.pAttachments = &colourState;

	//--PIPELINE LAYOUT--
	std::array<VkDescriptorSetLayout, 2> descriptorSetLayouts = {descriptorSetLayout, samplerSetLayout};
	
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
	pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

	//Create Pipeline Layout
	VkResult result = vkCreatePipelineLayout(mainDevice.logicalDevice, &pipelineLayoutCreateInfo, /*Memory management TODO*/nullptr, &pipelineLayout);
	if(result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create pipeline layout!");
	}

	//--DEPTH STENCIL TESTING--
	VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo = {};
	depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilCreateInfo.depthTestEnable = VK_TRUE;				//Enable checking depth to determine fragment write
	depthStencilCreateInfo.depthWriteEnable = VK_TRUE;				//Enable writing to depth buffer to replace old value
	depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;		//Comparison op that allows overwrite
	depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;		//Depth bounds test: does the depth value exist between two bounds?
	depthStencilCreateInfo.stencilTestEnable = VK_FALSE;			//Enable stencil test
	
	//--GRAPHICS PIPELINE CREATION--
	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	//We have 2 stages for now
	pipelineCreateInfo.stageCount = 2;									//Number of shader stages
	pipelineCreateInfo.pStages = shaderStages;							//List of shader stages
	pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;		//All the fixed functions pipeline states
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
	pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	//Not created at the moment
	pipelineCreateInfo.pDynamicState = nullptr;
	pipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;
	pipelineCreateInfo.pMultisampleState = &multiSamplingCreateInfo;
	pipelineCreateInfo.pColorBlendState = &colourBlendingCreateInfo;
	pipelineCreateInfo.pDepthStencilState = &depthStencilCreateInfo;
	pipelineCreateInfo.layout = pipelineLayout;							//Pipeline layout pipeline shoud use
	pipelineCreateInfo.renderPass = renderPass;							//Render pass description the pipeline is compatible with
	pipelineCreateInfo.subpass = 0;										//Subpass of render pass to use with pipeline

	//Pipeline derivatives: Can create multiple pipelines that derive from one another for optimization
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;				//Existing pipeline to derive from
	pipelineCreateInfo.basePipelineIndex = -1;							//or index of pipeline being created to derive from (in case creating multiple at once)

	//Create graphics pipeline
	result = vkCreateGraphicsPipelines(mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, /*Memory management TODO*/nullptr, &graphicsPipeline);
	if(result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Graphics Pipeline!");
	}
	
	//Destroy shader modules, no longer needed after pipeline creation (reverse order of creation)
	vkDestroyShaderModule(mainDevice.logicalDevice, fragmentShaderModule, /*Memory management TODO*/nullptr);
	vkDestroyShaderModule(mainDevice.logicalDevice, vertexShaderModule, /*Memory management TODO*/nullptr);
}

void VulkanRenderer::createDepthBufferImage()
{
	//Get supported format for depth buffer
	VkFormat depthFormat = chooseSupportedFormat(
		{VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT},
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

	//Create depth buffer image
	depthBufferImage = createImage(swapChainExtent.width, swapChainExtent.height,
		depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &depthBufferImageMemory);

	//Create depth buffer image view
	depthBufferImageView = createImageView(depthBufferImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void VulkanRenderer::createFrameBuffers()
{
	//Resize framebuffer count to equal swap chain image count
	swapChainFrameBuffers.resize(swapChainImages.size());

	//Create a framebuffer for each swaop chain image
	for(size_t i = 0; i < swapChainImages.size(); i++)
	{
		std::array<VkImageView, 2> attachments = {
			swapChainImages[i].imageView,
			depthBufferImageView
		}; //The order counts, as in the render pass creation!!
		
		VkFramebufferCreateInfo frameBufferCreateInfo = {};
		frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frameBufferCreateInfo.renderPass = renderPass;											//Render pass layout the frame buffer will be used with
		frameBufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		frameBufferCreateInfo.pAttachments = attachments.data();								//List of attachments (1:1 with render pass)
		frameBufferCreateInfo.width = swapChainExtent.width;									//Framebuffer widht
		frameBufferCreateInfo.height = swapChainExtent.height;									//Framebuffer height
		frameBufferCreateInfo.layers = 1;														//Framebuffer layers

		VkResult result = vkCreateFramebuffer(mainDevice.logicalDevice, &frameBufferCreateInfo, /*Memory management TODO*/nullptr, &swapChainFrameBuffers[i]);
		if(result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create framebuffer!");
		}
	}
}

void VulkanRenderer::createCommandPool()
{
	//Get indices of queue families from device
	QueueFamilyIndices queueFamilyIndices = getQueueFamiliesIndices(mainDevice.physicalDevice);
	
	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;	//Command can be reset
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;		//Queue family type that buffers from this command pool will use

	//Create a graphics queue family command pool
	VkResult result = vkCreateCommandPool(mainDevice.logicalDevice, &poolInfo, /*Memory management TODO*/nullptr, &graphicsCommandPool);
	if(result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create command pool!");
	}
}

void VulkanRenderer::createCommandBuffers()
{
	//Resize Command buffer count to mach each framebuffer
	commandBuffers.resize(swapChainFrameBuffers.size());

	VkCommandBufferAllocateInfo cbAllocInfo = {};
	cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cbAllocInfo.commandPool = graphicsCommandPool;
	cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;					//VK_COMMAND_BUFFER_LEVEL_PRIMARY: Buffer you submit directly to queue, cant be called by other buffer - VK_COMMAND_BUFFER_LEVEL_Secondary: cant be called directly, but by other buffer
	cbAllocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

	//Allocate (NOT CREATE!) command buffers and place handles in array of buffers
	VkResult result = vkAllocateCommandBuffers(mainDevice.logicalDevice, &cbAllocInfo, commandBuffers.data());
	if(result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate command buffers!");
	}
}

void VulkanRenderer::createSynchronization()
{
	imageAvailable.resize(MAX_FRAME_DRAWS);
	renderFinished.resize(MAX_FRAME_DRAWS);
	drawFences.resize(MAX_FRAME_DRAWS);
	//Semaphore creation info
	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	//Fence creation info
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for(size_t i = 0; i < MAX_FRAME_DRAWS; i++)
	{
		if(vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, /*Memory management TODO*/nullptr, &imageAvailable[i]) != VK_SUCCESS ||
		vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, /*Memory management TODO*/nullptr, &renderFinished[i]) != VK_SUCCESS ||
		vkCreateFence(mainDevice.logicalDevice, &fenceCreateInfo, /*Memory management TODO*/nullptr, &drawFences[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create a Semaphore (and/or Fence)!");
		}
	}
}

void VulkanRenderer::createTextureSampler()
{
	VkSamplerCreateInfo samplerCreateInfo ={};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;							//How to render when image is magnified on screen
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;							//How to render when image is minified on the screen
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;		//How to handle texture wrap in U direction (x)
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;		//How to handle texture wrap in V direction (y)
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;		//How to handle texture wrap in W direction (z)
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;		//Border beyond texture (only works for border clamp)
	samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;					//Whether coords should be normalize (between 0 and 1)
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;			//Blending mode between mip map level (linear interpolation)
	samplerCreateInfo.mipLodBias = 0.0f;									//LOD bias for mip levels
	samplerCreateInfo.minLod = 0.0f;										//Min LOD to pick mip level
	samplerCreateInfo.maxLod = 0.0f;										//Max LOD to pick mip level
	samplerCreateInfo.anisotropyEnable = VK_TRUE;							//Enable anisotropy
	samplerCreateInfo.maxAnisotropy = 16;									//Anisotropy sample level

	VkResult result = vkCreateSampler(mainDevice.logicalDevice, &samplerCreateInfo, nullptr, &textureSampler);
	if(result != VK_SUCCESS)
	{
		throw std::runtime_error("Filed to create a Texture Sampler!");
	}
	
}

void VulkanRenderer::createUniformBuffers()
{
	//ViewProjection Buffer size
	VkDeviceSize vpBufferSize = sizeof(UboViewProjection);

	//Model Buffer size
	// VkDeviceSize modelBufferSize = modelUniformAllignment * MAX_OBJECTS;

	//One uniform buffer for each image (and by extension, command buffer)
	vpUniformBuffer.resize(swapChainImages.size());
	vpUniformBufferMemory.resize(swapChainImages.size());
	// modelDynamicUniformBuffer.resize(swapChainImages.size());
	// modelDynamicUniformBufferMemory.resize(swapChainImages.size());

	//Create uniform buffers
	for(size_t i = 0; i < swapChainImages.size(); i++)
	{
		createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, vpBufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&vpUniformBuffer[i], &vpUniformBufferMemory[i]);
		
		// createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, modelBufferSize,
		// 	VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		// 	&modelDynamicUniformBuffer[i], &modelDynamicUniformBufferMemory[i]);
	}
}

void VulkanRenderer::createDescriptorPool()
{
	//CREATE UNIFORM DESCRIPTOR POOL
	
	//Type of descriptors + how many DESCRIPTORS, not Descriptor Sets (combined makes the pool size)
	//ViewProjection Pool
	VkDescriptorPoolSize vpPoolSize = {};
	vpPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	vpPoolSize.descriptorCount = static_cast<uint32_t>(vpUniformBuffer.size());

	// //Model Pool (Dynamic)
	// VkDescriptorPoolSize modelPoolSize = {};
	// modelPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	// modelPoolSize.descriptorCount = static_cast<uint32_t>(modelDynamicUniformBuffer.size());

	//List of Pool size
	std::vector<VkDescriptorPoolSize> descriptorPoolSizes = {vpPoolSize/*, modelPoolSize*/};
	
	//Data to create descriptor pool
	VkDescriptorPoolCreateInfo poolCreateInfo = {};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.maxSets = static_cast<uint32_t>(swapChainImages.size());					//Max num of descriptor sets thet can be created from the pool
	poolCreateInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());		//Amount of Pool Sizes being passed
	poolCreateInfo.pPoolSizes = descriptorPoolSizes.data();									//Pool Sizes to create pool with

	//Create descriptor pool
	VkResult result = vkCreateDescriptorPool(mainDevice.logicalDevice, &poolCreateInfo, nullptr, &descriptorPool);
	if(result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a descriptor pool!");
	}

	//CREATE SAMPLER DESCRIPTOR POOL
	//Texture sampler pool
	VkDescriptorPoolSize samplerPoolSize = {};
	samplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;							//Not really optimal
	samplerPoolSize.descriptorCount = MAX_OBJECTS;

	//Data to create sampler descriptor pool
	VkDescriptorPoolCreateInfo samplerPoolCreateInfo = {};
	samplerPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	samplerPoolCreateInfo.maxSets = MAX_OBJECTS;					
	samplerPoolCreateInfo.poolSizeCount = 1;		
	samplerPoolCreateInfo.pPoolSizes = &samplerPoolSize;									

	result = vkCreateDescriptorPool(mainDevice.logicalDevice, &samplerPoolCreateInfo, nullptr, &samplerDescriptorPool);
	if(result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create sampler descriptor pool!");
	}
}

void VulkanRenderer::createDescriptorSets()
{
	//Resize descriptor sets list so one for each buffer
	descriptorSets.resize(swapChainImages.size());

	std::vector<VkDescriptorSetLayout> setLayouts(swapChainImages.size(), descriptorSetLayout);

	//Descriptor set allocation info
	VkDescriptorSetAllocateInfo setAllocInfo = {};
	setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocInfo.descriptorPool = descriptorPool;									//Pool to allocate descriptor sets from
	setAllocInfo.descriptorSetCount = static_cast<uint32_t>(swapChainImages.size());//Number of sets to allocate
	setAllocInfo.pSetLayouts = setLayouts.data();									//Layouts to use to allocate sets (1:1 relationship)

	//Allocate descriptor sets
	VkResult result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &setAllocInfo, descriptorSets.data());
	if(result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a descriptor sets!");
	}

	//Update all of descriptor set buffer binding
	for(size_t i = 0; i < swapChainImages.size(); i++)
	{
		//View PROJECTION DESCRIPTOR
		//Buffer info and data offset info
		VkDescriptorBufferInfo vpBufferInfo = {};
		vpBufferInfo.buffer = vpUniformBuffer[i];		//Buffer to get data from
		vpBufferInfo.offset = 0;						//Position of start of data
		vpBufferInfo.range = sizeof(UboViewProjection);	//Size of data

		//Data about connection between binding and buffer
		VkWriteDescriptorSet vpSetWrite = {};
		vpSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		vpSetWrite.dstSet = descriptorSets[i];							//Descriptor set to update
		vpSetWrite.dstBinding = 0;										//Binding to update (has to match with shader/layout)
		vpSetWrite.dstArrayElement = 0;								//Index in array to update
		vpSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;	//Type of descriptor
		vpSetWrite.descriptorCount = 1;								//Amount to update
		vpSetWrite.pBufferInfo = &vpBufferInfo;						//Info about buffer data to bind

		// //Model DESCRIPTOR
		// //Model Buffer info and data offset info
		// VkDescriptorBufferInfo modelBufferInfo = {};
		// modelBufferInfo.buffer = modelDynamicUniformBuffer[i];	//Buffer to get data from
		// modelBufferInfo.offset = 0;							//Position of start of data
		// modelBufferInfo.range = modelUniformAllignment;		//Size of data
		//
		// //Data about connection between binding and buffer
		// VkWriteDescriptorSet modelSetWrite = {};
		// modelSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		// modelSetWrite.dstSet = descriptorSets[i];							//Descriptor set to update
		// modelSetWrite.dstBinding = 1;										//Binding to update (has to match with shader/layout)
		// modelSetWrite.dstArrayElement = 0;									//Index in array to update
		// modelSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;	//Type of descriptor
		// modelSetWrite.descriptorCount = 1;									//Amount to update
		// modelSetWrite.pBufferInfo = &modelBufferInfo;							//Info about buffer data to bind

		//List of descriptor sets write
		std::vector<VkWriteDescriptorSet> setWrites = {vpSetWrite/*, modelSetWrite*/};
		
		//Update the descriptor set with new buffer/binding info
		vkUpdateDescriptorSets(mainDevice.logicalDevice, static_cast<uint32_t>(setWrites.size()), setWrites.data(), 0, nullptr);
	}
}

void VulkanRenderer::updateUniformBuffers(uint32_t imageIndex)
{
	//Copy VP data
	void * data;
	vkMapMemory(mainDevice.logicalDevice, vpUniformBufferMemory[imageIndex], 0, sizeof(UboViewProjection), 0, &data);
	memcpy(data, &uboViewProjection, sizeof(UboViewProjection));
	vkUnmapMemory(mainDevice.logicalDevice, vpUniformBufferMemory[imageIndex]);

	//NOT IN USE ANYMORE (Model used not with UBOD, but with push_constant)
	// //Copy Model data
	// for(size_t i = 0; i < meshList.size(); i++)
	// {
	// 	Model* thisModel = (Model*)((uint64_t)modelTransferSpace + (i * modelUniformAllignment));
	// 	*thisModel = meshList[i].getModel();
	// }
	//
	// //Map the list of model data
	// vkMapMemory(mainDevice.logicalDevice, modelDynamicUniformBufferMemory[imageIndex], 0, modelUniformAllignment * meshList.size(), 0, &data);
	// memcpy(data, modelTransferSpace, modelUniformAllignment * meshList.size());
	// vkUnmapMemory(mainDevice.logicalDevice, modelDynamicUniformBufferMemory[imageIndex]);
}

void VulkanRenderer::recordCommands(uint32_t currentImage)
{
	//Information about to begin each command buffer
	VkCommandBufferBeginInfo bufferBeginInfo = {};
	bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	//Not relevant with fences introducted and MAX_FRAME_DRAWS = 2
	//bufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;	//Buffer can be resubmitted when it has already benn submitted during the execution

	//Information about how to begin a render pass (only needed for graphical application)
	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = renderPass;							//Render pass to begin
	renderPassBeginInfo.renderArea.offset = { 0, 0 };				//Start point of render pass in pixels
	renderPassBeginInfo.renderArea.extent = swapChainExtent;				//Size of region to run render pass on (starting at offset)

	std::array<VkClearValue, 2> clearValues = {};
	clearValues[0].color = {0.6f, 0.65f, 0.4f, 1.0f};
	clearValues[1].depthStencil.depth = 1.0f;

	renderPassBeginInfo.pClearValues = clearValues.data();							//List of clear values
	renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	
	
	renderPassBeginInfo.framebuffer = swapChainFrameBuffers[currentImage];
	
	//Start recording commands to command buffer
	VkResult result = vkBeginCommandBuffer(commandBuffers[currentImage],&bufferBeginInfo);
	if(result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to start recording command buffers!");
	}

		//Begin render pass
		vkCmdBeginRenderPass(commandBuffers[currentImage], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			//Bind pipeline to be use in render pass
			vkCmdBindPipeline(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

			for(size_t j = 0; j < meshList.size(); j++)
			{
				VkBuffer vertexBuffer[] = {meshList[j].getVertexBuffer()};									//Buffers to bind
				VkDeviceSize offsets[] = {0};																//Offsets into buffers being bound
				vkCmdBindVertexBuffers(commandBuffers[currentImage], 0, 1, vertexBuffer, offsets);	//Command to bind vertex buffer whith them

				//Bind mesh index buffer, with 0 offset using uint32 type
				vkCmdBindIndexBuffer(commandBuffers[currentImage], meshList[j].getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

				// //Dynamic offset amount
				// uint32_t dynamicOffset = static_cast<uint32_t>(modelUniformAllignment) * j;

				//"Push" constant to given shader stage directly (no buffer)
				vkCmdPushConstants(
					commandBuffers[currentImage],
					pipelineLayout,
					VK_SHADER_STAGE_VERTEX_BIT,				//Stage to push constant to
					0,										//Offset of push constant to update
					sizeof(Model),							//Size of data being pushed
					meshList[j].getModelPointer());				//Actual data being pushed

				std::array<VkDescriptorSet, 2> descriptorSetGroup = {
					descriptorSets[currentImage],
					samplerDescriptorSets[meshList[j].getTexId()]
				};
				
				//Bind descriptor sets
				vkCmdBindDescriptorSets(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
					0, static_cast<uint32_t>(descriptorSetGroup.size()), descriptorSetGroup.data(), 0, nullptr);
				
				//Execute pipeline
				vkCmdDrawIndexed(commandBuffers[currentImage], meshList[j].getIndexCount(), 1, 0, 0, 0);
			}
			
		//End render pass
		vkCmdEndRenderPass(commandBuffers[currentImage]);
	
	//Stop recording to command buffer
	result = vkEndCommandBuffer(commandBuffers[currentImage]);
	if(result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to stop recording command buffers!");
	}
}

//We just get the hardware GPU, so no creation of object and no need to destroy nothing about physical device
void VulkanRenderer::getPhysicalDevice()
{
	//Enumerate Physical devices the vkInstance can access
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	// If no devices available, then none supports Vulkan
	if (deviceCount == 0)
	{
		throw std::runtime_error("Can't find GPUs that support Vulkan Instance!");
	}

	//Get list of Physical Devices
	std::vector<VkPhysicalDevice> deviceList(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, deviceList.data());

	for (const auto& device : deviceList)
	{
		if (checkDeviceSuitable(device))
		{
			mainDevice.physicalDevice = device;
			break;
		}
	}

	//Get properties of gpu
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(mainDevice.physicalDevice, &deviceProperties);

	// minUniformBufferOffset = deviceProperties.limits.minUniformBufferOffsetAlignment;
}

std::vector<const char*> VulkanRenderer::getRequiredExtensions()
{

	//The extensions specified by GLFW are always required, but the debug messenger extension is conditionally added
	//Set up extensions Instance will use
	uint32_t glfwExtensionCount = 0;						//GLFW may require multiple extensions
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	if (enableValidationLayers) {
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
	}

	return extensions;
}

void VulkanRenderer::allocateDynamicBufferTransferSpace()
{
	//NOT IN USE ANYMORE (Model used not with UBOD, but with push_constant)
	// //Calculate allignment of model data
	// //Bit operation masterclass
	// modelUniformAllignment = (sizeof(Model) + minUniformBufferOffset - 1 )
	// 						& ~(minUniformBufferOffset - 1);
	//
	// //Create space in memory to hold dynamic buffer that is alligned to our required allignment and holds MAX_OBJECTS
	// modelTransferSpace = (Model *)_aligned_malloc(modelUniformAllignment * MAX_OBJECTS, modelUniformAllignment);
}

void VulkanRenderer::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
	createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;		//Specify all the types of severities you would like your callback to be called for
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;			//Filter which types of messages your callback is notified about
	createInfo.pfnUserCallback = debugCallback;																																			//Specifies the pointer to the callback function
	createInfo.pUserData = nullptr; // Optional
}

bool VulkanRenderer::checkInstanceExtensionsSupport(std::vector<const char*>* checkExtensions)
{
	//Need to get the number of extensions to hold extensions
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(/*Layer*/nullptr, &extensionCount, nullptr);

	//Create a list of vkExtensionsProperties using the counter
	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

	//Check if given extensions are in the list of available extensions
	for (const auto& checkExtensions : *checkExtensions)
	{
		bool hasExtensions = false;
		for (const auto& extension : extensions)
		{
			if (strcmp(checkExtensions, extension.extensionName))
			{
				hasExtensions = true;
				break;
			}
		}
		if (!hasExtensions) return false;
	}

	return true;
}

bool VulkanRenderer::checkDeviceExtensionsSupport(VkPhysicalDevice device)
{
	//Need to get the number of extensions to hold extensions
	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	if (extensionCount == 0)return false;

	//Populate extensions
	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());

	//Check if given extensions are in the list of available extensions
	for (const auto& deviceExtension : deviceExtensions)
	{
		bool hasExtensions = false;
		for (const auto& extension : extensions)
		{
			if (strcmp(deviceExtension, extension.extensionName))
			{
				hasExtensions = true;
				break;
			}
		}
		if (!hasExtensions) return false;
	}
	return true;
}

bool VulkanRenderer::checkDeviceSuitable(VkPhysicalDevice device)
{
	
	// //Information about the device itself
	// VkPhysicalDeviceProperties deviceProperties;
	// vkGetPhysicalDeviceProperties(device, &deviceProperties);

	//Information about what the device can do (geometry shader, tessellation shader, wide lines, etc)
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
	

	QueueFamilyIndices indices = getQueueFamiliesIndices(device);

	bool extensionsSupported = checkDeviceExtensionsSupport(device);

	bool swapChainValid = false;
	if (extensionsSupported)
	{
		SwapChainDetails swapChainDetails = getSwapChainDetails(device);
		swapChainValid = !swapChainDetails.formats.empty() && !swapChainDetails.presentationModes.empty();
	}
	
	return indices.isValid() && extensionsSupported && swapChainValid && deviceFeatures.samplerAnisotropy;
}

bool VulkanRenderer::checkValidationLayerSupport()
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	//Check if all of the layers in validationLayers exist in the availableLayers list
	for (const char* layerName : validationLayers) {
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers) {
			if (strcmp(layerName, layerProperties.layerName) == 0) {
				layerFound = true;
				break;
			}
		}

		if (!layerFound) {
			return false;
		}
	}

	return true;
}

QueueFamilyIndices VulkanRenderer::getQueueFamiliesIndices(VkPhysicalDevice device)
{
	QueueFamilyIndices indices;

	//Get all Queue family Property info for the given device
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilyList(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyList.data());
	
	//Go through each queue family and check if it has at least 1 of the required types of queue
	int i=0;
	for (const auto& queueFamily : queueFamilyList)
	{
		//First check if queue family has at least 1 queue in that family
		//Queue can be multiple types defined trough bitfield. Need to bitwise AND with VK_QUEUE_GRAPHICS_BIT
		if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)//& bitwise and
		{
			indices.graphicsFamily = i;			//If queue family is valid, then get index
		}

		//Check if queue family support presentation
		VkBool32 presentationSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device,i,surface,&presentationSupport);
		//Check if queue is presentation type (can be both graphics and presentation)
		if (queueFamily.queueCount > 0 && presentationSupport)
		{
			indices.presentationFamily = i;			//If queue family is valid, then get index
		}

		//Check if queue family indices are in a valid state, stop searching it
		if (indices.graphicsFamily)
		{
			break;
		}
		i++;
	}

	return indices;
}

SwapChainDetails VulkanRenderer::getSwapChainDetails(VkPhysicalDevice device)
{
	SwapChainDetails swapChainDetails;

	//--CAPABILITIES--
	//Get the surface capabilities for the given surface on the given physical device
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &swapChainDetails.surfaceCapabilities);

	//--FORMATS--
	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
	
	//If format returned, get list of format
	if (formatCount != 0)
	{
		//Resize because this vector was already created
		swapChainDetails.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, swapChainDetails.formats.data());
	}

	//--PRESENTATION MODES--
	uint32_t presentationCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentationCount, nullptr);

	//If presentation modes returned, get list of format
	if (presentationCount != 0)
	{
		//Resize because this vector was already created
		swapChainDetails.presentationModes.resize(formatCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentationCount, swapChainDetails.presentationModes.data());
	}

	return swapChainDetails;
}

//Best format is subjective, ours will be
//Format: VK_FORMAT_R8G8B8A8_UNORM (as backup VK_FORMAT_B8G8R8A8_UNORM)
//colorSpace: VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
VkSurfaceFormatKHR VulkanRenderer::chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
{
	//In this case all the formats are available
	if(formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
	{
		//Pick the right one 
		return {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
	}

	for(const auto &format : formats)
	{
		if((format.format == VK_FORMAT_R8G8B8A8_UNORM || format.format == VK_FORMAT_B8G8R8A8_UNORM)
			&& format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return format;
		}
	}

	//Return first
	return formats[0];
}

VkPresentModeKHR VulkanRenderer::chooseBestPresentationMode(const std::vector<VkPresentModeKHR>& presentationModes)
{
	for(const auto &presentationMode : presentationModes)
	{
		//We want mailbox
		if(presentationMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return presentationMode;
		}
	}

	//This always has to be available
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities)
{
	//If current extent is at numeric limits, then it can vary. Otherwise, it is the size of the window
	if(surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return surfaceCapabilities.currentExtent;
	}
	else
	{
		//If value can vary, need to set manually

		//Get window size
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		//Create new extent using window size
		VkExtent2D newExtent = {};
		newExtent.width = static_cast<uint32_t>(width);
		newExtent.height = static_cast<uint32_t>(height);

		//Surface also defines max and min, so make sure within boundaries clamping value
		newExtent.width = std::max(surfaceCapabilities.minImageExtent.width,std::min(surfaceCapabilities.maxImageExtent.width,newExtent.width));
		newExtent.height = std::max(surfaceCapabilities.minImageExtent.height,std::min(surfaceCapabilities.maxImageExtent.height,newExtent.height));

		return newExtent;
	}
}

VkFormat VulkanRenderer::chooseSupportedFormat(const std::vector<VkFormat>& formats, VkImageTiling tiling,
	VkFormatFeatureFlags featureFlags)
{
	//Loop through options and find a compatible one
	for(VkFormat format : formats)
	{
		//Get properties for given format on this device
		VkFormatProperties properties;
		vkGetPhysicalDeviceFormatProperties(mainDevice.physicalDevice, format, &properties);

		//Depending on tiling choice, need to check for different bit flag
		if(tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & featureFlags) == featureFlags)
		{
			return format;
		}
		else if(tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & featureFlags) == featureFlags)
		{
			return format;
		}
	}

	throw std::runtime_error("Failed to find a matching format!");
}

VkImage VulkanRenderer::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                                    VkImageUsageFlags useFlags, VkMemoryPropertyFlags propFlags, VkDeviceMemory* imageMemory)
{
	//CREATE IMAGE
	//Image Creation Info
	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;						//Type of image
	imageCreateInfo.extent.width = width;								//Widht of image extent
	imageCreateInfo.extent.height = height;								//Height of image extent
	imageCreateInfo.extent.depth = 1;									//Depth of image extent (just 1, no 3d aspect)
	imageCreateInfo.mipLevels = 1;										//Number of mipmap levels
	imageCreateInfo.arrayLayers = 1;									//Number of levels in image array
	imageCreateInfo.format = format;									//Format type of image
	imageCreateInfo.tiling = tiling;									//How image data should be tiled (arranged for optimal reading)
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;			//Layout of image data on creation
	imageCreateInfo.usage = useFlags;									//Bit flags defining what image will be used for
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;					//Number of samples for multisampling
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;			//Whether image can be shared between queues

	//Create an image
	VkImage image;
	VkResult result = vkCreateImage(mainDevice.logicalDevice, &imageCreateInfo, nullptr, &image);
	if(result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create an Image!");
	}
	
	//CREATE MEMORY FOR IMAGE
	//Get memory requirements for a type of image
	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(mainDevice.logicalDevice, image, &memoryRequirements);

	//Allocate memory for an image using requirements and user defined props
	VkMemoryAllocateInfo memoryAllocInfo = {};
	memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocInfo.allocationSize = memoryRequirements.size;
	memoryAllocInfo.memoryTypeIndex = findMemoryTypeIndex(mainDevice.physicalDevice, memoryRequirements.memoryTypeBits, propFlags);

	result = vkAllocateMemory(mainDevice.logicalDevice, &memoryAllocInfo, nullptr, imageMemory);
	if(result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate memory for an Image!");
	}

	//Connect memory to image
	result = vkBindImageMemory(mainDevice.logicalDevice, image, *imageMemory, 0);
	if(result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to bind memory to an Image!");
	}

	return image;
}

VkImageView VulkanRenderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
	VkImageViewCreateInfo viewCreateInfo = {};
	viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCreateInfo.image = image;												//Image to create view for
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;							//Type of image(1d, 2d, ...)
	viewCreateInfo.format = format;												//Format of image data
	viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;				//Allows remapping of rgba channels
	viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

	//Subresources allow the view only a part of an image
	viewCreateInfo.subresourceRange.aspectMask = aspectFlags;					//Wich aspect of image to view (e.g. COLOR_BIT for view colour)
	viewCreateInfo.subresourceRange.baseMipLevel = 0;							//Start mipmap level to view from
	viewCreateInfo.subresourceRange.levelCount = 1;								//Number of mipmap level to view
	viewCreateInfo.subresourceRange.baseArrayLayer = 0;							//Start array level to view from
	viewCreateInfo.subresourceRange.layerCount = 1;								//Number of array level to view

	//Create image view and return it
	VkImageView imageView;
	VkResult result = vkCreateImageView(mainDevice.logicalDevice, &viewCreateInfo, /*Memory management TODO*/nullptr, &imageView);

	if(result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create image view!");
	}

	return imageView;
}

VkShaderModule VulkanRenderer::createShaderModule(const std::vector<char>& code)
{
	//Shader Module creation information
	VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.codeSize = code.size();										//Size of code
	shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());		//Pointer to code (of uint32_t pointer type)

	VkShaderModule shaderModule;
	VkResult result = vkCreateShaderModule(mainDevice.logicalDevice, &shaderModuleCreateInfo, /*Memory management TODO*/nullptr, &shaderModule);

	if(result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a shader module!");
	}

	return shaderModule;
}

int VulkanRenderer::createTextureImage(std::string fileName)
{
	//Load image file
	int width, height;
	VkDeviceSize imageSize;
	stbi_uc* imageData = loadTextureFile(fileName, &width, &height, &imageSize);

	//Create staging buffer to hold loaded data, ready to copy to device
	VkBuffer imageStagingBuffer;
	VkDeviceMemory imageStagingBufferMemory;
	createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&imageStagingBuffer, &imageStagingBufferMemory);

	//Copy image data to staging buffer
	void* data;
	vkMapMemory(mainDevice.logicalDevice, imageStagingBufferMemory, 0, imageSize, 0, &data);
	memcpy(data, imageData, static_cast<size_t>(imageSize));
	vkUnmapMemory(mainDevice.logicalDevice, imageStagingBufferMemory);

	//Free original image data
	stbi_image_free(imageData);

	//Create image to hold final texture
	VkImage texImage;
	VkDeviceMemory texImageMemory;
	texImage = createImage(width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&texImageMemory);

	//COPY DATA TO IMAGE
	//Transition image to dst for copy operation
	transitionImageLayout(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, texImage,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	
	//Copy image data
	copyImageBuffer(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool,
		imageStagingBuffer, texImage, width, height);

	//Transition image to be shader readable for shader usage
	transitionImageLayout(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, texImage,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	

	//Add texture data to vector for reference
	textureImages.push_back(texImage);
	textureImagesMemory.push_back(texImageMemory);

	//Destroy staging buffer
	vkDestroyBuffer(mainDevice.logicalDevice, imageStagingBuffer, nullptr);
	vkFreeMemory(mainDevice.logicalDevice, imageStagingBufferMemory, nullptr);

	//Return index of new texture
	return textureImages.size() - 1; 
}

int VulkanRenderer::createTexture(std::string fileName)
{
	//Create texture image and get its location in array
	int textureImageLoc = createTextureImage(fileName);

	//Create Image View and add to list
	VkImageView imageView = createImageView(textureImages[textureImageLoc], VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
	textureImageViews.push_back(imageView);

	//Create Texture descriptor
	int descriptorLoc = createTextureDescriptor(imageView);

	//Return location of set with texture
	return descriptorLoc;
}

int VulkanRenderer::createTextureDescriptor(VkImageView textureImage)
{
	VkDescriptorSet descriptorSet;

	//Descriptor Set Allocation info
	VkDescriptorSetAllocateInfo setAllocInfo = {};
	setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocInfo.descriptorPool = samplerDescriptorPool;
	setAllocInfo.descriptorSetCount = 1;
	setAllocInfo.pSetLayouts = &samplerSetLayout;

	//Allocate descriptor set
	VkResult result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &setAllocInfo, &descriptorSet);
	if(result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate Texture Descriptor Set!");
	}

	//Texture image info
	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;				//Image layout when in use
	imageInfo.imageView = textureImage;												//Image to bind to set
	imageInfo.sampler = textureSampler;												//Sampler to use for set

	//Descriptor Write Info
	VkWriteDescriptorSet descriptorWrite = {};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = descriptorSet;
	descriptorWrite.dstBinding = 0;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pImageInfo = &imageInfo;

	//Update new descriptor set
	vkUpdateDescriptorSets(mainDevice.logicalDevice, 1, &descriptorWrite, 0, nullptr);

	//Add descriptor set to list
	samplerDescriptorSets.push_back(descriptorSet);

	//Return descriptor set location
	return samplerDescriptorSets.size() -1;
}

stbi_uc* VulkanRenderer::loadTextureFile(std::string fileName, int* width, int* height, VkDeviceSize* imageSize)
{
	//Number of channels image uses
	int channels;

	//Load pixel data for image
	std::string fileLoc = "Textures/" + fileName;
	stbi_uc* image = stbi_load(fileLoc.c_str(), width, height, &channels, STBI_rgb_alpha);

	if(!image)
	{
		throw std::runtime_error("Failed to load texture file! (" + fileName + ")");
	}

	//Calculate image size
	*imageSize = *width * *height * 4;

	return image;
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanRenderer::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
	switch(messageSeverity)
	{
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
			std::cerr << "[Validation layer] verbose: " << pCallbackData->pMessage << std::endl;
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
			std::cerr << "[Validation layer] info: " << pCallbackData->pMessage << std::endl;
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			std::cerr << "[Validation layer] warning: " << pCallbackData->pMessage << std::endl;
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			std::cerr << "[Validation layer] error: " << pCallbackData->pMessage << std::endl;
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT:
			std::cerr << "[Validation layer] fatal: " << pCallbackData->pMessage << std::endl;
			break;
		default:
			break;
	}
	//The callback returns a boolean that indicates if the Vulkan call that triggered the validation layer message should be aborted
	//Genarally return false (best practice)
	return VK_FALSE;
}