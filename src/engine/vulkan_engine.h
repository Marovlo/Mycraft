#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <VkBootstrap.h>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>

#include <vector>
#include <string>
#include <functional>
#include <deque>
#include <array>
#include <optional>
#include <unordered_map>

// --- Vertex definition ---
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription getBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();
};

// --- Uniform buffer ---
struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::vec4 fogColor;     // rgb = color, a = unused
    alignas(16) glm::vec4 viewPos;      // xyz = camera position, w = unused
    alignas(8)  glm::vec2 fogRange;     // x = fogStart, y = fogEnd
    alignas(8)  glm::vec2 padding;
};

// --- Allocated buffer helper ---
struct AllocatedBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
};

// --- Allocated image helper ---
struct AllocatedImage {
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
};

// --- Mesh data ---
struct Mesh {
    AllocatedBuffer vertexBuffer;
    AllocatedBuffer indexBuffer;
    uint32_t indexCount = 0;
};

// --- Deletion queue ---
struct DeletionQueue {
    std::deque<std::function<void()>> deletors;

    void push(std::function<void()>&& fn) {
        deletors.push_back(std::move(fn));
    }

    void flush() {
        for (auto it = deletors.rbegin(); it != deletors.rend(); ++it) {
            (*it)();
        }
        deletors.clear();
    }
};

// --- Frame data ---
struct FrameData {
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence renderFence = VK_NULL_HANDLE;

    AllocatedBuffer uniformBuffer;
    void* uniformBufferMapped = nullptr;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
};

// --- UI Vertex (2D overlay) ---
struct UIVertex {
    glm::vec2 position;   // screen-space pixels
    glm::vec2 texCoord;
    glm::vec4 color;

    static VkVertexInputBindingDescription getBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();
};

// --- UI Push Constants ---
struct UIPushConstants {
    glm::vec2 screenSize;
};

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

// --- Vulkan Engine ---
class VulkanEngine {
public:
    bool init(int width, int height, const char* title);
    void cleanup();
    void run();

    // Getters
    GLFWwindow* getWindow() const { return window_; }
    VkDevice getDevice() const { return device_; }
    VmaAllocator getAllocator() const { return allocator_; }
    uint32_t getWindowWidth() const { return windowWidth_; }
    uint32_t getWindowHeight() const { return windowHeight_; }

    // Mesh management
    Mesh uploadMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    void destroyMesh(Mesh& mesh);

    // UI Mesh (different vertex format)
    Mesh uploadUIMesh(const std::vector<UIVertex>& vertices, const std::vector<uint32_t>& indices);

    // Dynamic buffer (CPU writable, GPU readable — no staging needed)
    // For UI: create once, update every frame via mapped pointer
    AllocatedBuffer createDynamicBuffer(VkDeviceSize size, VkBufferUsageFlags usage);
    void* mapBuffer(AllocatedBuffer& buffer);
    void unmapBuffer(AllocatedBuffer& buffer);

    // Texture
    AllocatedImage uploadTexture(const uint8_t* pixels, int width, int height, int channels);
    void destroyTexture(AllocatedImage& image);

    // Callbacks
    std::function<void(float deltaTime)> onUpdate;
    std::function<void(VkCommandBuffer cmd, uint32_t frameIndex)> onRender;
    std::function<void(GLFWwindow* window, int key, int scancode, int action, int mods)> onKey;
    std::function<void(GLFWwindow* window, double xpos, double ypos)> onMouse;
    std::function<void(GLFWwindow* window, int button, int action, int mods)> onMouseButton;

    // Descriptor set for rendering
    VkPipelineLayout getPipelineLayout() const { return pipelineLayout_; }
    VkPipeline getPipeline() const { return graphicsPipeline_; }
    FrameData& getCurrentFrame() { return frames_[currentFrame_]; }

    // Update UBO
    void updateUniformBuffer(const UniformBufferObject& ubo);

    // Update texture descriptor
    void updateTextureDescriptor(VkImageView imageView, VkSampler sampler);
    VkSampler getDefaultSampler() const { return defaultSampler_; }

    // --- 2D UI Rendering ---
    // Call between beginUI() and endUI() to draw 2D elements.
    // These are drawn on top of 3D scene (no depth test, alpha blend).
    VkPipeline getUIPipeline() const { return uiPipeline_; }
    VkPipelineLayout getUIPipelineLayout() const { return uiPipelineLayout_; }

private:
    // Init steps
    void initVulkan();
    void createSwapchain();
    void createRenderPass();
    void createFramebuffers();
    void createCommandPools();
    void createSyncObjects();
    void createDescriptorSetLayout();
    void createGraphicsPipeline();
    void createUIPipeline();
    void createDepthResources();
    void createDescriptorPool();
    void createDescriptorSets();
    void createUniformBuffers();
    void createDefaultSampler();
    void createDefaultTexture();

    // Helpers
    VkShaderModule createShaderModule(const std::string& filepath);
    AllocatedBuffer createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
    void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer cmd);
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void recreateSwapchain();

    // Draw
    void drawFrame();

    // Window
    GLFWwindow* window_ = nullptr;
    uint32_t windowWidth_ = 0;
    uint32_t windowHeight_ = 0;
    bool framebufferResized_ = false;

    // Vulkan core
    VkInstance instance_ = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamily_ = 0;
    VmaAllocator allocator_ = VK_NULL_HANDLE;

    // Swapchain
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainFormat_;
    VkExtent2D swapchainExtent_;
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;

    // Render pass & framebuffers
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers_;

    // Depth
    AllocatedImage depthImage_;
    VkFormat depthFormat_;

    // Pipeline (3D world)
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline_ = VK_NULL_HANDLE;

    // Pipeline (2D UI overlay)
    VkPipelineLayout uiPipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline uiPipeline_ = VK_NULL_HANDLE;

    // Descriptors
    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;

    // Frames
    FrameData frames_[MAX_FRAMES_IN_FLIGHT];
    uint32_t currentFrame_ = 0;

    // Default resources
    VkSampler defaultSampler_ = VK_NULL_HANDLE;
    AllocatedImage defaultTexture_;

    // Cleanup
    DeletionQueue mainDeletionQueue_;
};
