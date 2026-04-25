#include "vulkan_engine.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <chrono>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <stb_image_write.h>

// ========== Vertex ==========

VkVertexInputBindingDescription Vertex::getBindingDescription() {
    VkVertexInputBindingDescription desc{};
    desc.binding = 0;
    desc.stride = sizeof(Vertex);
    desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return desc;
}

std::array<VkVertexInputAttributeDescription, 4> Vertex::getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 4> attrs{};
    attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)};
    attrs[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)};
    attrs[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, texCoord)};
    attrs[3] = {3, 0, VK_FORMAT_R32_SFLOAT, offsetof(Vertex, light)};
    return attrs;
}

// ========== UIVertex ==========

VkVertexInputBindingDescription UIVertex::getBindingDescription() {
    VkVertexInputBindingDescription desc{};
    desc.binding = 0;
    desc.stride = sizeof(UIVertex);
    desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return desc;
}

std::array<VkVertexInputAttributeDescription, 3> UIVertex::getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 3> attrs{};
    attrs[0] = {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(UIVertex, position)};
    attrs[1] = {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(UIVertex, texCoord)};
    attrs[2] = {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(UIVertex, color)};
    return attrs;
}

// ========== Init ==========

bool VulkanEngine::init(int width, int height, const char* title) {
    windowWidth_ = width;
    windowHeight_ = height;

    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW\n";
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window_) {
        std::cerr << "Failed to create window\n";
        return false;
    }

    // Note: glfwSetWindowUserPointer is reserved for InputManager.
    // We detect framebuffer resize via glfwGetFramebufferSize in drawFrame instead.

    initVulkan();
    createSwapchain();
    createRenderPass();
    createDepthResources();
    createFramebuffers();
    createCommandPools();
    createSyncObjects();
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createUIPipeline();
    createDescriptorPool();
    createUniformBuffers();
    createDescriptorSets();
    createDefaultSampler();
    createDefaultTexture();

    return true;
}

void VulkanEngine::initVulkan() {
    // Get vkGetInstanceProcAddr via GLFW (which links against the correct Vulkan loader)
    auto fp = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        glfwGetInstanceProcAddress(VK_NULL_HANDLE, "vkGetInstanceProcAddr"));

    // Instance
    vkb::InstanceBuilder instBuilder(fp);
    auto instResult = instBuilder
        .set_app_name("Mycraft")
        .request_validation_layers(false)
        .require_api_version(1, 2, 0)
#ifdef __APPLE__
        .enable_extension(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)
        .enable_extension("VK_KHR_get_physical_device_properties2")
#endif
        .build();

    if (!instResult) {
        throw std::runtime_error("Failed to create Vulkan instance: " + instResult.error().message());
    }
    auto vkbInstance = instResult.value();
    instance_ = vkbInstance.instance;
    debugMessenger_ = vkbInstance.debug_messenger;

    // Surface
    if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface");
    }

    // Physical device
    vkb::PhysicalDeviceSelector selector(vkbInstance);
    auto physResult = selector
        .set_minimum_version(1, 2)
        .set_surface(surface_)
        .select();

    if (!physResult) {
        throw std::runtime_error("Failed to select physical device: " + physResult.error().message());
    }
    auto vkbPhysicalDevice = physResult.value();
    physicalDevice_ = vkbPhysicalDevice.physical_device;

    // Logical device
    vkb::DeviceBuilder deviceBuilder(vkbPhysicalDevice);
    auto devResult = deviceBuilder.build();
    if (!devResult) {
        throw std::runtime_error("Failed to create logical device: " + devResult.error().message());
    }
    auto vkbDevice = devResult.value();
    device_ = vkbDevice.device;

    graphicsQueue_ = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    graphicsQueueFamily_ = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
    presentQueue_ = vkbDevice.get_queue(vkb::QueueType::present).value();

    // VMA
    VmaAllocatorCreateInfo allocInfo{};
    allocInfo.physicalDevice = physicalDevice_;
    allocInfo.device = device_;
    allocInfo.instance = instance_;
    allocInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    vmaCreateAllocator(&allocInfo, &allocator_);

    mainDeletionQueue_.push([this]() {
        vmaDestroyAllocator(allocator_);
    });
}

