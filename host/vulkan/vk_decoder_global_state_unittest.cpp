// copyright (c) 2022 the android open source project
//
// licensed under the apache license, version 2.0 (the "license");
// you may not use this file except in compliance with the license.
// you may obtain a copy of the license at
//
// http://www.apache.org/licenses/license-2.0
//
// unless required by applicable law or agreed to in writing, software
// distributed under the license is distributed on an "as is" basis,
// without warranties or conditions of any kind, either express or implied.
// see the license for the specific language governing permissions and
// limitations under the license.

#include "vk_decoder_global_state.cpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <thread>

#include "gfxstream/testing/TestUtils.h"

namespace gfxstream {
namespace host {
namespace vk {

// Handles the device-cleanup test needs to inspect after teardown.
struct CommandBufferCleanupProbe {
    Boxed<VkCommandBuffer> boxedCmdBuf;
    std::shared_ptr<OrderMaintenanceInfo> cmdBufOrder;
};

struct VkDecoderGlobalStateTestPeer {
    VkDecoderGlobalState::Impl impl;

    void registerDescriptorPool(VkDescriptorPool pool, std::vector<uint64_t> poolIds = {}) {
        std::lock_guard<std::mutex> lock(impl.mMutex);
        DescriptorPoolInfo info;
        info.preallocatedBoxedSets = std::move(poolIds);
        impl.mDescriptorPoolInfo[pool] = std::move(info);
    }

    void collectDescriptorPoolIds(gfxstream::base::BumpPool* pool, VkDevice device,
                                  VkDescriptorPool descriptorPool, uint32_t* pPoolIdCount,
                                  uint64_t* pPoolIds) {
        impl.on_vkCollectDescriptorPoolIdsGOOGLE(pool, kInvalidSnapshotApiCallHandle,
                                                 Boxed<VkDevice>(device), descriptorPool,
                                                 pPoolIdCount, pPoolIds);
    }

    VkDescriptorSet getOrAllocateDescriptorSetFromPoolAndId(
        VulkanDispatch* vk, VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout setLayout,
        uint64_t poolId, uint32_t pendingAlloc, bool* didAlloc) {
        std::lock_guard<std::mutex> lock(impl.mMutex);
        return impl.getOrAllocateDescriptorSetFromPoolAndIdLocked(
            vk, device, pool, setLayout, poolId, pendingAlloc, didAlloc);
    }

    void commitDescriptorSetUpdates(
        gfxstream::base::BumpPool* pool, VulkanDispatch* vk, VkDevice device,
        uint32_t descriptorPoolCount, const VkDescriptorPool* pDescriptorPools,
        uint32_t descriptorSetCount, const VkDescriptorSetLayout* pDescriptorSetLayouts,
        const uint64_t* pDescriptorSetPoolIds, const uint32_t* pDescriptorSetWhichPool,
        const uint32_t* pDescriptorSetPendingAllocation,
        const uint32_t* pDescriptorWriteStartingIndices, uint32_t pendingDescriptorWriteCount,
        const VkWriteDescriptorSet* pPendingDescriptorWrites,
        bool needToUnboxDescriptorSet = false) {
        std::lock_guard<std::mutex> lock(impl.mMutex);
        impl.on_vkQueueCommitDescriptorSetUpdatesGOOGLELocked(
            pool, kInvalidSnapshotApiCallHandle, vk, device, descriptorPoolCount, pDescriptorPools,
            descriptorSetCount, pDescriptorSetLayouts, pDescriptorSetPoolIds,
            pDescriptorSetWhichPool, pDescriptorSetPendingAllocation,
            pDescriptorWriteStartingIndices, pendingDescriptorWriteCount, pPendingDescriptorWrites,
            needToUnboxDescriptorSet);
    }

    void destroyCommandPoolWithExclusiveInfo(
        VkDevice device, VulkanDispatch* deviceDispatch, VkCommandPool commandPool,
        CommandPoolInfo& commandPoolInfo,
        std::unordered_map<VkCommandBuffer, CommandBufferInfo>& commandBufferInfos) {
        impl.destroyCommandPoolWithExclusiveInfo(device, deviceDispatch, commandPool,
                                                 commandPoolInfo, commandBufferInfos);
    }

