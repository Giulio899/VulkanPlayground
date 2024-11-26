#pragma once

#include <fstream>

#define GLFW_INCLUDE_VULKAN

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

const int MAX_FRAME_DRAWS = 2;
const int MAX_OBJECTS = 2;

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

//Vertex Data representation
struct Vertex
{
	glm::vec3 pos; //Vertex position (x,y,z)
	glm::vec3 col; //Vertex colour (r,g,b)
	glm::vec2 tex; //Texture coords (u,v)
};

// Indices (locations) of Queue Families (if they exist at all)
struct QueueFamilyIndices {
	int graphicsFamily = -1;				//Location of Graphic Queue Family
	int presentationFamily = -1;			//Location of Presentation Queue Family
	
	//Check if Queue families are valid
	bool isValid() 
	{
		return graphicsFamily >= 0 && presentationFamily >= 0;
	}
};

struct SwapChainDetails {
	VkSurfaceCapabilitiesKHR surfaceCapabilities;				//Surface properties, e.g. image size/extent
	std::vector<VkSurfaceFormatKHR> formats;					//Surface image formats, e.g. RGBA and size of each colour
	std::vector<VkPresentModeKHR> presentationModes;			//How image should be presented to screen (remember tearing)
};

struct SwapChainImage
{
	VkImage image;
	VkImageView imageView;
};

static std::vector<char> readFile(const std::string &filename)
{
	//Open stream from given file
	//std::ios::binary tells stream to read it as binary
	//std::ios::ate tells stream to start reading from end of file
	std::ifstream file(filename, std::ios::binary | std::ios::ate);

	//Check if file stream successfully opened
	if(!file.is_open())
	{
		throw std::runtime_error("Failed to open a file");
	}

	//Get current read position and use to resize file buffer
	size_t fileSize = (size_t)file.tellg();
	std::vector<char> fileBuffer(fileSize);

	//Move read position to start of file
	file.seekg(0);

	//Read file data into the buffer
	file.read(fileBuffer.data(), fileSize);

	//Close stream
	file.close();

	return fileBuffer;
}

static uint32_t findMemoryTypeIndex(VkPhysicalDevice physicalDevice, uint32_t allowedTypes, VkMemoryPropertyFlags properties)
{
	//Get properties of physical device memory
	VkPhysicalDeviceMemoryProperties memoryProperties = {};
	vkGetPhysicalDeviceMemoryProperties(physicalDevice,&memoryProperties);

	for(uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
	{
		//Bit shifting masterclass in the if
		if((allowedTypes & (1 << i))//Index of memory type must match corrisponding bit in allowedTypes
			&& (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)//Desired property bit flags are part of memory type's property flags                       
		{
			//This memory type i valid, so return it
			return i;
		}
	}
}

static void createBuffer(VkPhysicalDevice physicalDevice,
	VkDevice device, VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage,
	VkMemoryPropertyFlags bufferProperties, VkBuffer* buffer, VkDeviceMemory* bufferMemory)
{
	//CREATE VERTEX BUFFER
    //Information to create a buffer (doesn't include assigning memory)
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;									//Size of buffer (size of 1 vertex * number of vertices)
    bufferInfo.usage = bufferUsage;									 //Multiple types of buffer possible
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;             //Similar to swapchain images, can share vertex buffer

    VkResult result = vkCreateBuffer(device, &bufferInfo, /*Memory management TODO*/nullptr, buffer);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a Vertex Buffer!");
    }

    //GET BUFFER MEMORY REQUIREMENTS
    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(device, *buffer, &memoryRequirements);

    //ALLOCATE MEMORY BUFFER
    VkMemoryAllocateInfo memoryAllocInfo = {};
    memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocInfo.allocationSize = memoryRequirements.size;
    memoryAllocInfo.memoryTypeIndex = findMemoryTypeIndex(physicalDevice, memoryRequirements.memoryTypeBits, bufferProperties);       //Index of memory type on Physical Device thet has required bit flags
                                        																										//VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT: CPU can interact with memory
																																				//VK_MEMORY_PROPERTY_HOST_COHERENT_BIT: Allows placement of data straight into buffer after mapping (otherwise would have to specify manually)
    //Allocate memory to VkDeviceMemory
    result = vkAllocateMemory(device, &memoryAllocInfo,/*Memory management TODO*/nullptr, bufferMemory);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate Vertex Buffer Memory!");
    }

    //Allocate memory to given vertex buffer
    vkBindBufferMemory(device, *buffer, *bufferMemory, 0);
}