void VulkanEngine::createSwapchain() {
    vkb::SwapchainBuilder swapBuilder(physicalDevice_, device_, surface_);

    int w, h;
    glfwGetFramebufferSize(window_, &w, &h);

    auto swapResult = swapBuilder
        .set_desired_format({VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(w, h)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build();

    if (!swapResult) {
        throw std::runtime_error("Failed to create swapchain: " + swapResult.error().message());
    }
    auto vkbSwapchain = swapResult.value();

    swapchain_ = vkbSwapchain.swapchain;
    swapchainFormat_ = vkbSwapchain.image_format;
    swapchainExtent_ = vkbSwapchain.extent;
    swapchainImages_ = vkbSwapchain.get_images().value();
    swapchainImageViews_ = vkbSwapchain.get_image_views().value();

    windowWidth_ = swapchainExtent_.width;
    windowHeight_ = swapchainExtent_.height;
}

void VulkanEngine::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchainFormat_;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = VK_FORMAT_D32_SFLOAT;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

    VkRenderPassCreateInfo rpInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    rpInfo.pAttachments = attachments.data();
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device_, &rpInfo, nullptr, &renderPass_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass");
    }

    mainDeletionQueue_.push([this]() {
        vkDestroyRenderPass(device_, renderPass_, nullptr);
    });
}

void VulkanEngine::createDepthResources() {
    depthFormat_ = VK_FORMAT_D32_SFLOAT;

    VkImageCreateInfo imgInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.extent = {swapchainExtent_.width, swapchainExtent_.height, 1};
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.format = depthFormat_;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imgInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    vmaCreateImage(allocator_, &imgInfo, &allocInfo, &depthImage_.image, &depthImage_.allocation, nullptr);

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = depthImage_.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = depthFormat_;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    vkCreateImageView(device_, &viewInfo, nullptr, &depthImage_.imageView);
}

void VulkanEngine::createFramebuffers() {
    framebuffers_.resize(swapchainImageViews_.size());
    for (size_t i = 0; i < swapchainImageViews_.size(); i++) {
        std::array<VkImageView, 2> attachments = {swapchainImageViews_[i], depthImage_.imageView};

        VkFramebufferCreateInfo fbInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fbInfo.renderPass = renderPass_;
        fbInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        fbInfo.pAttachments = attachments.data();
        fbInfo.width = swapchainExtent_.width;
        fbInfo.height = swapchainExtent_.height;
        fbInfo.layers = 1;

        if (vkCreateFramebuffer(device_, &fbInfo, nullptr, &framebuffers_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer");
        }
    }
}

void VulkanEngine::createCommandPools() {
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = graphicsQueueFamily_;

        vkCreateCommandPool(device_, &poolInfo, nullptr, &frames_[i].commandPool);

        VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocInfo.commandPool = frames_[i].commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        vkAllocateCommandBuffers(device_, &allocInfo, &frames_[i].commandBuffer);
    }
}

void VulkanEngine::createSyncObjects() {
    VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkCreateSemaphore(device_, &semInfo, nullptr, &frames_[i].imageAvailableSemaphore);
        vkCreateSemaphore(device_, &semInfo, nullptr, &frames_[i].renderFinishedSemaphore);
        vkCreateFence(device_, &fenceInfo, nullptr, &frames_[i].renderFence);
    }
}

void VulkanEngine::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding = 0;
    uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.binding = 1;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.descriptorCount = 1;
    samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboBinding, samplerBinding};

    VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &descriptorSetLayout_);

    mainDeletionQueue_.push([this]() {
        vkDestroyDescriptorSetLayout(device_, descriptorSetLayout_, nullptr);
    });
}

