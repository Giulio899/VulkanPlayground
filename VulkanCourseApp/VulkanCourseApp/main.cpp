#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <vector>
#include <iostream>

#include "VulkanRenderer.h"

GLFWwindow* window;
VulkanRenderer vulkanRenderer;


void initWindow(std::string wName = "Test Window", const int width = 800, const int height = 600)
{
	//Initialize GLFW
	glfwInit();

	//Set GLFW to NOT work with OpenGL
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	//In Vulkan is complicate to handle the resizing of a window
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	window = glfwCreateWindow(width, height, wName.c_str(), nullptr, nullptr);
}

int main() 
{
	//Create Window
	initWindow("Test Window", 800, 600);

	//Create VulkanRenderer instance
	if (vulkanRenderer.init(window) == EXIT_FAILURE)
	{
		return EXIT_FAILURE;
	}

	//Loop until closed
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
		vulkanRenderer.draw();
	}

	vulkanRenderer.cleanup();

	//Destroy GLFW window and stop GLFW
	glfwDestroyWindow(window);
	glfwTerminate();

	return EXIT_SUCCESS;
}