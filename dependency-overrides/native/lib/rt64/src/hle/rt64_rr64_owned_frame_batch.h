// Completed, privately owned presentation images for one authored framebuffer.
#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <vector>

#include "render/rt64_render_target.h"
#include "rt64_rr64_frame_metadata.h"

namespace RT64::RR64FramePacing {
    struct OwnedFrameBatch {
        // workloadId is the selected target's authored writer, not the most
        // recent workload needed by the presentation queue's readiness wait.
        InterpolationBatchMetadata metadata;
        uint64_t resourceEpoch = 0;
        // Diagnostics for the actual interpolation endpoints, independent of
        // the latest queue watermark. Native pictures have no previous sample.
        uint64_t previousWriterWorkloadId = 0;
        uint64_t previousAuthoredTimestampNs = 0;
        std::vector<std::unique_ptr<RenderTarget>> images;
    };

    class OwnedFrameBatchCache {
    public:
        using Lease = std::shared_ptr<const OwnedFrameBatch>;
        explicit OwnedFrameBatchCache(bool enableReuse = true) : reuseEnabled(enableReuse) { }

        static constexpr std::size_t MaximumBatches = 8;
        static constexpr std::size_t MaximumVersionsPerTarget = 3;
        static constexpr uint64_t MaximumBatchBytes = 64ull * 1024ull * 1024ull;
        static constexpr uint64_t MaximumCacheBytes = 256ull * 1024ull * 1024ull;
        static constexpr uint64_t MaximumRecycledBytes = 64ull * 1024ull * 1024ull;
        static constexpr std::size_t MaximumRecycledImages = 8;

        // Only natural cache eviction can donate images. A present lease keeps
        // use_count above one through its GPU wait, so that batch is never
        // writable here. The exact new source dimensions are known at copy time.
        std::unique_ptr<RenderTarget> takeRecycledImage(SceneSnapshot scene,
            uint64_t resourceEpoch, PresentationTargetIdentity target,
            uint32_t width, uint32_t height, bool usesHDR)
        {
            std::vector<std::unique_ptr<RenderTarget>> discarded;
            std::unique_ptr<RenderTarget> result;
            {
                std::scoped_lock lock(mutex);
                for (std::size_t i = 0; i < recycled.size();) {
                    auto &entry = recycled[i];
                    const bool sameAddress = entry.target.address == target.address;
                    const bool compatible = entry.target == target &&
                        entry.image->width == width && entry.image->height == height &&
                        entry.image->usesHDR == usesHDR &&
                        entry.image->format == RenderTarget::colorBufferFormat(usesHDR);
                    const bool stale = !(entry.scene == scene) || entry.resourceEpoch != resourceEpoch ||
                        (sameAddress && !compatible);
                    if (stale || (!result && compatible)) {
                        recycledBytes -= entry.bytes;
                        if (stale) { discarded.emplace_back(std::move(entry.image)); }
                        else { result = std::move(entry.image); }
                        recycled.erase(recycled.begin() + i);
                    }
                    else { ++i; }
                }
                if (result) { ++reuseHits; }
                else { ++reuseMisses; }
            }
            return result;
        }

        struct ReuseStats { uint64_t hits, misses, bytes; std::size_t images; };
        ReuseStats reuseStats() const {
            std::scoped_lock lock(mutex);
            return {reuseHits, reuseMisses, recycledBytes, recycled.size()};
        }

        // Destinations must be resolved, non-MSAA color textures. Call before
        // allocating the images; publication repeats the size check. This
        // bounds pixel storage, excluding small API objects and descriptors.
        static constexpr uint64_t estimateImageBytes(uint32_t width,
            uint32_t height, bool usesHDR)
        {
            const uint64_t pixels = uint64_t(width) * uint64_t(height);
            const uint64_t bytesPerPixel = usesHDR ? 8u : 4u;
            return (pixels > std::numeric_limits<uint64_t>::max() / bytesPerPixel) ?
                std::numeric_limits<uint64_t>::max() : pixels * bytesPerPixel;
        }