void VulkanEngine::createGraphicsPipeline() {
    auto vertModule = createShaderModule(std::string(SHADER_DIR) + "/basic.vert.spv");
    auto fragModule = createShaderModule(std::string(SHADER_DIR) + "/basic.frag.spv");

    VkPipelineShaderStageCreateInfo vertStage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName = "main";

    VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

    auto bindingDesc = Vertex::getBindingDescription();
    auto attrDescs = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrDescs.size());
    vertexInput.pVertexAttributeDescriptions = attrDescs.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchainExtent_.width);
    viewport.height = static_cast<float>(swapchainExtent_.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchainExtent_;

    VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.lineWidth = 1.0f;
    raster.cullMode = VK_CULL_MODE_BACK_BIT;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &colorBlendAttachment;

    // Dynamic state for viewport/scissor
    std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout_;

    vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr, &pipelineLayout_);

    VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &raster;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout_;
    pipelineInfo.renderPass = renderPass_;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline");
    }

    vkDestroyShaderModule(device_, vertModule, nullptr);
    vkDestroyShaderModule(device_, fragModule, nullptr);

    mainDeletionQueue_.push([this]() {
        vkDestroyPipeline(device_, graphicsPipeline_, nullptr);
        vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
    });

    // --- Transparent pipeline (same shaders, same layout, but alpha blend + no depth write) ---
    {
        // Re-create shader modules (the previous ones were destroyed)
        auto vertModule2 = createShaderModule(std::string(SHADER_DIR) + "/basic.vert.spv");
        auto fragModule2 = createShaderModule(std::string(SHADER_DIR) + "/basic.frag.spv");

        VkPipelineShaderStageCreateInfo vertStage2{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        vertStage2.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertStage2.module = vertModule2;
        vertStage2.pName = "main";

        VkPipelineShaderStageCreateInfo fragStage2{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        fragStage2.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStage2.module = fragModule2;
        fragStage2.pName = "main";

        VkPipelineShaderStageCreateInfo stages2[] = {vertStage2, fragStage2};

        // Depth: test ON, write OFF (transparent objects must not occlude each other)
        VkPipelineDepthStencilStateCreateInfo transDepth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        transDepth.depthTestEnable = VK_TRUE;
        transDepth.depthWriteEnable = VK_FALSE;
        transDepth.depthCompareOp = VK_COMPARE_OP_LESS;

        // Alpha blending: src*srcAlpha + dst*(1-srcAlpha)
        VkPipelineColorBlendAttachmentState transBlend{};
        transBlend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        transBlend.blendEnable = VK_TRUE;
        transBlend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        transBlend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        transBlend.colorBlendOp = VK_BLEND_OP_ADD;
        transBlend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        transBlend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        transBlend.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo transColorBlend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        transColorBlend.attachmentCount = 1;
        transColorBlend.pAttachments = &transBlend;

        // No backface culling for transparent geometry (water visible from both sides)
        VkPipelineRasterizationStateCreateInfo transRaster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        transRaster.polygonMode = VK_POLYGON_MODE_FILL;
        transRaster.lineWidth = 1.0f;
        transRaster.cullMode = VK_CULL_MODE_NONE;
        transRaster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        VkGraphicsPipelineCreateInfo transPipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        transPipelineInfo.stageCount = 2;
        transPipelineInfo.pStages = stages2;
        transPipelineInfo.pVertexInputState = &vertexInput;
        transPipelineInfo.pInputAssemblyState = &inputAssembly;
        transPipelineInfo.pViewportState = &viewportState;
        transPipelineInfo.pRasterizationState = &transRaster;
        transPipelineInfo.pMultisampleState = &multisampling;
        transPipelineInfo.pDepthStencilState = &transDepth;
        transPipelineInfo.pColorBlendState = &transColorBlend;
        transPipelineInfo.pDynamicState = &dynamicState;
        transPipelineInfo.layout = pipelineLayout_;
        transPipelineInfo.renderPass = renderPass_;
        transPipelineInfo.subpass = 0;

        if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &transPipelineInfo, nullptr, &transparentPipeline_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create transparent pipeline");
        }

        vkDestroyShaderModule(device_, vertModule2, nullptr);
        vkDestroyShaderModule(device_, fragModule2, nullptr);

        mainDeletionQueue_.push([this]() {
            vkDestroyPipeline(device_, transparentPipeline_, nullptr);
        });
    }
}

