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
		createGraphicsPipeline();
		createFrameBuffers();
		// createCommandPool();
		// createCommandBuffers();
		// recordCommands();
	}
	catch (const std::runtime_error& e) {
		printf("ERROR: %s", e.what());
		return EXIT_FAILURE;
	}


	return EXIT_SUCCESS;
}

void VulkanRenderer::cleanup()
{
	//Reverse order than creation
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
	//Colour attachment of render pass
	VkAttachmentDescription colourAttaChment = {};
	colourAttaChment.format = swapChainImageFormat;						//Format to use for attachment
	colourAttaChment.samples = VK_SAMPLE_COUNT_1_BIT;					//Number of samples to write for multisampling
	colourAttaChment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;				//Describes what to do with attachment before rendering
	colourAttaChment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;			//Describes what to do with attachment after rendering
	colourAttaChment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;	//Describes what to do with stencil before rendering
	colourAttaChment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;	//Describes what to do with stencil after rendering

	//Framebuffer data will be stored as an image, but images can be given different data layouts
	//to give optimal use for certain operation
	colourAttaChment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;			//Image data layout before render pass start
	colourAttaChment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;		//Image data layout after render pass (to change to)

	//Attachment reference uses in attachment index that refers to index in the attachments list passed to renderPassCreateInfo
	VkAttachmentReference colourAttachmentReference = {};
	colourAttachmentReference.attachment = 0;										//Index in the renderpass attachment list
	colourAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;	//Layout between initial and final of render pass
	
	//Information about the paticular subpass the render pass is using
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;		//Pipeline type subpass is bound to
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colourAttachmentReference;

	
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
	
	//Create info for render pass
	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = 1;
	renderPassCreateInfo.pAttachments = &colourAttaChment;
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
	//--VERTEX INPUT (TODO: put vertex description when resources created)--
	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
	vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputCreateInfo.vertexBindingDescriptionCount = 0;
	vertexInputCreateInfo.pVertexBindingDescriptions = nullptr;										//List of vertex binding descriptions (data spacing/stride information)
	vertexInputCreateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputCreateInfo.pVertexAttributeDescriptions = nullptr;									//List of vertex attributes descriptions(data format, where to bind to or from)

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
	rasterizerCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;									//Winding to determine which side is front
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


	//--PIPELINE LAYOUT (TODO: Apply Future Descriptor Set Layouts)--
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = 0;
	pipelineLayoutCreateInfo.pSetLayouts = nullptr;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

	//Create Pipeline Layout
	VkResult result = vkCreatePipelineLayout(mainDevice.logicalDevice, &pipelineLayoutCreateInfo, /*Memory management TODO*/nullptr, &pipelineLayout);
	if(result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create pipeline layout!");
	}

	//--DEPTH STENCIL TESTING--
	//TODO: Set up depth stencil testing


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
	//Not created at the moment
	pipelineCreateInfo.pDepthStencilState = nullptr;
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

void VulkanRenderer::createFrameBuffers()
{
	//Resize framebuffer count to equal swap chain image count
	swapChainFrameBuffers.resize(swapChainImages.size());

	//Create a framebuffer for each swaop chain image
	for(size_t i = 0; i < swapChainImages.size(); i++)
	{
		std::array<VkImageView, 1> attachments = {
			swapChainImages[i].imageView
		};
		
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

void VulkanRenderer::recordCommands()
{
	//Information about to begin each command buffer
	VkCommandBufferBeginInfo bufferBeginInfo = {};
	bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	bufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;	//Buffer can be resubmitted when it has already benn submitted during the execution

	//Information about how to begin a render pass (only needed for graphical application)
	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = renderPass;							//Render pass to begin
	renderPassBeginInfo.renderArea.offset = { 0, 0 };				//Start point of render pass in pixels
	renderPassBeginInfo.renderArea.extent = swapChainExtent;				//Size of region to run render pass on (starting at offset)
	VkClearValue clearValues[] = {
		{0.6f, 0.65f, 0.4f, 1.0f}
	};
	renderPassBeginInfo.pClearValues = clearValues;							//List of clear values (TODO: Depth attachment clear value)
	renderPassBeginInfo.clearValueCount = 1;
	
	for(size_t i = 0; i < commandBuffers.size(); i++)
	{
		renderPassBeginInfo.framebuffer = swapChainFrameBuffers[i];
		
		//Start recording commands to command buffer
		VkResult result = vkBeginCommandBuffer(commandBuffers[i],&bufferBeginInfo);
		if(result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to start recording command buffers!");
		}

			//Begin render pass
			vkCmdBeginRenderPass(commandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

				//Bind pipeline to be use in render pass
				vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

				//Execute pipeline
				vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);
		
			//End render pass
			vkCmdEndRenderPass(commandBuffers[i]);
		
		//Stop recording to command buffer
		result = vkEndCommandBuffer(commandBuffers[i]);
		if(result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to stop recording command buffers!");
		}
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
	/*
	//Information about the device itself
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);

	//Information about what the device can do (geometry shader, tessellation shader, wide lines, etc)
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
	*/

	QueueFamilyIndices indices = getQueueFamiliesIndices(device);

	bool extensionsSupported = checkDeviceExtensionsSupport(device);

	bool swapChainValid = false;
	if (extensionsSupported)
	{
		SwapChainDetails swapChainDetails = getSwapChainDetails(device);
		swapChainValid = !swapChainDetails.formats.empty() && !swapChainDetails.presentationModes.empty();
	}
	
	return indices.isValid() && extensionsSupported && swapChainValid;
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