        static constexpr bool canAllocateImages(uint32_t width, uint32_t height,
            bool usesHDR, std::size_t imageCount)
        {
            return (width != 0) && (height != 0) && (imageCount >= 1) &&
                (imageCount <= MaximumCadenceFrames) &&
                (estimateImageBytes(width, height, usesHDR) <=
                    MaximumBatchBytes / imageCount);
        }

        // The producer must finish and wait for every copy before this call.
        // Transfer exclusive ownership and retain no writable aliases. The
        // cache does not submit work, wait on the GPU, or infer fence readiness.
        bool publishAfterGpuWait(std::unique_ptr<OwnedFrameBatch> batch) {
            uint64_t batchBytes = 0;
            if (!validBatch(batch.get(), batchBytes)) {
                return false;
            }

            std::shared_ptr<OwnedFrameBatch> published(std::move(batch));
            std::vector<Lease> retired;
            {
                std::scoped_lock lock(mutex);
                const auto &incoming = published->metadata;
                for (std::size_t index = 0; index < entries.size();) {
                    const auto &existing = entries[index].batch;
                    const auto &metadata = existing->metadata;
                    const bool sameAddress = metadata.target.address == incoming.target.address;
                    const bool incompatibleAddress = sameAddress &&
                        (!(metadata.target == incoming.target) ||
                            !(metadata.scene == incoming.scene) ||
                            (existing->resourceEpoch != published->resourceEpoch));
                    const bool replacement = sameAddress &&
                        (metadata.workloadId == incoming.workloadId);
                    if (incompatibleAddress || replacement) {
                        retireAt(index, retired);
                    }
                    else {
                        index++;
                    }
                }

                std::size_t targetVersions = 0;
                for (const auto &entry : entries) {
                    targetVersions += entry.batch->metadata.target == incoming.target;
                }
                while (targetVersions >= MaximumVersionsPerTarget) {
                    for (std::size_t index = 0; index < entries.size(); index++) {
                        if (entries[index].batch->metadata.target == incoming.target) {
                            retireAt(index, retired, true);
                            targetVersions--;
                            break;
                        }
                    }
                }

                while (!entries.empty() && ((entries.size() >= MaximumBatches) ||
                    (cachedBytes > MaximumCacheBytes - batchBytes)))
                {
                    retireAt(0, retired, true);
                }
                entries.push_back({std::move(published), batchBytes});
                cachedBytes += batchBytes;
            }
            // Destroy evicted resources outside the cache lock. A presenter
            // holding a lease keeps them alive until its GPU use has completed.
            return true;
        }

        Lease find(const InterpolationBatchMetadata &requested,
            uint64_t resourceEpoch) const
        {
            if (resourceEpoch == 0) {
                return {};
            }
            std::scoped_lock lock(mutex);
            for (auto entry = entries.rbegin(); entry != entries.rend(); ++entry) {
                const auto &batch = entry->batch;
                if ((batch->resourceEpoch == resourceEpoch) &&
                    batch->metadata.matches(requested.workloadId, requested.scene,
                        requested.target, requested.sourceRate, requested.targetRate))
                {
                    return batch;
                }
            }
            return {};
        }

        void eraseAddress(uint32_t address) {
            std::vector<Lease> retired;
            std::vector<std::unique_ptr<RenderTarget>> discarded;
            {
                std::scoped_lock lock(mutex);
                for (std::size_t index = 0; index < entries.size();) {
                    if (entries[index].batch->metadata.target.address == address) {
                        retireAt(index, retired);
                    }
                    else {
                        index++;
                    }
                }
                for (std::size_t i = 0; i < recycled.size();) {
                    if (recycled[i].target.address == address) {
                        recycledBytes -= recycled[i].bytes;
                        discarded.emplace_back(std::move(recycled[i].image));
                        recycled.erase(recycled.begin() + i);
                    }
                    else { ++i; }
                }
            }
        }