void VulkanEngine::createUIPipeline() {
    auto vertModule = createShaderModule(std::string(SHADER_DIR) + "/ui.vert.spv");
    auto fragModule = createShaderModule(std::string(SHADER_DIR) + "/ui.frag.spv");

    VkPipelineShaderStageCreateInfo vertStage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName = "main";

    VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

    auto bindingDesc = UIVertex::getBindingDescription();
    auto attrDescs = UIVertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrDescs.size());
    vertexInput.pVertexAttributeDescriptions = attrDescs.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.lineWidth = 1.0f;
    raster.cullMode = VK_CULL_MODE_NONE;  // UI quads are double-sided
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // No depth test for UI
    VkPipelineDepthStencilStateCreateInfo depthStencil{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;

    // Alpha blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &colorBlendAttachment;

    std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // Push constant for screen size
    VkPushConstantRange pushConstRange{};
    pushConstRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstRange.offset = 0;
    pushConstRange.size = sizeof(UIPushConstants);

    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &descriptorSetLayout_;  // reuse same descriptor layout (UBO + sampler)
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstRange;

    vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &uiPipelineLayout_);

    VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &raster;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = uiPipelineLayout_;
    pipelineInfo.renderPass = renderPass_;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &uiPipeline_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create UI pipeline");
    }

    vkDestroyShaderModule(device_, vertModule, nullptr);
    vkDestroyShaderModule(device_, fragModule, nullptr);

    mainDeletionQueue_.push([this]() {
        vkDestroyPipeline(device_, uiPipeline_, nullptr);
        vkDestroyPipelineLayout(device_, uiPipelineLayout_, nullptr);
    });
}

void VulkanEngine::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT};
    poolSizes[1] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT};

    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

    vkCreateDescriptorPool(device_, &poolInfo, nullptr, &descriptorPool_);

    mainDeletionQueue_.push([this]() {
        vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
    });
}

void VulkanEngine::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        frames_[i].uniformBuffer = createBuffer(bufferSize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU);

        vmaMapMemory(allocator_, frames_[i].uniformBuffer.allocation, &frames_[i].uniformBufferMapped);
    }
}

void VulkanEngine::createDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout_);

    VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = descriptorPool_;
    allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    allocInfo.pSetLayouts = layouts.data();

    std::vector<VkDescriptorSet> sets(MAX_FRAMES_IN_FLIGHT);
    vkAllocateDescriptorSets(device_, &allocInfo, sets.data());

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        frames_[i].descriptorSet = sets[i];

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = frames_[i].uniformBuffer.buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkWriteDescriptorSet uboWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        uboWrite.dstSet = frames_[i].descriptorSet;
        uboWrite.dstBinding = 0;
        uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboWrite.descriptorCount = 1;
        uboWrite.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(device_, 1, &uboWrite, 0, nullptr);
    }
}

void VulkanEngine::createDefaultSampler() {
    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    vkCreateSampler(device_, &samplerInfo, nullptr, &defaultSampler_);

    mainDeletionQueue_.push([this]() {
        vkDestroySampler(device_, defaultSampler_, nullptr);
    });
}

void VulkanEngine::createDefaultTexture() {
    // 1x1 white pixel
    uint8_t pixel[] = {255, 255, 255, 255};
    defaultTexture_ = uploadTexture(pixel, 1, 1, 4);

    // Bind default texture to all descriptor sets
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.sampler = defaultSampler_;
        imageInfo.imageView = defaultTexture_.imageView;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = frames_[i].descriptorSet;
        write.dstBinding = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
    }

    mainDeletionQueue_.push([this]() {
        destroyTexture(defaultTexture_);
    });
}

