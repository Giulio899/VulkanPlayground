#include "Mesh.h"

Mesh::Mesh()
{
}

Mesh::Mesh(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice,
        VkQueue transferQueue, VkCommandPool transferCommandPool,
        std::vector<Vertex>* vertices, std::vector<uint32_t>* indices,
        int newTexId)
{
    indexCount = indices->size();
    vertexCount = vertices->size();
    physicalDevice = newPhysicalDevice;
    device = newDevice;
    createVertexBuffer(transferQueue, transferCommandPool, vertices);
    createIndexBuffer(transferQueue, transferCommandPool, indices);

    model.model = glm::mat4(1.0f);
    texId = newTexId;
}

void Mesh::setModel(glm::mat4 newModel)
{
    model.model = newModel;
}

Model Mesh::getModel()
{
    return model;
}

Model* Mesh::getModelPointer()
{
    return &model;
}

int Mesh::getTexId()
{
    return texId;
}

int Mesh::getVertexCount()
{
    return vertexCount;
}

VkBuffer Mesh::getVertexBuffer()
{
    return vertexBuffer;
}

int Mesh::getIndexCount()
{
    return indexCount;
}

VkBuffer Mesh::getIndexBuffer()
{
    return indexBuffer;
}

void Mesh::destroyBuffers()
{
    vkDestroyBuffer(device, vertexBuffer, /*Memory management TODO*/nullptr);
    vkFreeMemory(device, vertexBufferMemory, /*Memory management TODO*/nullptr);

    vkDestroyBuffer(device, indexBuffer, /*Memory management TODO*/nullptr);
    vkFreeMemory(device, indexBufferMemory, /*Memory management TODO*/nullptr);
}

Mesh::~Mesh()
{
}

void Mesh::createVertexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<Vertex>* vertices)
{
    //Get size of buffer needed for vertices
    VkDeviceSize bufferSize = sizeof(Vertex) * vertices->size();

    //Temporary buffer to stage vertex data before transferring to GPU
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    //Create staging buffer and allocate memory to it
    createBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &stagingBuffer, &stagingBufferMemory);

    //MAP MEMORY TO VERTEX BUFFER
    void* data;                                                                       //1.Create pointer to a point in normal memory
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);       //2."Map" the vertex buffer memory to that point
    memcpy(data, vertices->data(), (size_t)bufferSize);                           //3.Copy memory from vertices vector to the point
    vkUnmapMemory(device,stagingBufferMemory);                                        //4.Unmap the vertex buffer memory

    //Create buffer with TRANSFER_DST_BIT to mark as a recipient for transfer data (also VERTEX_BUFFER)
    //Buffer memory is to be DEVICE_LOCAL_BIT meaning memory is on the GPU and not accessible by CPU
    createBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &vertexBuffer, &vertexBufferMemory);

    //Copy staging buffer to vertex buffer on GPU
    copyBuffer(device, transferQueue, transferCommandPool, stagingBuffer, vertexBuffer, bufferSize);

    //Clean up staging buffer parts
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void Mesh::createIndexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<uint32_t>* indices)
{
    //Get size of buffer needed for indices
    VkDeviceSize bufferSize = sizeof(uint32_t)* indices->size();

    //Temporary buffer to stage index data before transferring to GPU
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    
    //Create staging buffer and allocate memory to it
    createBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &stagingBuffer, &stagingBufferMemory);

    //MAP MEMORY TO VERTEX BUFFER
    void* data;                                                                       
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);      
    memcpy(data, indices->data(), (size_t)bufferSize);                           
    vkUnmapMemory(device,stagingBufferMemory);                                        

    //Create buffer for index data on GPU access only area
    createBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &indexBuffer, &indexBufferMemory);

    //Copy staging buffer to index buffer on GPU
    copyBuffer(device, transferQueue, transferCommandPool, stagingBuffer, indexBuffer, bufferSize);

    //Clean up staging buffer parts
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}               