        void clear() {
            std::vector<Entry> retired;
            std::vector<RecycledImage> discarded;
            {
                std::scoped_lock lock(mutex);
                retired.swap(entries);
                cachedBytes = 0;
                discarded.swap(recycled);
                recycledBytes = 0;
            }
        }

        std::size_t size() const {
            std::scoped_lock lock(mutex);
            return entries.size();
        }

        uint64_t estimatedBytes() const {
            std::scoped_lock lock(mutex);
            return cachedBytes + recycledBytes;
        }

    private:
        struct Entry {
            std::shared_ptr<OwnedFrameBatch> batch;
            uint64_t bytes = 0;
        };
        struct RecycledImage {
            SceneSnapshot scene;
            uint64_t resourceEpoch;
            PresentationTargetIdentity target;
            uint64_t bytes;
            std::unique_ptr<RenderTarget> image;
        };

        static bool validBatch(const OwnedFrameBatch *batch, uint64_t &bytes) {
            if ((batch == nullptr) || (batch->resourceEpoch == 0) ||
                (batch->metadata.workloadId == 0) ||
                (batch->metadata.scene.epoch == 0) || !batch->metadata.scene.raceActive ||
                !batch->metadata.target.valid() || (batch->metadata.targetRate == 0) ||
                batch->images.empty() || (batch->images.size() > MaximumCadenceFrames))
            {
                return false;
            }
            // A native image is valid at equal/lower display rates and during
            // timing warm-up. Multiple images require a proved integral rate.
            if ((batch->images.size() > 1) &&
                ((batch->metadata.sourceRate == 0) ||
                    (batch->metadata.targetRate <= batch->metadata.sourceRate) ||
                    ((batch->metadata.targetRate % batch->metadata.sourceRate) != 0) ||
                    (batch->images.size() != batch->metadata.targetRate / batch->metadata.sourceRate))) {
                return false;
            }

            const RenderTarget *first = batch->images.front().get();
            if (first == nullptr) {
                return false;
            }
            for (const auto &image : batch->images) {
                if ((image == nullptr) || (image->texture == nullptr) ||
                    (image->width == 0) || (image->height == 0) ||
                    (image->type != Framebuffer::Type::Color) ||
                    (image->multisampling.sampleCount > 1) ||
                    (image->resolvedTexture != nullptr) ||
                    (image->width != first->width) || (image->height != first->height) ||
                    (image->usesHDR != first->usesHDR) || (image->format != first->format))
                {
                    return false;
                }
                const uint64_t imageBytes = estimateImageBytes(image->width,
                    image->height, image->usesHDR);
                if (imageBytes > MaximumBatchBytes - bytes) {
                    return false;
                }
                bytes += imageBytes;
            }
            return true;
        }

        void retireAt(std::size_t index, std::vector<Lease> &retired, bool allowReuse = false) {
            auto &batch = entries[index].batch;
            if (reuseEnabled && allowReuse && batch.use_count() == 1) {
                for (auto &image : batch->images) {
                    const uint64_t bytes = estimateImageBytes(image->width, image->height, image->usesHDR);
                    if (recycled.size() < MaximumRecycledImages && bytes <= MaximumRecycledBytes - recycledBytes) {
                        recycled.push_back({batch->metadata.scene, batch->resourceEpoch,
                            batch->metadata.target, bytes, std::move(image)});
                        recycledBytes += bytes;
                    }
                }
            }
            cachedBytes -= entries[index].bytes;
            retired.emplace_back(std::move(entries[index].batch));
            entries.erase(entries.begin() + index);
        }

        mutable std::mutex mutex;
        std::vector<Entry> entries;
        uint64_t cachedBytes = 0;
        std::vector<RecycledImage> recycled;
        uint64_t recycledBytes = 0;
        uint64_t reuseHits = 0, reuseMisses = 0;
        const bool reuseEnabled;
    };
}