// ========== Shader Module ==========

VkShaderModule VulkanEngine::createShaderModule(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + filepath);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> code(fileSize);
    file.seekg(0);
    file.read(code.data(), fileSize);

    VkShaderModuleCreateInfo createInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule module;
    if (vkCreateShaderModule(device_, &createInfo, nullptr, &module) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module");
    }
    return module;
}

// ========== Buffer helpers ==========

AllocatedBuffer VulkanEngine::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) {
    VkBufferCreateInfo bufInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufInfo.size = size;
    bufInfo.usage = usage;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = memoryUsage;

    AllocatedBuffer buffer;
    vmaCreateBuffer(allocator_, &bufInfo, &allocInfo, &buffer.buffer, &buffer.allocation, nullptr);
    return buffer;
}

void VulkanEngine::copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    auto cmd = beginSingleTimeCommands();
    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(cmd, src, dst, 1, &copyRegion);
    endSingleTimeCommands(cmd);
}

VkCommandBuffer VulkanEngine::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = frames_[0].commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device_, &allocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    return cmd;
}

void VulkanEngine::endSingleTimeCommands(VkCommandBuffer cmd) {
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    vkQueueSubmit(graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue_);

    vkFreeCommandBuffers(device_, frames_[0].commandPool, 1, &cmd);
}

void VulkanEngine::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
    auto cmd = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags srcStage, dstStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::runtime_error("Unsupported layout transition");
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    endSingleTimeCommands(cmd);
}

// ========== Mesh ==========