    // Boxes one object of each device-owned type and registers it in the Impl
    // info maps, as if the guest had created it on `unboxedDevice`.
    CommandBufferCleanupProbe registerDeviceObjectsForCleanupTest(VulkanDispatch* globalVk,
                                                                  VulkanDispatch* deviceVk,
                                                                  VkDevice unboxedDevice) {
        impl.m_vk = globalVk;

        Boxed<VkDevice> boxedDevice = new_boxed_VkDevice(unboxedDevice, deviceVk);
        VkCommandPool unboxedCmdPool = reinterpret_cast<VkCommandPool>(0xa001'0000ULL);
        Boxed<VkCommandPool> boxedCmdPool =
            new_boxed_non_dispatchable_VkCommandPool(unboxedCmdPool);
        VkCommandBuffer unboxedCmdBuf = reinterpret_cast<VkCommandBuffer>(0xa002'0000ULL);
        Boxed<VkCommandBuffer> boxedCmdBuf = new_boxed_VkCommandBuffer(unboxedCmdBuf, deviceVk);
        BoxedHandleInfo* cmdBufBoxedInfo = sBoxedHandleManager.get(boxedCmdBuf);
        EXPECT_NE(nullptr, cmdBufBoxedInfo);
        if (cmdBufBoxedInfo) {
            cmdBufBoxedInfo->readStream =
                std::make_unique<VulkanMemReadingStream>(nullptr, gfxstream::host::FeatureSet{});
        }
        CommandBufferCleanupProbe probe;
        probe.boxedCmdBuf = boxedCmdBuf;
        probe.cmdBufOrder = ordmaint_VkCommandBuffer(boxedCmdBuf);

        VkBuffer unboxedBuffer = reinterpret_cast<VkBuffer>(0xa003'0000ULL);
        Boxed<VkBuffer> boxedBuffer = new_boxed_non_dispatchable_VkBuffer(unboxedBuffer);
        VkDeviceMemory unboxedMem = reinterpret_cast<VkDeviceMemory>(0xa004'0000ULL);
        Boxed<VkDeviceMemory> boxedMem = new_boxed_non_dispatchable_VkDeviceMemory(unboxedMem);
        VkDescriptorPool unboxedDescPool = reinterpret_cast<VkDescriptorPool>(0xa005'0000ULL);
        Boxed<VkDescriptorPool> boxedDescPool =
            new_boxed_non_dispatchable_VkDescriptorPool(unboxedDescPool);
        VkDescriptorUpdateTemplate unboxedTemplate =
            reinterpret_cast<VkDescriptorUpdateTemplate>(0xa006'0000ULL);
        Boxed<VkDescriptorUpdateTemplate> boxedTemplate =
            new_boxed_non_dispatchable_VkDescriptorUpdateTemplate(unboxedTemplate);
        VkSamplerYcbcrConversion unboxedYcbcr =
            reinterpret_cast<VkSamplerYcbcrConversion>(0xa007'0000ULL);
        Boxed<VkSamplerYcbcrConversion> boxedYcbcr =
            new_boxed_non_dispatchable_VkSamplerYcbcrConversion(unboxedYcbcr);
        VkShaderModule unboxedShader = reinterpret_cast<VkShaderModule>(0xa008'0000ULL);
        Boxed<VkShaderModule> boxedShader = new_boxed_non_dispatchable_VkShaderModule(unboxedShader);
        VkPipeline unboxedPipeline = reinterpret_cast<VkPipeline>(0xa009'0000ULL);
        Boxed<VkPipeline> boxedPipeline = new_boxed_non_dispatchable_VkPipeline(unboxedPipeline);
        VkPipelineCache unboxedCache = reinterpret_cast<VkPipelineCache>(0xa00a'0000ULL);
        Boxed<VkPipelineCache> boxedCache = new_boxed_non_dispatchable_VkPipelineCache(unboxedCache);
        VkPipelineLayout unboxedLayout = reinterpret_cast<VkPipelineLayout>(0xa00b'0000ULL);
        Boxed<VkPipelineLayout> boxedLayout =
            new_boxed_non_dispatchable_VkPipelineLayout(unboxedLayout);
        VkFramebuffer unboxedFb = reinterpret_cast<VkFramebuffer>(0xa00c'0000ULL);
        Boxed<VkFramebuffer> boxedFb = new_boxed_non_dispatchable_VkFramebuffer(unboxedFb);
        VkRenderPass unboxedRp = reinterpret_cast<VkRenderPass>(0xa00d'0000ULL);
        Boxed<VkRenderPass> boxedRp = new_boxed_non_dispatchable_VkRenderPass(unboxedRp);

        {
            std::lock_guard<std::mutex> lock(impl.mMutex);
            DeviceInfo& devInfo = impl.mDeviceInfo[unboxedDevice];
            devInfo.boxed = boxedDevice;
            devInfo.decompPipelines =
                std::make_unique<GpuDecompressionPipelineManager>(deviceVk, unboxedDevice);
            devInfo.deviceOpTracker = std::make_shared<DeviceOpTracker>(unboxedDevice, deviceVk);
            devInfo.externalFencePool =
                std::make_unique<ExternalFencePool<VulkanDispatch>>(deviceVk, unboxedDevice);

            CommandPoolInfo& poolInfo = impl.mCommandPoolInfo[unboxedCmdPool];
            poolInfo.device = unboxedDevice;
            poolInfo.boxed = boxedCmdPool;
            poolInfo.cmdBuffers.insert(unboxedCmdBuf);

            CommandBufferInfo& cmdInfo = impl.mCommandBufferInfo[unboxedCmdBuf];
            cmdInfo.device = unboxedDevice;
            cmdInfo.cmdPool = unboxedCmdPool;
            cmdInfo.boxed = boxedCmdBuf;

            BufferInfo& bufInfo = impl.mBufferInfo[unboxedBuffer];
            bufInfo.device = unboxedDevice;
            bufInfo.boxed = boxedBuffer;

            MemoryInfo& memInfo = impl.mMemoryInfo[unboxedMem];
            memInfo.device = unboxedDevice;
            memInfo.boxed = boxedMem;
            memInfo.directMapped = true;

            DescriptorPoolInfo& descPoolInfo = impl.mDescriptorPoolInfo[unboxedDescPool];
            descPoolInfo.device = unboxedDevice;
            descPoolInfo.boxed = boxedDescPool;

            DescriptorUpdateTemplateInfo& tmplInfo =
                impl.mDescriptorUpdateTemplateInfo[unboxedTemplate];
            tmplInfo.device = unboxedDevice;
            tmplInfo.boxed = boxedTemplate;
            tmplInfo.data.resize(256, 0xab);

            SamplerYcbcrConversionInfo& ycbcrInfo = impl.mSamplerYcbcrConversionInfo[unboxedYcbcr];
            ycbcrInfo.device = unboxedDevice;
            ycbcrInfo.boxed = boxedYcbcr;

            ShaderModuleInfo& shaderInfo = impl.mShaderModuleInfo[unboxedShader];
            shaderInfo.device = unboxedDevice;
            shaderInfo.boxed = boxedShader;

            PipelineInfo& pipeInfo = impl.mPipelineInfo[unboxedPipeline];
            pipeInfo.device = unboxedDevice;
            pipeInfo.boxed = boxedPipeline;

            PipelineCacheInfo& cacheInfo = impl.mPipelineCacheInfo[unboxedCache];
            cacheInfo.device = unboxedDevice;
            cacheInfo.boxed = boxedCache;

            PipelineLayoutInfo& layoutInfo = impl.mPipelineLayoutInfo[unboxedLayout];
            layoutInfo.device = unboxedDevice;
            layoutInfo.boxed = boxedLayout;

            FramebufferInfo& fbInfo = impl.mFramebufferInfo[unboxedFb];
            fbInfo.device = unboxedDevice;
            fbInfo.boxed = boxedFb;

            RenderPassInfo& rpInfo = impl.mRenderPassInfo[unboxedRp];
            rpInfo.device = unboxedDevice;
            rpInfo.boxed = boxedRp;
        }
        return probe;
    }

    // Mirrors process cleanup: extracts the device and every object it owns
    // from the Impl info maps.
    InstanceObjects::DeviceObjects extractDeviceObjectsForCleanupTest(VkDevice unboxedDevice) {
        InstanceObjects::DeviceObjects deviceObjects;
        std::lock_guard<std::mutex> lock(impl.mMutex);
        auto devIt = impl.mDeviceInfo.find(unboxedDevice);
        deviceObjects.device = impl.mDeviceInfo.extract(devIt);
        impl.extractDeviceAndDependenciesLocked(unboxedDevice, deviceObjects);
        return deviceObjects;
    }

    size_t descriptorUpdateTemplateInfoCount() {
        std::lock_guard<std::mutex> lock(impl.mMutex);
        return impl.mDescriptorUpdateTemplateInfo.size();
    }

    size_t samplerYcbcrConversionInfoCount() {
        std::lock_guard<std::mutex> lock(impl.mMutex);
        return impl.mSamplerYcbcrConversionInfo.size();
    }

    void destroyDeviceObjects(InstanceObjects::DeviceObjects& deviceObjects) {
        impl.destroyDeviceObjects(deviceObjects);
    }

    // Registers `unboxedInstance` with already boxed physical devices, as if
    // the first vkEnumeratePhysicalDevices call had populated them. Returns
    // the boxed instance handle.
    Boxed<VkInstance> registerInstanceWithPhysicalDevicesForCleanupTest(
        VulkanDispatch* globalVk, VkInstance unboxedInstance,
        const std::vector<VkPhysicalDevice>& unboxedPhysicalDevices,
        std::vector<Boxed<VkPhysicalDevice>> boxedPhysicalDevices) {
        impl.m_vk = globalVk;
        Boxed<VkInstance> boxedInstance = new_boxed_VkInstance(unboxedInstance, globalVk);

        std::lock_guard<std::mutex> lock(impl.mMutex);
        InstanceInfo& instanceInfo = impl.mInstanceInfo[unboxedInstance];
        instanceInfo.boxed = boxedInstance;
        instanceInfo.boxedPhysicalDevices = std::move(boxedPhysicalDevices);
        for (VkPhysicalDevice unboxedPhysicalDevice : unboxedPhysicalDevices) {
            impl.mPhysdevInfo[unboxedPhysicalDevice].instance = unboxedInstance;
        }
        return boxedInstance;
    }

    VkResult enumeratePhysicalDevices(Boxed<VkInstance> boxedInstance,
                                      uint32_t* pPhysicalDeviceCount,
                                      VkPhysicalDevice* pPhysicalDevices) {
        return impl.on_vkEnumeratePhysicalDevices(nullptr, kInvalidSnapshotApiCallHandle,
                                                  boxedInstance, pPhysicalDeviceCount,
                                                  pPhysicalDevices);
    }

    size_t physicalDeviceInfoCount() {
        std::lock_guard<std::mutex> lock(impl.mMutex);
        return impl.mPhysdevInfo.size();
    }

    // Mirrors vkDestroyInstanceImpl: extracts the instance and everything it
    // owns from the Impl info maps.
    InstanceObjects extractInstanceObjectsForCleanupTest(VkInstance unboxedInstance) {
        InstanceObjects instanceObjects;
        std::lock_guard<std::mutex> lock(impl.mMutex);
        impl.extractInstanceAndDependenciesLocked(unboxedInstance, instanceObjects);
        return instanceObjects;
    }

    void destroyInstanceObjects(InstanceObjects& instanceObjects) {
        impl.destroyInstanceObjects(instanceObjects);
    }

    void setBatchedDescriptorSetUpdateEnabled(bool enabled) {
        impl.mBatchedDescriptorSetUpdateEnabled = enabled;
    }

    VkResult createDescriptorSetLayout(Boxed<VkDevice> boxedDevice,
                                       const VkDescriptorSetLayoutCreateInfo* createInfo,
                                       Boxed<VkDescriptorSetLayout>* outBoxedLayout) {
        VkDescriptorSetLayout rawLayout = VK_NULL_HANDLE;
        VkResult res = impl.on_vkCreateDescriptorSetLayout(
            nullptr, kInvalidSnapshotApiCallHandle, boxedDevice, createInfo, nullptr, &rawLayout);
        *outBoxedLayout = Boxed<VkDescriptorSetLayout>(rawLayout);
        return res;
    }

    void destroyDescriptorSetLayout(Boxed<VkDevice> boxedDevice,
                                    VkDescriptorSetLayout unboxedLayout) {
        impl.on_vkDestroyDescriptorSetLayout(nullptr, kInvalidSnapshotApiCallHandle, boxedDevice,
                                             unboxedLayout, nullptr);
    }

    VkResult createDescriptorPool(Boxed<VkDevice> boxedDevice,
                                  const VkDescriptorPoolCreateInfo* createInfo,
                                  Boxed<VkDescriptorPool>* outBoxedPool) {
        VkDescriptorPool rawPool = VK_NULL_HANDLE;
        VkResult res = impl.on_vkCreateDescriptorPool(nullptr, kInvalidSnapshotApiCallHandle,
                                                      boxedDevice, createInfo, nullptr, &rawPool);
        *outBoxedPool = Boxed<VkDescriptorPool>(rawPool);
        return res;
    }

    VkResult resetDescriptorPool(Boxed<VkDevice> boxedDevice, VkDescriptorPool unboxedPool) {
        return impl.on_vkResetDescriptorPool(nullptr, kInvalidSnapshotApiCallHandle, boxedDevice,
                                             unboxedPool, 0);
    }

    void destroyDescriptorPool(Boxed<VkDevice> boxedDevice, VkDescriptorPool unboxedPool) {
        impl.on_vkDestroyDescriptorPool(nullptr, kInvalidSnapshotApiCallHandle, boxedDevice,
                                        unboxedPool, nullptr);
    }

    VkResult allocateDescriptorSets(Boxed<VkDevice> boxedDevice,
                                    const VkDescriptorSetAllocateInfo* allocInfo,
                                    VkDescriptorSet* outBoxedSets) {
        return impl.on_vkAllocateDescriptorSets(nullptr, kInvalidSnapshotApiCallHandle, boxedDevice,
                                                allocInfo, outBoxedSets);
    }

    VkResult freeDescriptorSets(Boxed<VkDevice> boxedDevice, VkDescriptorPool unboxedPool,
                                uint32_t count, const VkDescriptorSet* unboxedSets) {
        return impl.on_vkFreeDescriptorSets(nullptr, kInvalidSnapshotApiCallHandle, boxedDevice,
                                            unboxedPool, count, unboxedSets);
    }

    void updateDescriptorSets(gfxstream::base::BumpPool* pool, Boxed<VkDevice> boxedDevice,
                              uint32_t writeCount, const VkWriteDescriptorSet* writes,
                              uint32_t copyCount, const VkCopyDescriptorSet* copies) {
        impl.on_vkUpdateDescriptorSets(pool, kInvalidSnapshotApiCallHandle, boxedDevice, writeCount,
                                       writes, copyCount, copies);
    }

    void registerPreallocatedPoolWithState(VkDevice unboxedDevice, VkDescriptorPool unboxedPool,
                                           Boxed<VkDescriptorPool> boxedPool, uint32_t maxSets,
                                           VkDescriptorType type, uint32_t descriptorCount,
                                           std::vector<uint64_t> preallocatedIds) {
        std::lock_guard<std::mutex> lock(impl.mMutex);
        DescriptorPoolInfo info = {};
        info.device = unboxedDevice;
        info.boxed = boxedPool;
        info.maxSets = maxSets;
        info.usedSets = 0;
        info.pools.push_back({type, descriptorCount, 0});
        info.preallocatedBoxedSets = std::move(preallocatedIds);
        impl.mDescriptorPoolInfo[unboxedPool] = std::move(info);
    }

    uint32_t descriptorPoolUsedSets(VkDescriptorPool unboxedPool) {
        std::lock_guard<std::mutex> lock(impl.mMutex);
        return impl.mDescriptorPoolInfo.at(unboxedPool).usedSets;
    }

    uint32_t descriptorPoolUsedCount(VkDescriptorPool unboxedPool, size_t poolIdx = 0) {
        std::lock_guard<std::mutex> lock(impl.mMutex);
        return impl.mDescriptorPoolInfo.at(unboxedPool).pools.at(poolIdx).used;
    }

    size_t sparseBindingWriteCount(VkDescriptorSet unboxedSet, uint32_t binding) {
        std::lock_guard<std::mutex> lock(impl.mMutex);
        return impl.mDescriptorSetInfo.at(unboxedSet).allWrites.at(binding).size();
    }

    bool sparseBindingHasImageView(VkDescriptorSet unboxedSet, uint32_t binding, uint32_t elem,
                                   VkImageView expectedView) {
        std::lock_guard<std::mutex> lock(impl.mMutex);
        const auto& map = impl.mDescriptorSetInfo.at(unboxedSet).allWrites.at(binding);
        auto it = map.find(elem);
        return it != map.end() &&
               it->second.writeType == DescriptorSetInfo::DescriptorWriteType::ImageInfo &&
               it->second.imageInfo.imageView == expectedView;
    }
};

namespace {

TEST(VkDecoderGlobalStateDescriptorSetPoolIdTest, BogusPoolIdReturnsNullWithoutCrash) {
    VkDecoderGlobalStateTestPeer peer;
    VulkanDispatch vk = {};
    VkDevice device = reinterpret_cast<VkDevice>(0x1111'0000);
    VkDescriptorPool pool = reinterpret_cast<VkDescriptorPool>(0x2222'0000);
    VkDescriptorSetLayout setLayout = reinterpret_cast<VkDescriptorSetLayout>(0x3333'0000);

    peer.registerDescriptorPool(pool);

    bool didAlloc = true;
    constexpr uint64_t kBogusPoolId = 0xdeadbeef'cafebabeULL;
    EXPECT_EQ(nullptr, sBoxedHandleManager.get(kBogusPoolId));

    VkDescriptorSet result = peer.getOrAllocateDescriptorSetFromPoolAndId(
        &vk, device, pool, setLayout, kBogusPoolId, /*pendingAlloc=*/1, &didAlloc);
    EXPECT_EQ(VK_NULL_HANDLE, result);
    EXPECT_FALSE(didAlloc);

    gfxstream::base::BumpPool bumpPool;
    uint32_t whichPool = 0;
    uint32_t pendingAlloc = 1;
    uint32_t writeStartIndex = 0;
    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    };
    peer.commitDescriptorSetUpdates(
        &bumpPool, &vk, device,
        /*descriptorPoolCount=*/1, &pool,
        /*descriptorSetCount=*/1, &setLayout, &kBogusPoolId, &whichPool, &pendingAlloc,
        &writeStartIndex, /*pendingDescriptorWriteCount=*/1, &write);
}

TEST(VkDecoderGlobalStateDescriptorSetPoolIdTest, UnallocatedAndAllocatedPoolIdHandling) {
    VkDecoderGlobalStateTestPeer peer;
    VulkanDispatch vk = {};
    VkDevice device = reinterpret_cast<VkDevice>(0x1111'0000);
    VkDescriptorPool pool = reinterpret_cast<VkDescriptorPool>(0x2222'0000);
    VkDescriptorSetLayout setLayout = reinterpret_cast<VkDescriptorSetLayout>(0x3333'0000);
    VkDescriptorSet expectedSet = reinterpret_cast<VkDescriptorSet>(0x4444'0000);

    peer.registerDescriptorPool(pool);

    Boxed<VkDescriptorSet> boxedNullSet = new_boxed_non_dispatchable_VkDescriptorSet(VK_NULL_HANDLE);
    uint64_t nullPoolId = reinterpret_cast<uint64_t>(boxedNullSet.value());
    bool didAlloc = true;
    EXPECT_EQ(VK_NULL_HANDLE,
              peer.getOrAllocateDescriptorSetFromPoolAndId(
                  &vk, device, pool, setLayout, nullPoolId, /*pendingAlloc=*/0, &didAlloc));
    EXPECT_FALSE(didAlloc);
    delete_VkDescriptorSet(boxedNullSet);

    Boxed<VkDescriptorSet> boxedValidSet = new_boxed_non_dispatchable_VkDescriptorSet(expectedSet);
    uint64_t validPoolId = reinterpret_cast<uint64_t>(boxedValidSet.value());
    didAlloc = true;
    EXPECT_EQ(expectedSet,
              peer.getOrAllocateDescriptorSetFromPoolAndId(
                  &vk, device, pool, setLayout, validPoolId, /*pendingAlloc=*/0, &didAlloc));
    EXPECT_FALSE(didAlloc);
    delete_VkDescriptorSet(boxedValidSet);
}

TEST(VkDecoderGlobalStateDescriptorSetPoolIdTest, CollectDescriptorPoolIdsClampsCount) {
    VkDecoderGlobalStateTestPeer peer;
    gfxstream::base::BumpPool bumpPool;
    VkDevice device = reinterpret_cast<VkDevice>(0x1111'0000);
    VkDescriptorPool pool = reinterpret_cast<VkDescriptorPool>(0x2222'0000);

    peer.registerDescriptorPool(pool, {101, 102, 103, 104});

    uint32_t count = 0;
    peer.collectDescriptorPoolIds(&bumpPool, device, pool, &count, nullptr);
    EXPECT_EQ(4u, count);

    uint64_t outIds[4] = {0, 0, 0, 0};
    count = 1;
    peer.collectDescriptorPoolIds(&bumpPool, device, pool, &count, outIds);
    EXPECT_EQ(1u, count);
    EXPECT_EQ(101u, outIds[0]);
    EXPECT_EQ(0u, outIds[1]);

    count = 10;
    peer.collectDescriptorPoolIds(&bumpPool, device, pool, &count, outIds);
    EXPECT_EQ(4u, count);
    EXPECT_EQ(101u, outIds[0]);
    EXPECT_EQ(102u, outIds[1]);
    EXPECT_EQ(103u, outIds[2]);
    EXPECT_EQ(104u, outIds[3]);
}

TEST(VkDecoderGlobalStateBoxedHandleTest, OrderMaintenanceInfoOutlivesDeletedHandle) {
    Boxed<VkCommandBuffer> boxed =
        new_boxed_VkCommandBuffer(reinterpret_cast<VkCommandBuffer>(0x5555'0000), nullptr);
    std::shared_ptr<OrderMaintenanceInfo> order = ordmaint_VkCommandBuffer(boxed);
    ASSERT_NE(nullptr, order);
    EXPECT_EQ(order, ordmaint_VkCommandBuffer(boxed));

    delete_VkCommandBuffer(boxed);
    EXPECT_EQ(nullptr, ordmaint_VkCommandBuffer(boxed));
    EXPECT_EQ(1, order.use_count());
}

TEST(VkDecoderGlobalStateBoxedHandleTest, ConcurrentOrderMaintenanceInfoLookupAndDelete) {
    constexpr int kIterations = 20000;
    std::atomic<uint64_t> currentHandle{0};
    std::atomic<bool> done{false};

    std::thread reader([&]() {
        while (!done.load(std::memory_order_acquire)) {
            Boxed<VkCommandBuffer> boxed =
                Boxed<VkCommandBuffer>::fromRaw(currentHandle.load(std::memory_order_acquire));
            std::shared_ptr<OrderMaintenanceInfo> order = ordmaint_VkCommandBuffer(boxed);
            if (order) {
                EXPECT_EQ(0u, __atomic_load_n(&order->sequenceNumber, __ATOMIC_ACQUIRE));
            }
        }
    });

    for (int i = 0; i < kIterations; ++i) {
        Boxed<VkCommandBuffer> boxed =
            new_boxed_VkCommandBuffer(reinterpret_cast<VkCommandBuffer>(0x5555'0000), nullptr);
        currentHandle.store(reinterpret_cast<uint64_t>(boxed.value()), std::memory_order_release);
        delete_VkCommandBuffer(boxed);
    }

    done.store(true, std::memory_order_release);
    reader.join();
}

// Regression test for b/573373414: the host driver can hand out the same unboxed handle value
// for a new object right after the previous object was destroyed, and the new boxed handle may be
// registered before the old boxed handle is removed. Removing the old boxed handle must not drop
// the reverse mapping owned by the new boxed handle, otherwise unboxed_to_boxed_* lookups fail
// and (among other things) snapshot dependency tracking for dedicated allocations breaks.
TEST(BoxedHandleManagerReverseMapTest, RemoveOldBoxedHandleKeepsReusedUnboxedMapping) {
    VkImage underlying = reinterpret_cast<VkImage>(0x5555'0000);

    Boxed<VkImage> boxedOld = new_boxed_non_dispatchable_VkImage(underlying);
    ASSERT_NE(Boxed<VkImage>(), boxedOld);
    EXPECT_EQ(boxedOld, unboxed_to_boxed_non_dispatchable_VkImage(underlying));

    // Host destroys the old image and immediately reuses the handle value for a new image, whose
    // boxed handle gets registered before the old boxed handle is deleted.
    Boxed<VkImage> boxedNew = new_boxed_non_dispatchable_VkImage(underlying);
    ASSERT_NE(Boxed<VkImage>(), boxedNew);
    ASSERT_NE(boxedOld, boxedNew);
    EXPECT_EQ(boxedNew, unboxed_to_boxed_non_dispatchable_VkImage(underlying));

    delete_VkImage(boxedOld);
    EXPECT_EQ(nullptr, sBoxedHandleManager.get(boxedOld));
    // The new boxed handle must still be reachable from the (reused) unboxed handle.
    EXPECT_EQ(boxedNew, unboxed_to_boxed_non_dispatchable_VkImage(underlying));

    delete_VkImage(boxedNew);
    EXPECT_EQ(Boxed<VkImage>(), unboxed_to_boxed_non_dispatchable_VkImage(underlying));
}

TEST(BoxedHandleManagerReverseMapTest, UpdateOldUnboxedDoesNotDropReusedMapping) {
    VkImage underlyingA = reinterpret_cast<VkImage>(0x6666'0000);
    VkImage underlyingB = reinterpret_cast<VkImage>(0x7777'0000);

    Boxed<VkImage> boxed1 = new_boxed_non_dispatchable_VkImage(underlyingA);
    // Another boxed handle takes over underlyingA (reuse) before boxed1 is re-pointed.
    Boxed<VkImage> boxed2 = new_boxed_non_dispatchable_VkImage(underlyingA);
    EXPECT_EQ(boxed2, unboxed_to_boxed_non_dispatchable_VkImage(underlyingA));

    set_boxed_non_dispatchable_VkImage(boxed1, underlyingB);
    EXPECT_EQ(boxed1, unboxed_to_boxed_non_dispatchable_VkImage(underlyingB));
    // boxed2's mapping for underlyingA must survive boxed1's update.
    EXPECT_EQ(boxed2, unboxed_to_boxed_non_dispatchable_VkImage(underlyingA));

    delete_VkImage(boxed1);
    delete_VkImage(boxed2);
    EXPECT_EQ(Boxed<VkImage>(), unboxed_to_boxed_non_dispatchable_VkImage(underlyingA));
    EXPECT_EQ(Boxed<VkImage>(), unboxed_to_boxed_non_dispatchable_VkImage(underlyingB));
}

TEST(BoxedHandleManagerReverseMapTest, NullUnboxedHandleIsNeverReverseMapped) {
    // Boxing VK_NULL_HANDLE must not register key 0 in the reverse map: it could never be erased
    // (eraseReverseMappingLocked skips 0) and would make lookups of VK_NULL_HANDLE return a
    // stale boxed handle.
    Boxed<VkImage> boxedNull = new_boxed_non_dispatchable_VkImage(VK_NULL_HANDLE);
    ASSERT_NE(Boxed<VkImage>(), boxedNull);
    EXPECT_EQ(Boxed<VkImage>(), unboxed_to_boxed_non_dispatchable_VkImage(VK_NULL_HANDLE));

    VkImage underlying = reinterpret_cast<VkImage>(0x9999'0000);
    set_boxed_non_dispatchable_VkImage(boxedNull, underlying);
    EXPECT_EQ(boxedNull, unboxed_to_boxed_non_dispatchable_VkImage(underlying));
    set_boxed_non_dispatchable_VkImage(boxedNull, VK_NULL_HANDLE);
    EXPECT_EQ(Boxed<VkImage>(), unboxed_to_boxed_non_dispatchable_VkImage(underlying));
    EXPECT_EQ(Boxed<VkImage>(), unboxed_to_boxed_non_dispatchable_VkImage(VK_NULL_HANDLE));

    delete_VkImage(boxedNull);
    EXPECT_EQ(Boxed<VkImage>(), unboxed_to_boxed_non_dispatchable_VkImage(VK_NULL_HANDLE));
}

TEST(BoxedHandleManagerReverseMapTest, DelayedRemoveClearsReverseMapping) {
    VkDevice device = reinterpret_cast<VkDevice>(0x1111'0000);
    VkSampler underlying = reinterpret_cast<VkSampler>(0x8888'0000);

    Boxed<VkSampler> boxed = new_boxed_non_dispatchable_VkSampler(underlying);
    EXPECT_EQ(boxed, unboxed_to_boxed_non_dispatchable_VkSampler(underlying));

    delayed_delete_VkSampler(boxed, device, nullptr);
    // Still alive until the delayed removes for the device are processed.
    EXPECT_EQ(boxed, unboxed_to_boxed_non_dispatchable_VkSampler(underlying));

    sBoxedHandleManager.processDelayedRemoves(device);
    EXPECT_EQ(nullptr, sBoxedHandleManager.get(boxed));
    EXPECT_EQ(Boxed<VkSampler>(), unboxed_to_boxed_non_dispatchable_VkSampler(underlying));
}

TEST(VkDecoderGlobalStateProcessCleanupLeakTest, DestroyCommandPoolDeletesBoxedCommandBuffers) {
    VkDecoderGlobalStateTestPeer peer;
    const uint64_t baselineHandles = sBoxedHandleManager.getHandlesCount();

    static int sDestroyedPools = 0;
    sDestroyedPools = 0;
    VulkanDispatch deviceVk = {};
    deviceVk.vkDestroyCommandPool = [](VkDevice, VkCommandPool, const VkAllocationCallbacks*) {
        ++sDestroyedPools;
    };

    VkDevice unboxedDevice = reinterpret_cast<VkDevice>(0xb001'0000ULL);
    VkCommandPool unboxedPool = reinterpret_cast<VkCommandPool>(0xb002'0000ULL);
    Boxed<VkCommandPool> boxedPool = new_boxed_non_dispatchable_VkCommandPool(unboxedPool);
    VkCommandBuffer unboxedCmdBuf = reinterpret_cast<VkCommandBuffer>(0xb003'0000ULL);
    Boxed<VkCommandBuffer> boxedCmdBuf = new_boxed_VkCommandBuffer(unboxedCmdBuf, &deviceVk);
    BoxedHandleInfo* cmdBufBoxedInfo = sBoxedHandleManager.get(boxedCmdBuf);
    ASSERT_NE(nullptr, cmdBufBoxedInfo);
    cmdBufBoxedInfo->readStream =
        std::make_unique<VulkanMemReadingStream>(nullptr, gfxstream::host::FeatureSet{});
    std::shared_ptr<OrderMaintenanceInfo> cmdBufOrder = ordmaint_VkCommandBuffer(boxedCmdBuf);
    ASSERT_NE(nullptr, cmdBufOrder);
    EXPECT_EQ(2, cmdBufOrder.use_count());

    CommandPoolInfo poolInfo;
    poolInfo.device = unboxedDevice;
    poolInfo.boxed = boxedPool;
    poolInfo.cmdBuffers.insert(unboxedCmdBuf);

    std::unordered_map<VkCommandBuffer, CommandBufferInfo> cmdBufferInfos;
    CommandBufferInfo& cmdInfo = cmdBufferInfos[unboxedCmdBuf];
    cmdInfo.device = unboxedDevice;
    cmdInfo.cmdPool = unboxedPool;
    cmdInfo.boxed = boxedCmdBuf;

    peer.destroyCommandPoolWithExclusiveInfo(unboxedDevice, &deviceVk, unboxedPool, poolInfo,
                                             cmdBufferInfos);
    delete_VkCommandPool(boxedPool);

    EXPECT_EQ(1, sDestroyedPools);
    EXPECT_TRUE(cmdBufferInfos.empty());
    EXPECT_EQ(nullptr, sBoxedHandleManager.get(boxedCmdBuf));
    EXPECT_EQ(1, cmdBufOrder.use_count());
    EXPECT_EQ(baselineHandles, sBoxedHandleManager.getHandlesCount());
}

TEST(VkDecoderGlobalStateProcessCleanupLeakTest, DeviceCleanupDeletesAllExtractedBoxedHandles) {
    VkDecoderGlobalStateTestPeer peer;
    const uint64_t baselineHandles = sBoxedHandleManager.getHandlesCount();

    static int sDestroyCalls = 0;
    sDestroyCalls = 0;
    VulkanDispatch globalVk = {};
    globalVk.vkDestroyDevice = [](VkDevice, const VkAllocationCallbacks*) { ++sDestroyCalls; };
    VulkanDispatch deviceVk = {};
    deviceVk.vkDeviceWaitIdle = [](VkDevice) -> VkResult { return VK_SUCCESS; };
    deviceVk.vkDestroyBuffer = [](VkDevice, VkBuffer, const VkAllocationCallbacks*) {
        ++sDestroyCalls;
    };
    deviceVk.vkFreeMemory = [](VkDevice, VkDeviceMemory, const VkAllocationCallbacks*) {
        ++sDestroyCalls;
    };
    deviceVk.vkFreeCommandBuffers = [](VkDevice, VkCommandPool, uint32_t, const VkCommandBuffer*) {
        ++sDestroyCalls;
    };
    deviceVk.vkDestroyCommandPool = [](VkDevice, VkCommandPool, const VkAllocationCallbacks*) {
        ++sDestroyCalls;
    };
    deviceVk.vkDestroyDescriptorPool =
        [](VkDevice, VkDescriptorPool, const VkAllocationCallbacks*) { ++sDestroyCalls; };
    deviceVk.vkDestroyDescriptorSetLayout =
        [](VkDevice, VkDescriptorSetLayout layout, const VkAllocationCallbacks*) {
            if (layout != VK_NULL_HANDLE) ++sDestroyCalls;
        };
    deviceVk.vkDestroyDescriptorUpdateTemplate =
        [](VkDevice, VkDescriptorUpdateTemplate, const VkAllocationCallbacks*) { ++sDestroyCalls; };
    deviceVk.vkDestroySamplerYcbcrConversion =
        [](VkDevice, VkSamplerYcbcrConversion, const VkAllocationCallbacks*) { ++sDestroyCalls; };
    deviceVk.vkDestroyShaderModule = [](VkDevice, VkShaderModule, const VkAllocationCallbacks*) {
        ++sDestroyCalls;
    };
    deviceVk.vkDestroyPipeline = [](VkDevice, VkPipeline, const VkAllocationCallbacks*) {
        ++sDestroyCalls;
    };
    deviceVk.vkDestroyPipelineCache = [](VkDevice, VkPipelineCache, const VkAllocationCallbacks*) {
        ++sDestroyCalls;
    };
    deviceVk.vkDestroyPipelineLayout =
        [](VkDevice, VkPipelineLayout layout, const VkAllocationCallbacks*) {
            if (layout != VK_NULL_HANDLE) ++sDestroyCalls;
        };
    deviceVk.vkDestroyFramebuffer = [](VkDevice, VkFramebuffer, const VkAllocationCallbacks*) {
        ++sDestroyCalls;
    };
    deviceVk.vkDestroyRenderPass = [](VkDevice, VkRenderPass, const VkAllocationCallbacks*) {
        ++sDestroyCalls;
    };

    VkDevice unboxedDevice = reinterpret_cast<VkDevice>(0xc001'0000ULL);
    CommandBufferCleanupProbe probe =
        peer.registerDeviceObjectsForCleanupTest(&globalVk, &deviceVk, unboxedDevice);

    InstanceObjects::DeviceObjects deviceObjects =
        peer.extractDeviceObjectsForCleanupTest(unboxedDevice);
    EXPECT_EQ(0u, peer.descriptorUpdateTemplateInfoCount());
    EXPECT_EQ(0u, peer.samplerYcbcrConversionInfoCount());

    peer.destroyDeviceObjects(deviceObjects);

    EXPECT_EQ(14, sDestroyCalls);
    EXPECT_EQ(nullptr, sBoxedHandleManager.get(probe.boxedCmdBuf));
    ASSERT_NE(nullptr, probe.cmdBufOrder);
    EXPECT_EQ(1, probe.cmdBufOrder.use_count());
    EXPECT_EQ(baselineHandles, sBoxedHandleManager.getHandlesCount());
}

TEST(VkDecoderGlobalStateProcessCleanupLeakTest,
     EnumeratePhysicalDevicesReturnsInstanceOwnedHandlesUntilInstanceDestroyed) {
    VkDecoderGlobalStateTestPeer peer;
    const uint64_t baselineHandles = sBoxedHandleManager.getHandlesCount();

    static int sDestroyedInstances = 0;
    sDestroyedInstances = 0;
    VulkanDispatch globalVk = {};
    globalVk.vkDestroyInstance = [](VkInstance, const VkAllocationCallbacks*) {
        ++sDestroyedInstances;
    };

    VkInstance unboxedInstance = reinterpret_cast<VkInstance>(0xd001'0000ULL);
    const std::vector<VkPhysicalDevice> unboxedPhysicalDevices = {
        reinterpret_cast<VkPhysicalDevice>(0xd002'0000ULL),
        reinterpret_cast<VkPhysicalDevice>(0xd003'0000ULL),
    };
    const std::vector<Boxed<VkPhysicalDevice>> boxedPhysicalDevices = {
        new_boxed_VkPhysicalDevice(unboxedPhysicalDevices[0], &globalVk),
        new_boxed_VkPhysicalDevice(unboxedPhysicalDevices[1], &globalVk),
    };
    Boxed<VkInstance> boxedInstance = peer.registerInstanceWithPhysicalDevicesForCleanupTest(
        &globalVk, unboxedInstance, unboxedPhysicalDevices, boxedPhysicalDevices);
    EXPECT_EQ(2u, peer.physicalDeviceInfoCount());

    // Count-only query reports every physical device without writing handles.
    uint32_t count = 0;
    EXPECT_EQ(VK_SUCCESS, peer.enumeratePhysicalDevices(boxedInstance, &count, nullptr));
    EXPECT_EQ(2u, count);

    // An array that is too small gets only as many handles as it can hold,
    // the count is updated to the number written and VK_INCOMPLETE returned.
    VkPhysicalDevice out[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
    count = 1;
    EXPECT_EQ(VK_INCOMPLETE, peer.enumeratePhysicalDevices(boxedInstance, &count, out));
    EXPECT_EQ(1u, count);
    EXPECT_EQ(boxedPhysicalDevices[0].value(), out[0]);
    EXPECT_EQ(VK_NULL_HANDLE, out[1]);

    // Repeated enumeration on the same instance returns the same boxed
    // handles and does not box new ones.
    for (int i = 0; i < 2; ++i) {
        out[0] = out[1] = VK_NULL_HANDLE;
        count = 2;
        EXPECT_EQ(VK_SUCCESS, peer.enumeratePhysicalDevices(boxedInstance, &count, out));
        EXPECT_EQ(2u, count);
        EXPECT_EQ(boxedPhysicalDevices[0].value(), out[0]);
        EXPECT_EQ(boxedPhysicalDevices[1].value(), out[1]);
    }
    EXPECT_EQ(baselineHandles + 3, sBoxedHandleManager.getHandlesCount());

    // Destroying the instance releases its physical devices with it.
    InstanceObjects instanceObjects = peer.extractInstanceObjectsForCleanupTest(unboxedInstance);
    EXPECT_EQ(0u, peer.physicalDeviceInfoCount());
    peer.destroyInstanceObjects(instanceObjects);

    EXPECT_EQ(1, sDestroyedInstances);
    for (Boxed<VkPhysicalDevice> boxedPhysicalDevice : boxedPhysicalDevices) {
        EXPECT_EQ(nullptr, sBoxedHandleManager.get(boxedPhysicalDevice));
    }
    EXPECT_EQ(nullptr, sBoxedHandleManager.get(boxedInstance));
    EXPECT_EQ(baselineHandles, sBoxedHandleManager.getHandlesCount());
}

TEST(VkDecoderGlobalStateBindlessDescriptorTest,
     VariableDescriptorCountPoolValidationAndDefaultZero) {
    VkDecoderGlobalStateTestPeer peer;
    peer.setBatchedDescriptorSetUpdateEnabled(true);

    static uint64_t sNextHandle = 0xe100'0000ULL;
    VulkanDispatch deviceVk = {};
    deviceVk.vkCreateDescriptorSetLayout = [](VkDevice, const VkDescriptorSetLayoutCreateInfo*,
                                              const VkAllocationCallbacks*,
                                              VkDescriptorSetLayout* pSetLayout) -> VkResult {
        *pSetLayout = reinterpret_cast<VkDescriptorSetLayout>(++sNextHandle);
        return VK_SUCCESS;
    };
    deviceVk.vkDestroyDescriptorSetLayout = [](VkDevice, VkDescriptorSetLayout,
                                               const VkAllocationCallbacks*) {};
    deviceVk.vkAllocateDescriptorSets = [](VkDevice, const VkDescriptorSetAllocateInfo* ci,
                                           VkDescriptorSet* pSets) -> VkResult {
        for (uint32_t i = 0; i < ci->descriptorSetCount; ++i) {
            pSets[i] = reinterpret_cast<VkDescriptorSet>(++sNextHandle);
        }
        return VK_SUCCESS;
    };
    deviceVk.vkDestroyDescriptorPool = [](VkDevice, VkDescriptorPool,
                                          const VkAllocationCallbacks*) {};

    VkDevice unboxedDevice = reinterpret_cast<VkDevice>(0xe001'0000ULL);
    Boxed<VkDevice> boxedDevice = new_boxed_VkDevice(unboxedDevice, &deviceVk);
    VkDescriptorPool unboxedPool = reinterpret_cast<VkDescriptorPool>(0xe002'0000ULL);
    Boxed<VkDescriptorPool> boxedPool = new_boxed_non_dispatchable_VkDescriptorPool(unboxedPool);
    peer.registerPreallocatedPoolWithState(unboxedDevice, unboxedPool, boxedPool, /*maxSets=*/3,
                                           VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                           /*descriptorCount=*/64, {});

    VkDescriptorSetLayoutBinding binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .descriptorCount = 500000,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };
    VkDescriptorBindingFlags bindingFlag = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
    VkDescriptorSetLayoutBindingFlagsCreateInfo flagsCi = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = 1,
        .pBindingFlags = &bindingFlag,
    };
    VkDescriptorSetLayoutCreateInfo layoutCi = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &flagsCi,
        .bindingCount = 1,
        .pBindings = &binding,
    };
    Boxed<VkDescriptorSetLayout> boxedLayout;
    ASSERT_EQ(VK_SUCCESS, peer.createDescriptorSetLayout(boxedDevice, &layoutCi, &boxedLayout));
    VkDescriptorSetLayout unboxedLayout = unbox_VkDescriptorSetLayout(boxedLayout);

    // Exceeding pool capacity via explicit variable count fails validation.
    uint32_t tooMany = 128;
    VkDescriptorSetVariableDescriptorCountAllocateInfo varAi = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .descriptorSetCount = 1,
        .pDescriptorCounts = &tooMany,
    };
    VkDescriptorSetAllocateInfo allocCi = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = &varAi,
        .descriptorPool = unboxedPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &unboxedLayout,
    };
    VkDescriptorSet boxedSet1 = VK_NULL_HANDLE;
    EXPECT_EQ(VK_ERROR_OUT_OF_POOL_MEMORY,
              peer.allocateDescriptorSets(boxedDevice, &allocCi, &boxedSet1));
    EXPECT_EQ(0u, peer.descriptorPoolUsedSets(unboxedPool));

    // Requesting 16 descriptors from a 500k max-bound layout succeeds and charges 16.
    uint32_t validCount = 16;
    varAi.pDescriptorCounts = &validCount;
    ASSERT_EQ(VK_SUCCESS, peer.allocateDescriptorSets(boxedDevice, &allocCi, &boxedSet1));
    EXPECT_EQ(1u, peer.descriptorPoolUsedSets(unboxedPool));
    EXPECT_EQ(16u, peer.descriptorPoolUsedCount(unboxedPool));

    // Omitting VkDescriptorSetVariableDescriptorCountAllocateInfo defaults the variable count to 0.
    allocCi.pNext = nullptr;
    VkDescriptorSet boxedSet2 = VK_NULL_HANDLE;
    ASSERT_EQ(VK_SUCCESS, peer.allocateDescriptorSets(boxedDevice, &allocCi, &boxedSet2));
    EXPECT_EQ(2u, peer.descriptorPoolUsedSets(unboxedPool));
    EXPECT_EQ(16u, peer.descriptorPoolUsedCount(unboxedPool));

    peer.destroyDescriptorPool(boxedDevice, unboxedPool);
    delete_VkDescriptorPool(boxedPool);
    peer.destroyDescriptorSetLayout(boxedDevice, unboxedLayout);
    delete_VkDescriptorSetLayout(boxedLayout);
    delete_VkDevice(boxedDevice);
}

TEST(VkDecoderGlobalStateBindlessDescriptorTest,
     NonPreallocatedBoxedSetsDoNotLeakUnderBatchedMode) {
    VkDecoderGlobalStateTestPeer peer;
    peer.setBatchedDescriptorSetUpdateEnabled(true);
    const uint64_t baselineHandles = sBoxedHandleManager.getHandlesCount();

    static uint64_t sNextHandle = 0xe200'0000ULL;
    VulkanDispatch deviceVk = {};
    deviceVk.vkCreateDescriptorSetLayout = [](VkDevice, const VkDescriptorSetLayoutCreateInfo*,
                                              const VkAllocationCallbacks*,
                                              VkDescriptorSetLayout* pSetLayout) -> VkResult {
        *pSetLayout = reinterpret_cast<VkDescriptorSetLayout>(++sNextHandle);
        return VK_SUCCESS;
    };
    deviceVk.vkDestroyDescriptorSetLayout = [](VkDevice, VkDescriptorSetLayout,
                                               const VkAllocationCallbacks*) {};
    deviceVk.vkAllocateDescriptorSets = [](VkDevice, const VkDescriptorSetAllocateInfo* ci,
                                           VkDescriptorSet* pSets) -> VkResult {
        for (uint32_t i = 0; i < ci->descriptorSetCount; ++i) {
            pSets[i] = reinterpret_cast<VkDescriptorSet>(++sNextHandle);
        }
        return VK_SUCCESS;
    };
    deviceVk.vkFreeDescriptorSets = [](VkDevice, VkDescriptorPool, uint32_t,
                                       const VkDescriptorSet*) -> VkResult { return VK_SUCCESS; };
    deviceVk.vkResetDescriptorPool = [](VkDevice, VkDescriptorPool,
                                        VkDescriptorPoolResetFlags) -> VkResult {
        return VK_SUCCESS;
    };
    deviceVk.vkDestroyDescriptorPool = [](VkDevice, VkDescriptorPool,
                                          const VkAllocationCallbacks*) {};

    VkDevice unboxedDevice = reinterpret_cast<VkDevice>(0xe201'0000ULL);
    Boxed<VkDevice> boxedDevice = new_boxed_VkDevice(unboxedDevice, &deviceVk);
    VkDescriptorPool unboxedPool = reinterpret_cast<VkDescriptorPool>(0xe202'0000ULL);
    Boxed<VkDescriptorPool> boxedPool = new_boxed_non_dispatchable_VkDescriptorPool(unboxedPool);
    Boxed<VkDescriptorSet> preallocSlot =
        new_boxed_non_dispatchable_VkDescriptorSet(VK_NULL_HANDLE);
    uint64_t preallocId = reinterpret_cast<uint64_t>(preallocSlot.value());
    peer.registerPreallocatedPoolWithState(unboxedDevice, unboxedPool, boxedPool, /*maxSets=*/4,
                                           VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                           /*descriptorCount=*/64, {preallocId});

    VkDescriptorSetLayoutBinding binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .descriptorCount = 1024,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };
    VkDescriptorBindingFlags bindingFlag = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
    VkDescriptorSetLayoutBindingFlagsCreateInfo flagsCi = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = 1,
        .pBindingFlags = &bindingFlag,
    };
    VkDescriptorSetLayoutCreateInfo layoutCi = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &flagsCi,
        .bindingCount = 1,
        .pBindings = &binding,
    };
    Boxed<VkDescriptorSetLayout> boxedLayout;
    ASSERT_EQ(VK_SUCCESS, peer.createDescriptorSetLayout(boxedDevice, &layoutCi, &boxedLayout));
    VkDescriptorSetLayout unboxedLayout = unbox_VkDescriptorSetLayout(boxedLayout);

    uint32_t varCount = 8;
    VkDescriptorSetVariableDescriptorCountAllocateInfo varAi = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .descriptorSetCount = 1,
        .pDescriptorCounts = &varCount,
    };
    VkDescriptorSetAllocateInfo allocCi = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = &varAi,
        .descriptorPool = unboxedPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &unboxedLayout,
    };

    // 1. Individual free deletes non-preallocated boxed handle.
    VkDescriptorSet boxedSync1 = VK_NULL_HANDLE;
    ASSERT_EQ(VK_SUCCESS, peer.allocateDescriptorSets(boxedDevice, &allocCi, &boxedSync1));
    VkDescriptorSet unboxedSync1 = unbox_VkDescriptorSet(Boxed<VkDescriptorSet>(boxedSync1));
    ASSERT_EQ(VK_SUCCESS, peer.freeDescriptorSets(boxedDevice, unboxedPool, 1, &unboxedSync1));
    EXPECT_EQ(nullptr, sBoxedHandleManager.get(reinterpret_cast<uint64_t>(boxedSync1)));
    EXPECT_NE(nullptr, sBoxedHandleManager.get(preallocId));
    EXPECT_EQ(0u, peer.descriptorPoolUsedSets(unboxedPool));
    EXPECT_EQ(0u, peer.descriptorPoolUsedCount(unboxedPool));

    // 2. Pool reset deletes non-preallocated boxed handle while keeping preallocated slot alive.
    VkDescriptorSet boxedSync2 = VK_NULL_HANDLE;
    ASSERT_EQ(VK_SUCCESS, peer.allocateDescriptorSets(boxedDevice, &allocCi, &boxedSync2));
    bool didAlloc = false;
    ASSERT_NE(VK_NULL_HANDLE, peer.getOrAllocateDescriptorSetFromPoolAndId(
                                  &deviceVk, unboxedDevice, unboxedPool, unboxedLayout, preallocId,
                                  /*pendingAlloc=*/1, &didAlloc));
    ASSERT_EQ(VK_SUCCESS, peer.resetDescriptorPool(boxedDevice, unboxedPool));
    EXPECT_EQ(nullptr, sBoxedHandleManager.get(reinterpret_cast<uint64_t>(boxedSync2)));
    ASSERT_NE(nullptr, sBoxedHandleManager.get(preallocId));
    EXPECT_EQ(0u, sBoxedHandleManager.get(preallocId)->underlying);
    EXPECT_EQ(0u, peer.descriptorPoolUsedSets(unboxedPool));

    // 3. Pool destroy deletes both non-preallocated and preallocated boxed handles.
    VkDescriptorSet boxedSync3 = VK_NULL_HANDLE;
    ASSERT_EQ(VK_SUCCESS, peer.allocateDescriptorSets(boxedDevice, &allocCi, &boxedSync3));
    peer.destroyDescriptorPool(boxedDevice, unboxedPool);
    EXPECT_EQ(nullptr, sBoxedHandleManager.get(reinterpret_cast<uint64_t>(boxedSync3)));
    EXPECT_EQ(nullptr, sBoxedHandleManager.get(preallocId));

    delete_VkDescriptorPool(boxedPool);
    peer.destroyDescriptorSetLayout(boxedDevice, unboxedLayout);
    delete_VkDescriptorSetLayout(boxedLayout);
    delete_VkDevice(boxedDevice);
    EXPECT_EQ(baselineHandles, sBoxedHandleManager.getHandlesCount());
}

TEST(VkDecoderGlobalStateBindlessDescriptorTest, ReallocatingPoolIdAndSparseDescriptorCopy) {
    VkDecoderGlobalStateTestPeer peer;
    peer.setBatchedDescriptorSetUpdateEnabled(true);
    gfxstream::base::BumpPool bumpPool;

    static uint64_t sNextHandle = 0xe300'0000ULL;
    VulkanDispatch deviceVk = {};
    deviceVk.vkCreateDescriptorSetLayout = [](VkDevice, const VkDescriptorSetLayoutCreateInfo*,
                                              const VkAllocationCallbacks*,
                                              VkDescriptorSetLayout* pSetLayout) -> VkResult {
        *pSetLayout = reinterpret_cast<VkDescriptorSetLayout>(++sNextHandle);
        return VK_SUCCESS;
    };
    deviceVk.vkDestroyDescriptorSetLayout = [](VkDevice, VkDescriptorSetLayout,
                                               const VkAllocationCallbacks*) {};
    deviceVk.vkAllocateDescriptorSets = [](VkDevice, const VkDescriptorSetAllocateInfo* ci,
                                           VkDescriptorSet* pSets) -> VkResult {
        for (uint32_t i = 0; i < ci->descriptorSetCount; ++i) {
            pSets[i] = reinterpret_cast<VkDescriptorSet>(++sNextHandle);
        }
        return VK_SUCCESS;
    };
    deviceVk.vkFreeDescriptorSets = [](VkDevice, VkDescriptorPool, uint32_t,
                                       const VkDescriptorSet*) -> VkResult { return VK_SUCCESS; };
    static VkDescriptorSet sLastDriverUpdatedDstSet = VK_NULL_HANDLE;
    sLastDriverUpdatedDstSet = VK_NULL_HANDLE;
    deviceVk.vkUpdateDescriptorSets = [](VkDevice, uint32_t writeCount,
                                         const VkWriteDescriptorSet* pWrites, uint32_t,
                                         const VkCopyDescriptorSet*) {
        if (writeCount > 0 && pWrites != nullptr) {
            sLastDriverUpdatedDstSet = pWrites[0].dstSet;
        }
    };
    deviceVk.vkDestroyDescriptorPool = [](VkDevice, VkDescriptorPool,
                                          const VkAllocationCallbacks*) {};

    VkDevice unboxedDevice = reinterpret_cast<VkDevice>(0xe301'0000ULL);
    Boxed<VkDevice> boxedDevice = new_boxed_VkDevice(unboxedDevice, &deviceVk);
    VkDescriptorPool unboxedPool = reinterpret_cast<VkDescriptorPool>(0xe302'0000ULL);
    Boxed<VkDescriptorPool> boxedPool = new_boxed_non_dispatchable_VkDescriptorPool(unboxedPool);
    Boxed<VkDescriptorSet> preallocSlot =
        new_boxed_non_dispatchable_VkDescriptorSet(VK_NULL_HANDLE);
    uint64_t preallocId = reinterpret_cast<uint64_t>(preallocSlot.value());
    peer.registerPreallocatedPoolWithState(unboxedDevice, unboxedPool, boxedPool, /*maxSets=*/4,
                                           VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                           /*descriptorCount=*/500000, {preallocId});

    VkDescriptorSetLayoutBinding fixedBinding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .descriptorCount = 4,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };
    VkDescriptorSetLayoutCreateInfo fixedLayoutCi = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &fixedBinding,
    };
    Boxed<VkDescriptorSetLayout> boxedFixedLayout;
    ASSERT_EQ(VK_SUCCESS,
              peer.createDescriptorSetLayout(boxedDevice, &fixedLayoutCi, &boxedFixedLayout));
    VkDescriptorSetLayout unboxedFixedLayout = unbox_VkDescriptorSetLayout(boxedFixedLayout);

    // Re-realizing the same poolId releases previous pool accounting instead of double-charging.
    bool didAlloc = false;
    ASSERT_NE(VK_NULL_HANDLE,
              peer.getOrAllocateDescriptorSetFromPoolAndId(&deviceVk, unboxedDevice, unboxedPool,
                                                           unboxedFixedLayout, preallocId,
                                                           /*pendingAlloc=*/1, &didAlloc));
    EXPECT_EQ(1u, peer.descriptorPoolUsedSets(unboxedPool));
    EXPECT_EQ(4u, peer.descriptorPoolUsedCount(unboxedPool));

    ASSERT_NE(VK_NULL_HANDLE,
              peer.getOrAllocateDescriptorSetFromPoolAndId(&deviceVk, unboxedDevice, unboxedPool,
                                                           unboxedFixedLayout, preallocId,
                                                           /*pendingAlloc=*/1, &didAlloc));
    EXPECT_EQ(1u, peer.descriptorPoolUsedSets(unboxedPool));
    EXPECT_EQ(4u, peer.descriptorPoolUsedCount(unboxedPool));

    // Sparse high-index write and copy keep per-binding storage O(written descriptors).
    VkDescriptorSetLayoutBinding bindlessBinding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .descriptorCount = 200000,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };
    VkDescriptorBindingFlags varFlag = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
    VkDescriptorSetLayoutBindingFlagsCreateInfo flagsCi = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = 1,
        .pBindingFlags = &varFlag,
    };
    VkDescriptorSetLayoutCreateInfo bindlessLayoutCi = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &flagsCi,
        .bindingCount = 1,
        .pBindings = &bindlessBinding,
    };
    Boxed<VkDescriptorSetLayout> boxedBindlessLayout;
    ASSERT_EQ(VK_SUCCESS,
              peer.createDescriptorSetLayout(boxedDevice, &bindlessLayoutCi, &boxedBindlessLayout));
    VkDescriptorSetLayout unboxedBindlessLayout = unbox_VkDescriptorSetLayout(boxedBindlessLayout);

    uint32_t varCounts[2] = {200000, 200000};
    VkDescriptorSetVariableDescriptorCountAllocateInfo varAi = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .descriptorSetCount = 2,
        .pDescriptorCounts = varCounts,
    };
    VkDescriptorSetLayout layouts[2] = {unboxedBindlessLayout, unboxedBindlessLayout};
    VkDescriptorSetAllocateInfo allocCi = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = &varAi,
        .descriptorPool = unboxedPool,
        .descriptorSetCount = 2,
        .pSetLayouts = layouts,
    };
    VkDescriptorSet boxedSets[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
    ASSERT_EQ(VK_SUCCESS, peer.allocateDescriptorSets(boxedDevice, &allocCi, boxedSets));
    VkDescriptorSet srcSet = unbox_VkDescriptorSet(Boxed<VkDescriptorSet>(boxedSets[0]));
    VkDescriptorSet dstSet = unbox_VkDescriptorSet(Boxed<VkDescriptorSet>(boxedSets[1]));

    VkImageView fakeView = reinterpret_cast<VkImageView>(0xe399'0000ULL);
    VkDescriptorImageInfo imgInfo = {
        .imageView = fakeView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = srcSet,
        .dstBinding = 0,
        .dstArrayElement = 123456,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .pImageInfo = &imgInfo,
    };
    VkCopyDescriptorSet copy = {
        .sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET,
        .srcSet = srcSet,
        .srcBinding = 0,
        .srcArrayElement = 123456,
        .dstSet = dstSet,
        .dstBinding = 0,
        .dstArrayElement = 199999,
        .descriptorCount = 1,
    };
    peer.updateDescriptorSets(&bumpPool, boxedDevice, 1, &write, 1, &copy);

    EXPECT_EQ(1u, peer.sparseBindingWriteCount(srcSet, 0));
    EXPECT_EQ(1u, peer.sparseBindingWriteCount(dstSet, 0));
    EXPECT_TRUE(peer.sparseBindingHasImageView(dstSet, 0, 199999, fakeView));

    // Snapshot load replays writes with boxed dstSet and pendingAlloc = 0; verify
    // needToUnboxDescriptorSet unboxes dstSet to the underlying host VkDescriptorSet.
    uint64_t boxedDstId = reinterpret_cast<uint64_t>(boxedSets[1]);
    uint32_t whichPool = 0;
    uint32_t pendingAlloc = 0;
    uint32_t writeStartIdx = 0;
    VkWriteDescriptorSet snapshotWrite = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = boxedSets[1],
        .dstBinding = 0,
        .dstArrayElement = 77777,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .pImageInfo = &imgInfo,
    };
    peer.commitDescriptorSetUpdates(&bumpPool, &deviceVk, unboxedDevice, 1, &unboxedPool, 1,
                                    &unboxedBindlessLayout, &boxedDstId, &whichPool, &pendingAlloc,
                                    &writeStartIdx, 1, &snapshotWrite,
                                    /*needToUnboxDescriptorSet=*/true);
    EXPECT_EQ(dstSet, sLastDriverUpdatedDstSet);
    EXPECT_TRUE(peer.sparseBindingHasImageView(dstSet, 0, 77777, fakeView));

    peer.destroyDescriptorPool(boxedDevice, unboxedPool);
    delete_VkDescriptorPool(boxedPool);
    peer.destroyDescriptorSetLayout(boxedDevice, unboxedFixedLayout);
    delete_VkDescriptorSetLayout(boxedFixedLayout);
    peer.destroyDescriptorSetLayout(boxedDevice, unboxedBindlessLayout);
    delete_VkDescriptorSetLayout(boxedBindlessLayout);
    delete_VkDevice(boxedDevice);
}

TEST(VkDecoderGlobalStateBindlessDescriptorTest,
     BatchedPoolCreateUnpatchedFallbackAndInlineUniformBlock) {
    const size_t baselineHandles = sBoxedHandleManager.getHandlesCount();
    VkDecoderGlobalStateTestPeer peer;
    peer.setBatchedDescriptorSetUpdateEnabled(true);
    gfxstream::base::BumpPool bumpPool;

    static uint64_t sNextHandle = 0xe400'0000ULL;
    static bool sFailNextAlloc = false;
    static uint32_t sLastSeenDriverVarCount = 0;
    sFailNextAlloc = false;
    sLastSeenDriverVarCount = 0;

    VulkanDispatch deviceVk = {};
    deviceVk.vkCreateDescriptorPool = [](VkDevice, const VkDescriptorPoolCreateInfo*,
                                         const VkAllocationCallbacks*,
                                         VkDescriptorPool* pPool) -> VkResult {
        *pPool = reinterpret_cast<VkDescriptorPool>(++sNextHandle);
        return VK_SUCCESS;
    };
    deviceVk.vkDestroyDescriptorPool = [](VkDevice, VkDescriptorPool,
                                          const VkAllocationCallbacks*) {};
    deviceVk.vkCreateDescriptorSetLayout = [](VkDevice, const VkDescriptorSetLayoutCreateInfo*,
                                              const VkAllocationCallbacks*,
                                              VkDescriptorSetLayout* pSetLayout) -> VkResult {
        *pSetLayout = reinterpret_cast<VkDescriptorSetLayout>(++sNextHandle);
        return VK_SUCCESS;
    };
    deviceVk.vkDestroyDescriptorSetLayout = [](VkDevice, VkDescriptorSetLayout,
                                               const VkAllocationCallbacks*) {};
    deviceVk.vkAllocateDescriptorSets = [](VkDevice, const VkDescriptorSetAllocateInfo* ci,
                                           VkDescriptorSet* pSets) -> VkResult {
        if (sFailNextAlloc) {
            return VK_ERROR_OUT_OF_POOL_MEMORY;
        }
        const auto* varInfo =
            vk_find_struct<VkDescriptorSetVariableDescriptorCountAllocateInfo>(ci);
        sLastSeenDriverVarCount =
            (varInfo && varInfo->descriptorSetCount > 0 && varInfo->pDescriptorCounts)
                ? varInfo->pDescriptorCounts[0]
                : 0xffffffffu;
        for (uint32_t i = 0; i < ci->descriptorSetCount; ++i) {
            pSets[i] = reinterpret_cast<VkDescriptorSet>(++sNextHandle);
        }
        return VK_SUCCESS;
    };
    deviceVk.vkFreeDescriptorSets = [](VkDevice, VkDescriptorPool, uint32_t,
                                       const VkDescriptorSet*) -> VkResult { return VK_SUCCESS; };
    deviceVk.vkUpdateDescriptorSets = [](VkDevice, uint32_t, const VkWriteDescriptorSet*, uint32_t,
                                         const VkCopyDescriptorSet*) {};

    VkDevice unboxedDevice = reinterpret_cast<VkDevice>(0xe401'0000ULL);
    Boxed<VkDevice> boxedDevice = new_boxed_VkDevice(unboxedDevice, &deviceVk);

    // 1. on_vkCreateDescriptorPool uses mBatchedDescriptorSetUpdateEnabled without requiring
    // m_vkEmulation and mints preallocatedBoxedSets.
    VkDescriptorPoolSize poolSizes[2] = {
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 16},
        {VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK, 64},
    };
    VkDescriptorPoolCreateInfo poolCi = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 3,
        .poolSizeCount = 2,
        .pPoolSizes = poolSizes,
    };
    Boxed<VkDescriptorPool> boxedPool;
    ASSERT_EQ(VK_SUCCESS, peer.createDescriptorPool(boxedDevice, &poolCi, &boxedPool));
    VkDescriptorPool unboxedPool = unbox_VkDescriptorPool(boxedPool);

    uint32_t poolIdCount = 0;
    peer.collectDescriptorPoolIds(&bumpPool, boxedDevice.value(), unboxedPool, &poolIdCount,
                                  nullptr);
    ASSERT_EQ(3u, poolIdCount);
    std::vector<uint64_t> poolIds(poolIdCount, 0);
    peer.collectDescriptorPoolIds(&bumpPool, boxedDevice.value(), unboxedPool, &poolIdCount,
                                  poolIds.data());

    // 2. Unpatched batched guest allocating a variable-count layout via poolId gets a clamped
    // VkDescriptorSetVariableDescriptorCountAllocateInfo accounting for fixed same-type bindings.
    VkDescriptorSetLayoutBinding mixedBindings[2] = {
        {0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 100, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
    };
    VkDescriptorBindingFlags mixedFlags[2] = {
        0,
        VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT,
    };
    VkDescriptorSetLayoutBindingFlagsCreateInfo mixedFlagsCi = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = 2,
        .pBindingFlags = mixedFlags,
    };
    VkDescriptorSetLayoutCreateInfo mixedLayoutCi = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &mixedFlagsCi,
        .bindingCount = 2,
        .pBindings = mixedBindings,
    };
    Boxed<VkDescriptorSetLayout> boxedMixedLayout;
    ASSERT_EQ(VK_SUCCESS,
              peer.createDescriptorSetLayout(boxedDevice, &mixedLayoutCi, &boxedMixedLayout));
    VkDescriptorSetLayout unboxedMixedLayout = unbox_VkDescriptorSetLayout(boxedMixedLayout);

    // 3. Driver allocation failure does not corrupt pool accounting or mDescriptorSetInfo.
    sFailNextAlloc = true;
    bool didAlloc = true;
    EXPECT_EQ(VK_NULL_HANDLE,
              peer.getOrAllocateDescriptorSetFromPoolAndId(&deviceVk, unboxedDevice, unboxedPool,
                                                           unboxedMixedLayout, poolIds[0],
                                                           /*pendingAlloc=*/1, &didAlloc));
    EXPECT_FALSE(didAlloc);
    EXPECT_EQ(0u, peer.descriptorPoolUsedSets(unboxedPool));
    EXPECT_EQ(0u, peer.descriptorPoolUsedCount(unboxedPool));

    sFailNextAlloc = false;
    VkDescriptorSet fallbackSet = peer.getOrAllocateDescriptorSetFromPoolAndId(
        &deviceVk, unboxedDevice, unboxedPool, unboxedMixedLayout, poolIds[0],
        /*pendingAlloc=*/1, &didAlloc);
    ASSERT_NE(VK_NULL_HANDLE, fallbackSet);
    EXPECT_TRUE(didAlloc);
    EXPECT_EQ(12u, sLastSeenDriverVarCount);
    EXPECT_EQ(16u, peer.descriptorPoolUsedCount(unboxedPool, 0));

    // 4. Zero-count InlineUniformBlock write does not wrap into a higher binding or abort the
    // remaining writes in the batch, and valid InlineUniformBlock copies do not wrap either.
    VkDescriptorSetLayoutBinding iubBindings[2] = {
        {0, VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK, 0, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 2, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
    };
    VkDescriptorSetLayoutCreateInfo iubLayoutCi = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings = iubBindings,
    };
    Boxed<VkDescriptorSetLayout> boxedIubLayout;
    ASSERT_EQ(VK_SUCCESS,
              peer.createDescriptorSetLayout(boxedDevice, &iubLayoutCi, &boxedIubLayout));
    VkDescriptorSetLayout unboxedIubLayout = unbox_VkDescriptorSetLayout(boxedIubLayout);

    ASSERT_EQ(VK_SUCCESS, peer.freeDescriptorSets(boxedDevice, unboxedPool, 1, &fallbackSet));
    VkDescriptorSet iubSet = peer.getOrAllocateDescriptorSetFromPoolAndId(
        &deviceVk, unboxedDevice, unboxedPool, unboxedIubLayout, poolIds[0], /*pendingAlloc=*/1,
        &didAlloc);
    ASSERT_NE(VK_NULL_HANDLE, iubSet);

    uint8_t iubBytes[4] = {1, 2, 3, 4};
    VkWriteDescriptorSetInlineUniformBlock iubWriteNext = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK,
        .dataSize = 4,
        .pData = iubBytes,
    };
    VkImageView expectedImg = reinterpret_cast<VkImageView>(0xe499'0001ULL);
    VkDescriptorImageInfo imgInfo = {
        .imageView = expectedImg,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkWriteDescriptorSet batchWrites[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = &iubWriteNext,
            .dstSet = iubSet,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 4,
            .descriptorType = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = iubSet,
            .dstBinding = 1,
            .dstArrayElement = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .pImageInfo = &imgInfo,
        },
    };
    peer.updateDescriptorSets(&bumpPool, boxedDevice, 2, batchWrites, 0, nullptr);
    EXPECT_EQ(0u, peer.sparseBindingWriteCount(iubSet, 0));
    EXPECT_EQ(1u, peer.sparseBindingWriteCount(iubSet, 1));
    EXPECT_TRUE(peer.sparseBindingHasImageView(iubSet, 1, 1, expectedImg));

    peer.destroyDescriptorPool(boxedDevice, unboxedPool);
    delete_VkDescriptorPool(boxedPool);
    peer.destroyDescriptorSetLayout(boxedDevice, unboxedMixedLayout);
    delete_VkDescriptorSetLayout(boxedMixedLayout);
    peer.destroyDescriptorSetLayout(boxedDevice, unboxedIubLayout);
    delete_VkDescriptorSetLayout(boxedIubLayout);
    delete_VkDevice(boxedDevice);
    EXPECT_EQ(baselineHandles, sBoxedHandleManager.getHandlesCount());
}

}  // namespace
}  // namespace vk
}  // namespace host
}  // namespace gfxstream