static VkCommandBuffer beginCommandBuffer(VkDevice device, VkCommandPool commandPool)
{
	//Command buffer to hold transfer commands
	VkCommandBuffer commandBuffer;

	//Command buffer details
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	//Allocate command buffer from pool
	vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

	//Information to begin the command buffer record
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;	//We are only using the command buffer once

	//Begin recording tranfer commands
	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

static void endAndSubmitCommandBuffer(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkCommandBuffer commandBuffer)
{
	//End commands
	vkEndCommandBuffer(commandBuffer);

	//Queue submission information
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	//Submit transfer command to tranfer queue
	vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
	//and wait until TODO: Change -> not ideal for loading a lot of meshes
	vkQueueWaitIdle(queue);

	//Free temporary command buffer back to pool
	vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

static void copyBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool,
	VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize)
{
	//Create Buffer
	VkCommandBuffer transferCommandBuffer = beginCommandBuffer(device, transferCommandPool);
	
		//Region of data to copy from and to
		VkBufferCopy bufferCopyRegion = {};
		bufferCopyRegion.srcOffset = 0;
		bufferCopyRegion.dstOffset = 0;
		bufferCopyRegion.size = bufferSize;

		//Command to copy src buffer to dst buffer
		vkCmdCopyBuffer(transferCommandBuffer, srcBuffer, dstBuffer, 1, &bufferCopyRegion);

	//End and submit command buffer
	endAndSubmitCommandBuffer(device, transferCommandPool, transferQueue, transferCommandBuffer);
}

static void copyImageBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool,
	VkBuffer srcBuffer, VkImage image, uint32_t width, uint32_t height)
{
	//Create Buffer
	VkCommandBuffer transferCommandBuffer = beginCommandBuffer(device, transferCommandPool);

	VkBufferImageCopy imageRegion = {};
	imageRegion.bufferOffset = 0;													//Offset into data
	imageRegion.bufferRowLength = 0;												//Row lenght of data to calculate data spacing
	imageRegion.bufferImageHeight = 0;												//Image height to calculate data spacing
	imageRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;			//Which aspect of image to copy
	imageRegion.imageSubresource.mipLevel = 0;										//Mipmap level to copy
	imageRegion.imageSubresource.baseArrayLayer = 0;								//Starting array layer (if array)
	imageRegion.imageSubresource.layerCount = 1;									//Number of layers to copy starting at baseArrayLayer
	imageRegion.imageOffset = {0,0,0};									//Offset into image (as opposed to row data in buffer offset)
	imageRegion.imageExtent = {width,height,1};							//Size of region to copy as (x,y,z) values

	//Copy buffer to given image
	vkCmdCopyBufferToImage(transferCommandBuffer, srcBuffer, image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageRegion);
	
	//End and submit command buffer
	endAndSubmitCommandBuffer(device, transferCommandPool, transferQueue, transferCommandBuffer);
}

static void transitionImageLayout(VkDevice device, VkQueue queue, VkCommandPool commandPool,
	VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	//Create Buffer
	VkCommandBuffer commandBuffer = beginCommandBuffer(device, commandPool);

	VkImageMemoryBarrier imageMemoryBarrier = {};
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.oldLayout = oldLayout;											//Layout to transition from
	imageMemoryBarrier.newLayout = newLayout;											//Layout to transition to
	imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;					//Queue family to transition from
	imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;					//Queue family to transition to
	imageMemoryBarrier.image = image;													//Image being accessed and modified as part of barrier
	imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;			//Aspect of image being altered
	imageMemoryBarrier.subresourceRange.baseMipLevel = 0;								//First mip level to start alteration on
	imageMemoryBarrier.subresourceRange.levelCount = 1;									//Number of mip levels to alter starting from base
	imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;								//First layer to start alteration on
	imageMemoryBarrier.subresourceRange.layerCount = 1;									//Number of layer to alter starting from base

	VkPipelineStageFlags srcStage;
	VkPipelineStageFlags dstStage;
	
	//If transitioning from new image to image ready to receive data...
	if(oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		imageMemoryBarrier.srcAccessMask = 0;									//Memory access stage must happen after...
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;		//Memory access stage must happen before...

		srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	//If transitioning from transfer destination to image shader readable...
	else if(oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;		//Memory access stage must happen after...
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;			//Memory access stage must happen before...

		srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}

	vkCmdPipelineBarrier(
		commandBuffer,
		srcStage, dstStage,										//Pipeline stages (match to src and dst accessMask)
		0,														//Dependency flags
		0, nullptr,							//Memory Barrier count and data
		0, nullptr,						//Buffer memory barrier count and data	
		1, &imageMemoryBarrier			//Image memory barrier count and data
	);
	
	//End and submit command buffer
	endAndSubmitCommandBuffer(device, commandPool, queue, commandBuffer);
}