Mesh VulkanEngine::uploadMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
    Mesh mesh;
    mesh.indexCount = static_cast<uint32_t>(indices.size());

    VkDeviceSize vbSize = sizeof(Vertex) * vertices.size();
    VkDeviceSize ibSize = sizeof(uint32_t) * indices.size();
    VkDeviceSize totalSize = vbSize + ibSize;

    // Single staging buffer for both vertex and index data
    auto staging = createBuffer(totalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void* data;
    vmaMapMemory(allocator_, staging.allocation, &data);
    memcpy(data, vertices.data(), vbSize);
    memcpy(static_cast<char*>(data) + vbSize, indices.data(), ibSize);
    vmaUnmapMemory(allocator_, staging.allocation);

    mesh.vertexBuffer = createBuffer(vbSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);
    mesh.indexBuffer = createBuffer(ibSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    // Single command buffer for both copies
    auto cmd = beginSingleTimeCommands();
    VkBufferCopy vbCopy{};
    vbCopy.srcOffset = 0;
    vbCopy.size = vbSize;
    vkCmdCopyBuffer(cmd, staging.buffer, mesh.vertexBuffer.buffer, 1, &vbCopy);

    VkBufferCopy ibCopy{};
    ibCopy.srcOffset = vbSize;
    ibCopy.size = ibSize;
    vkCmdCopyBuffer(cmd, staging.buffer, mesh.indexBuffer.buffer, 1, &ibCopy);
    endSingleTimeCommands(cmd);

    vmaDestroyBuffer(allocator_, staging.buffer, staging.allocation);

    return mesh;
}

void VulkanEngine::destroyMesh(Mesh& mesh) {
    if (mesh.vertexBuffer.allocation) {
        vmaDestroyBuffer(allocator_, mesh.vertexBuffer.buffer, mesh.vertexBuffer.allocation);
        mesh.vertexBuffer = {};
    }
    if (mesh.indexBuffer.allocation) {
        vmaDestroyBuffer(allocator_, mesh.indexBuffer.buffer, mesh.indexBuffer.allocation);
        mesh.indexBuffer = {};
    }
    mesh.indexCount = 0;
}

Mesh VulkanEngine::uploadUIMesh(const std::vector<UIVertex>& vertices, const std::vector<uint32_t>& indices) {
    Mesh mesh;
    mesh.indexCount = static_cast<uint32_t>(indices.size());

    VkDeviceSize vbSize = sizeof(UIVertex) * vertices.size();
    VkDeviceSize ibSize = sizeof(uint32_t) * indices.size();
    VkDeviceSize totalSize = vbSize + ibSize;

    auto staging = createBuffer(totalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void* data;
    vmaMapMemory(allocator_, staging.allocation, &data);
    memcpy(data, vertices.data(), vbSize);
    memcpy(static_cast<char*>(data) + vbSize, indices.data(), ibSize);
    vmaUnmapMemory(allocator_, staging.allocation);

    mesh.vertexBuffer = createBuffer(vbSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);
    mesh.indexBuffer = createBuffer(ibSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    auto cmd = beginSingleTimeCommands();
    VkBufferCopy vbCopy{};
    vbCopy.srcOffset = 0;
    vbCopy.size = vbSize;
    vkCmdCopyBuffer(cmd, staging.buffer, mesh.vertexBuffer.buffer, 1, &vbCopy);

    VkBufferCopy ibCopy{};
    ibCopy.srcOffset = vbSize;
    ibCopy.size = ibSize;
    vkCmdCopyBuffer(cmd, staging.buffer, mesh.indexBuffer.buffer, 1, &ibCopy);
    endSingleTimeCommands(cmd);

    vmaDestroyBuffer(allocator_, staging.buffer, staging.allocation);

    return mesh;
}

AllocatedBuffer VulkanEngine::createDynamicBuffer(VkDeviceSize size, VkBufferUsageFlags usage) {
    VkBufferCreateInfo bufInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufInfo.size = size;
    bufInfo.usage = usage;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    AllocatedBuffer buffer;
    vmaCreateBuffer(allocator_, &bufInfo, &allocInfo, &buffer.buffer, &buffer.allocation, nullptr);
    return buffer;
}

void* VulkanEngine::mapBuffer(AllocatedBuffer& buffer) {
    void* data;
    vmaMapMemory(allocator_, buffer.allocation, &data);
    return data;
}

void VulkanEngine::unmapBuffer(AllocatedBuffer& buffer) {
    vmaUnmapMemory(allocator_, buffer.allocation);
}

// ========== Texture ==========

AllocatedImage VulkanEngine::uploadTexture(const uint8_t* pixels, int width, int height, int channels) {
    AllocatedImage image;
    VkDeviceSize imageSize = width * height * 4;

    auto staging = createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    void* data;
    vmaMapMemory(allocator_, staging.allocation, &data);

    if (channels == 4) {
        memcpy(data, pixels, imageSize);
    } else {
        // Convert to RGBA
        uint8_t* dst = static_cast<uint8_t*>(data);
        for (int i = 0; i < width * height; i++) {
            dst[i * 4 + 0] = pixels[i * channels + 0];
            dst[i * 4 + 1] = channels > 1 ? pixels[i * channels + 1] : pixels[i * channels];
            dst[i * 4 + 2] = channels > 2 ? pixels[i * channels + 2] : pixels[i * channels];
            dst[i * 4 + 3] = 255;
        }
    }
    vmaUnmapMemory(allocator_, staging.allocation);

    VkImageCreateInfo imgInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    vmaCreateImage(allocator_, &imgInfo, &allocCreateInfo, &image.image, &image.allocation, nullptr);

    transitionImageLayout(image.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    auto cmd = beginSingleTimeCommands();
    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
    vkCmdCopyBufferToImage(cmd, staging.buffer, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    endSingleTimeCommands(cmd);

    transitionImageLayout(image.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vmaDestroyBuffer(allocator_, staging.buffer, staging.allocation);

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = image.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    vkCreateImageView(device_, &viewInfo, nullptr, &image.imageView);

    return image;
}

void VulkanEngine::destroyTexture(AllocatedImage& image) {
    if (image.imageView) {
        vkDestroyImageView(device_, image.imageView, nullptr);
    }
    if (image.allocation) {
        vmaDestroyImage(allocator_, image.image, image.allocation);
    }
    image = {};
}

void VulkanEngine::updateTextureDescriptor(VkImageView imageView, VkSampler sampler) {
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.sampler = sampler;
        imageInfo.imageView = imageView;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = frames_[i].descriptorSet;
        write.dstBinding = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
    }
}

void VulkanEngine::updateUniformBuffer(const UniformBufferObject& ubo) {
    memcpy(frames_[currentFrame_].uniformBufferMapped, &ubo, sizeof(ubo));
}

// ========== Swapchain recreation ==========

void VulkanEngine::recreateSwapchain() {
    int w = 0, h = 0;
    glfwGetFramebufferSize(window_, &w, &h);
    while (w == 0 || h == 0) {
        glfwGetFramebufferSize(window_, &w, &h);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(device_);

    // Cleanup old
    for (auto fb : framebuffers_) {
        vkDestroyFramebuffer(device_, fb, nullptr);
    }
    for (auto iv : swapchainImageViews_) {
        vkDestroyImageView(device_, iv, nullptr);
    }
    vkDestroyImageView(device_, depthImage_.imageView, nullptr);
    vmaDestroyImage(allocator_, depthImage_.image, depthImage_.allocation);
    vkDestroySwapchainKHR(device_, swapchain_, nullptr);

    createSwapchain();
    createDepthResources();
    createFramebuffers();
}

// ========== Draw frame ==========

void VulkanEngine::drawFrame() {
    // Detect framebuffer resize by polling
    int fbW, fbH;
    glfwGetFramebufferSize(window_, &fbW, &fbH);
    if (fbW != static_cast<int>(swapchainExtent_.width) ||
        fbH != static_cast<int>(swapchainExtent_.height)) {
        framebufferResized_ = true;
    }

    auto& frame = frames_[currentFrame_];

    vkWaitForFences(device_, 1, &frame.renderFence, VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
        frame.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    }

    vkResetFences(device_, 1, &frame.renderFence);
    vkResetCommandBuffer(frame.commandBuffer, 0);

    // Record command buffer
    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(frame.commandBuffer, &beginInfo);

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{clearColor_.r, clearColor_.g, clearColor_.b, clearColor_.a}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rpBegin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpBegin.renderPass = renderPass_;
    rpBegin.framebuffer = framebuffers_[imageIndex];
    rpBegin.renderArea.offset = {0, 0};
    rpBegin.renderArea.extent = swapchainExtent_;
    rpBegin.clearValueCount = static_cast<uint32_t>(clearValues.size());
    rpBegin.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(frame.commandBuffer, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(frame.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline_);

    // Set dynamic viewport and scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchainExtent_.width);
    viewport.height = static_cast<float>(swapchainExtent_.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(frame.commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchainExtent_;
    vkCmdSetScissor(frame.commandBuffer, 0, 1, &scissor);

    vkCmdBindDescriptorSets(frame.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout_, 0, 1, &frame.descriptorSet, 0, nullptr);

    // Call user render callback
    if (onRender) {
        onRender(frame.commandBuffer, currentFrame_);
    }

    vkCmdEndRenderPass(frame.commandBuffer);
    vkEndCommandBuffer(frame.commandBuffer);

    // Submit
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &frame.imageAvailableSemaphore;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frame.commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &frame.renderFinishedSemaphore;

    vkQueueSubmit(graphicsQueue_, 1, &submitInfo, frame.renderFence);

    VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &frame.renderFinishedSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain_;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(presentQueue_, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized_) {
        framebufferResized_ = false;
        recreateSwapchain();
    }

    // Screenshot if requested (after present, image is in PRESENT_SRC layout)
    if (!pendingScreenshotPath_.empty()) {
        executeScreenshot(imageIndex);
    }

    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanEngine::requestScreenshot(const std::string& filepath) {
    pendingScreenshotPath_ = filepath;
}

void VulkanEngine::executeScreenshot(uint32_t imageIndex) {
    if (pendingScreenshotPath_.empty()) return;

    vkDeviceWaitIdle(device_);

    VkDeviceSize imageSize = swapchainExtent_.width * swapchainExtent_.height * 4;
    auto stagingBuf = createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    auto cmd = beginSingleTimeCommands();

    // Transition swapchain image to transfer src
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = swapchainImages_[imageIndex];
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Copy image to buffer
    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {swapchainExtent_.width, swapchainExtent_.height, 1};
    vkCmdCopyImageToBuffer(cmd, swapchainImages_[imageIndex],
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            stagingBuf.buffer, 1, &region);

    // Transition back
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    endSingleTimeCommands(cmd);

    // Read pixels and write PNG
    void* data;
    vmaMapMemory(allocator_, stagingBuf.allocation, &data);

    // Swapchain format is B8G8R8A8_SRGB — need to swizzle to RGBA for PNG
    uint32_t w = swapchainExtent_.width;
    uint32_t h = swapchainExtent_.height;
    std::vector<uint8_t> rgba(w * h * 4);
    auto* src = static_cast<uint8_t*>(data);
    for (uint32_t i = 0; i < w * h; i++) {
        rgba[i*4+0] = src[i*4+2];  // R <- B
        rgba[i*4+1] = src[i*4+1];  // G
        rgba[i*4+2] = src[i*4+0];  // B <- R
        rgba[i*4+3] = 255;
    }

    vmaUnmapMemory(allocator_, stagingBuf.allocation);
    vmaDestroyBuffer(allocator_, stagingBuf.buffer, stagingBuf.allocation);

    // Write PNG using stb_image_write
    stbi_write_png(pendingScreenshotPath_.c_str(), w, h, 4, rgba.data(), w * 4);

    std::cout << "Screenshot saved: " << pendingScreenshotPath_ << " (" << w << "x" << h << ")\n";
    pendingScreenshotPath_.clear();
}

// ========== Run ==========

void VulkanEngine::run() {
    auto lastTime = std::chrono::high_resolution_clock::now();

    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        if (onUpdate) {
            onUpdate(dt);
        }

        drawFrame();
    }

    vkDeviceWaitIdle(device_);
}

// ========== Cleanup ==========

void VulkanEngine::cleanup() {
    vkDeviceWaitIdle(device_);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (frames_[i].uniformBuffer.allocation) {
            vmaUnmapMemory(allocator_, frames_[i].uniformBuffer.allocation);
            vmaDestroyBuffer(allocator_, frames_[i].uniformBuffer.buffer, frames_[i].uniformBuffer.allocation);
            frames_[i].uniformBuffer = {};
        }

        vkDestroySemaphore(device_, frames_[i].imageAvailableSemaphore, nullptr);
        vkDestroySemaphore(device_, frames_[i].renderFinishedSemaphore, nullptr);
        vkDestroyFence(device_, frames_[i].renderFence, nullptr);
        vkDestroyCommandPool(device_, frames_[i].commandPool, nullptr);
    }

    for (auto fb : framebuffers_) {
        vkDestroyFramebuffer(device_, fb, nullptr);
    }
    for (auto iv : swapchainImageViews_) {
        vkDestroyImageView(device_, iv, nullptr);
    }

    vkDestroyImageView(device_, depthImage_.imageView, nullptr);
    vmaDestroyImage(allocator_, depthImage_.image, depthImage_.allocation);

    vkDestroySwapchainKHR(device_, swapchain_, nullptr);

    mainDeletionQueue_.flush();

    vkDestroySurfaceKHR(instance_, surface_, nullptr);
    vkDestroyDevice(device_, nullptr);

    vkb::destroy_debug_utils_messenger(instance_, debugMessenger_);
    vkDestroyInstance(instance_, nullptr);

    glfwDestroyWindow(window_);
    glfwTerminate();
}
