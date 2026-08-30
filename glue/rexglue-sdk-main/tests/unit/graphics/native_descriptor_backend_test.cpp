#include <catch2/catch_test_macros.hpp>

#include "graphics/gta4_native/native_descriptor_backend.h"

namespace rex::graphics::gta4_native {
namespace {

NativeDescriptorPolicyInputs FullySupportedPolicy() {
  NativeDescriptorPolicyInputs inputs{};
  inputs.request = NativeDescriptorBackendRequest::kAuto;
  inputs.indexing_features = {true, true, true, true};
  inputs.limits.max_per_stage_sampled_images = 16384;
  inputs.limits.max_descriptor_set_sampled_images = 16384;
  inputs.limits.max_per_stage_samplers = 16384;
  inputs.limits.max_descriptor_set_samplers = 16384;
  inputs.limits.max_per_stage_resources = 32768;
  inputs.limits.max_descriptors_in_all_pools = 65536;
  inputs.layout_support = {true, true, 16384, 16384};
  inputs.required_sampled_images = 8192;
  inputs.required_samplers = 2048;
  inputs.required_per_stage_resources = 10240;
  inputs.required_pool_descriptors = 20480;
  inputs.use_update_after_bind_layout = true;
  return inputs;
}

NativeDescriptorEpochConfig TwoCopySingleSlotConfig() {
  return {2, 1, NativeDescriptorKind::kSampledImage, {1, 0}};
}

NativeDescriptorCapacityInputs CompactCapacityInputs() {
  NativeDescriptorCapacityInputs inputs{};
  inputs.limits = {262144, 262144, 2048, 2048, 300000, 600000};
  inputs.desired_sampled_images_per_set = 16384;
  inputs.desired_samplers = 1024;
  inputs.minimum_sampled_images_per_set = 4096;
  inputs.minimum_samplers = 256;
  inputs.sampled_image_set_count = 4;
  inputs.frame_copy_count = 2;
  inputs.storage_descriptors_per_copy = 1;
  return inputs;
}

void CommitAllWrites(NativeDescriptorEpochTable& table, uint32_t frame_copy) {
  const NativeDescriptorWriteBatch batch = table.BeginFrameCopyWrites(frame_copy);
  REQUIRE(batch);
  REQUIRE(table.CommitFrameCopyWrites(batch) == NativeDescriptorStatus::kSuccess);
}

}  // namespace

TEST_CASE("native descriptor capacity uses the compact desired table shape",
          "[gta4-native][descriptor]") {
  const NativeDescriptorCapacity capacity = ChooseNativeDescriptorCapacity(CompactCapacityInputs());
  REQUIRE(capacity);
  CHECK(capacity.sampled_images_per_set == 16384);
  CHECK(capacity.samplers == 1024);
  CHECK(capacity.sampled_images_per_stage == 65536);
  CHECK(capacity.resources_per_stage == 66561);
  CHECK(capacity.pool_sampled_images == 131072);
  CHECK(capacity.pool_samplers == 2048);
  CHECK(capacity.pool_storage_buffers == 2);
  CHECK(capacity.pool_descriptors == 133122);
}

TEST_CASE("native descriptor capacity clamps across combined device limits",
          "[gta4-native][descriptor]") {
  NativeDescriptorCapacityInputs inputs = CompactCapacityInputs();
  inputs.limits = {48000, 40000, 700, 600, 39000, 80000};
  const NativeDescriptorCapacity capacity = ChooseNativeDescriptorCapacity(inputs);
  REQUIRE(capacity);
  CHECK(capacity.sampled_images_per_set == 9685);
  CHECK(capacity.samplers == 259);
  CHECK(capacity.sampled_images_per_stage == 38740);
  CHECK(capacity.resources_per_stage == 39000);
  CHECK(capacity.pool_descriptors == 78000);
}

TEST_CASE("native descriptor capacity rejects unsafe floors and malformed shapes",
          "[gta4-native][descriptor]") {
  NativeDescriptorCapacityInputs inputs = CompactCapacityInputs();
  inputs.limits.max_per_stage_resources = 1000;
  CHECK(ChooseNativeDescriptorCapacity(inputs).status ==
        NativeDescriptorStatus::kInsufficientDeviceLimits);

  inputs = CompactCapacityInputs();
  inputs.frame_copy_count = 0;
  CHECK(ChooseNativeDescriptorCapacity(inputs).status ==
        NativeDescriptorStatus::kInvalidConfiguration);
}

TEST_CASE("native descriptor auto policy selects indexed only after full capability proof",
          "[gta4-native][descriptor]") {
  NativeDescriptorPolicyInputs inputs = FullySupportedPolicy();
  NativeDescriptorPolicyDecision decision = ChooseNativeDescriptorBackend(inputs);
  REQUIRE(decision);
  CHECK(decision.backend == NativeDescriptorBackend::kIndexed);
  CHECK(decision.indexed_rejection == NativeDescriptorStatus::kSuccess);

  inputs.indexing_features.descriptor_binding_partially_bound = false;
  decision = ChooseNativeDescriptorBackend(inputs);
  REQUIRE(decision);
  CHECK(decision.backend == NativeDescriptorBackend::kCached);
  CHECK(decision.indexed_rejection == NativeDescriptorStatus::kMissingIndexingFeatures);

  inputs = FullySupportedPolicy();
  inputs.limits.max_descriptor_set_sampled_images = 4096;
  decision = ChooseNativeDescriptorBackend(inputs);
  REQUIRE(decision);
  CHECK(decision.backend == NativeDescriptorBackend::kCached);
  CHECK(decision.indexed_rejection == NativeDescriptorStatus::kInsufficientDeviceLimits);
}

TEST_CASE("native descriptor indexed policy requires an exact layout support query",
          "[gta4-native][descriptor]") {
  NativeDescriptorPolicyInputs inputs = FullySupportedPolicy();
  inputs.request = NativeDescriptorBackendRequest::kIndexed;
  inputs.layout_support.queried = false;
  NativeDescriptorPolicyDecision decision = ChooseNativeDescriptorBackend(inputs);
  CHECK_FALSE(decision);
  CHECK(decision.status == NativeDescriptorStatus::kLayoutSupportNotQueried);

  inputs.layout_support.queried = true;
  inputs.layout_support.supported = false;
  decision = ChooseNativeDescriptorBackend(inputs);
  CHECK_FALSE(decision);
  CHECK(decision.status == NativeDescriptorStatus::kLayoutUnsupported);

  inputs.layout_support.supported = true;
  inputs.layout_support.supported_samplers = 1024;
  decision = ChooseNativeDescriptorBackend(inputs);
  CHECK_FALSE(decision);
  CHECK(decision.status == NativeDescriptorStatus::kLayoutUnsupported);
}

TEST_CASE("native descriptor cached policy is the capability-independent fallback",
          "[gta4-native][descriptor]") {
  NativeDescriptorPolicyInputs inputs{};
  inputs.request = NativeDescriptorBackendRequest::kCached;
  inputs.required_sampled_images = 1;
  inputs.required_samplers = 1;
  inputs.required_per_stage_resources = 2;
  inputs.required_pool_descriptors = 2;
  const NativeDescriptorPolicyDecision decision = ChooseNativeDescriptorBackend(inputs);
  REQUIRE(decision);
  CHECK(decision.backend == NativeDescriptorBackend::kCached);
}

TEST_CASE("native descriptor policy requires update-after-bind layout features when selected",
          "[gta4-native][descriptor]") {
  NativeDescriptorPolicyInputs inputs = FullySupportedPolicy();
  inputs.indexing_features.descriptor_binding_sampler_update_after_bind = false;
  CHECK(ChooseNativeDescriptorBackend(inputs).backend == NativeDescriptorBackend::kCached);

  inputs.request = NativeDescriptorBackendRequest::kIndexed;
  CHECK(ChooseNativeDescriptorBackend(inputs).status ==
        NativeDescriptorStatus::kMissingUpdateAfterBindFeatures);

  inputs = FullySupportedPolicy();
  inputs.required_sampled_images = 0;
  CHECK(ChooseNativeDescriptorBackend(inputs).status ==
        NativeDescriptorStatus::kInvalidConfiguration);
}

TEST_CASE("native descriptor epoch table requires valid non-null fallback descriptors",
          "[gta4-native][descriptor]") {
  NativeDescriptorEpochTable missing_frames({0, 1, NativeDescriptorKind::kSampledImage, {1, 0}});
  CHECK_FALSE(missing_frames.valid());
  CHECK(missing_frames.initialization_status() == NativeDescriptorStatus::kInvalidConfiguration);

  NativeDescriptorEpochTable null_image({2, 1, NativeDescriptorKind::kSampledImage, {0, 0}});
  CHECK_FALSE(null_image.valid());
  CHECK(null_image.initialization_status() == NativeDescriptorStatus::kInvalidDescriptor);

  NativeDescriptorEpochTable null_sampler({2, 1, NativeDescriptorKind::kSampler, {0, 0}});
  CHECK_FALSE(null_sampler.valid());
  CHECK(null_sampler.initialization_status() == NativeDescriptorStatus::kInvalidDescriptor);
}

TEST_CASE("native descriptor allocation publishes valid payloads to every frame copy",
          "[gta4-native][descriptor]") {
  NativeDescriptorEpochTable table(TwoCopySingleSlotConfig());
  REQUIRE(table.valid());
  const NativeDescriptorAllocation allocation = table.Allocate({10, 0});
  REQUIRE(allocation);
  CHECK(table.live_count() == 1);
  CHECK(table.free_count() == 0);

  for (uint32_t frame_copy : {0u, 1u}) {
    const NativeDescriptorWriteBatch batch = table.BeginFrameCopyWrites(frame_copy);
    REQUIRE(batch);
    REQUIRE(batch.writes.size() == 1);
    CHECK(batch.writes.front().slot == allocation.handle.index);
    CHECK(batch.writes.front().payload == NativeDescriptorPayload{10, 0});
    CHECK(batch.writes.front().payload.valid(table.kind()));
    CHECK_FALSE(batch.writes.front().tombstone);
    REQUIRE(table.CommitFrameCopyWrites(batch) == NativeDescriptorStatus::kSuccess);

    const NativeDescriptorWriteBatch empty = table.BeginFrameCopyWrites(frame_copy);
    REQUIRE(empty);
    CHECK(empty.writes.empty());
    REQUIRE(table.CommitFrameCopyWrites(empty) == NativeDescriptorStatus::kSuccess);
  }
}

TEST_CASE("native descriptor image and sampler tables accept independent payloads",
          "[gta4-native][descriptor]") {
  NativeDescriptorEpochTable image_table({2, 1, NativeDescriptorKind::kSampledImage, {1, 0}});
  REQUIRE(image_table.valid());
  REQUIRE(image_table.Allocate({10, 0}));
  CHECK(image_table.Allocate({0, 20}).status == NativeDescriptorStatus::kInvalidDescriptor);

  NativeDescriptorEpochTable sampler_table({2, 1, NativeDescriptorKind::kSampler, {0, 2}});
  REQUIRE(sampler_table.valid());
  const NativeDescriptorAllocation sampler = sampler_table.Allocate({0, 20});
  REQUIRE(sampler);
  const NativeDescriptorWriteBatch batch = sampler_table.BeginFrameCopyWrites(0);
  REQUIRE(batch);
  REQUIRE(batch.writes.size() == 1);
  CHECK(batch.writes.front().payload == NativeDescriptorPayload{0, 20});
  CHECK(batch.writes.front().payload.valid(NativeDescriptorKind::kSampler));
  REQUIRE(sampler_table.CommitFrameCopyWrites(batch) == NativeDescriptorStatus::kSuccess);
  CHECK(sampler_table.Update(sampler.handle, {30, 0}) ==
        NativeDescriptorStatus::kInvalidDescriptor);
}

TEST_CASE("native descriptor writes are transactional and block concurrent mutation",
          "[gta4-native][descriptor]") {
  NativeDescriptorEpochTable table(TwoCopySingleSlotConfig());
  const NativeDescriptorAllocation allocation = table.Allocate({10, 0});
  REQUIRE(allocation);
  const NativeDescriptorWriteBatch batch = table.BeginFrameCopyWrites(0);
  REQUIRE(batch);
  CHECK(table.Update(allocation.handle, {30, 0}) == NativeDescriptorStatus::kPendingWriteBatch);
  CHECK(table.Retire(allocation.handle, 7) == NativeDescriptorStatus::kPendingWriteBatch);

  NativeDescriptorWriteBatch altered = batch;
  altered.writes.front().payload.image_view = 999;
  CHECK(table.CommitFrameCopyWrites(altered) == NativeDescriptorStatus::kInvalidWriteBatch);
  CHECK(table.CancelFrameCopyWrites(batch) == NativeDescriptorStatus::kSuccess);
  CHECK(table.Update(allocation.handle, {30, 0}) == NativeDescriptorStatus::kSuccess);
}

TEST_CASE("native descriptor frame copies cannot be updated or reused while in flight",
          "[gta4-native][descriptor]") {
  NativeDescriptorEpochTable table(TwoCopySingleSlotConfig());
  REQUIRE(table.MarkFrameCopySubmitted(0, 1) == NativeDescriptorStatus::kSuccess);
  CHECK(table.BeginFrameCopyWrites(0).status == NativeDescriptorStatus::kFrameCopyInUse);
  CHECK(table.MarkFrameCopySubmitted(0, 2) == NativeDescriptorStatus::kFrameCopyInUse);
  REQUIRE(table.MarkGpuCompleted(1) == NativeDescriptorStatus::kSuccess);

  const NativeDescriptorWriteBatch safe_batch = table.BeginFrameCopyWrites(0);
  REQUIRE(safe_batch);
  REQUIRE(table.CommitFrameCopyWrites(safe_batch) == NativeDescriptorStatus::kSuccess);
  REQUIRE(table.MarkFrameCopySubmitted(0, 2) == NativeDescriptorStatus::kSuccess);
  CHECK(table.MarkFrameCopySubmitted(1, 2) == NativeDescriptorStatus::kNonMonotonicGpuSerial);
  CHECK(table.MarkGpuCompleted(0) == NativeDescriptorStatus::kNonMonotonicGpuSerial);
}

TEST_CASE("native descriptor recycling waits for every tombstone copy",
          "[gta4-native][descriptor]") {
  NativeDescriptorEpochTable table(TwoCopySingleSlotConfig());
  const NativeDescriptorAllocation first = table.Allocate({10, 0});
  REQUIRE(first);
  CommitAllWrites(table, 0);
  CommitAllWrites(table, 1);
  REQUIRE(table.Retire(first.handle, 5) == NativeDescriptorStatus::kSuccess);
  CHECK_FALSE(table.IsRetirementComplete(first.handle));
  REQUIRE(table.MarkGpuCompleted(5) == NativeDescriptorStatus::kSuccess);

  const NativeDescriptorWriteBatch first_tombstone = table.BeginFrameCopyWrites(0);
  REQUIRE(first_tombstone);
  REQUIRE(first_tombstone.writes.size() == 1);
  CHECK(first_tombstone.writes.front().tombstone);
  CHECK(first_tombstone.writes.front().payload == table.fallback());
  CHECK(first_tombstone.writes.front().payload.valid(table.kind()));
  REQUIRE(table.CommitFrameCopyWrites(first_tombstone) == NativeDescriptorStatus::kSuccess);
  CHECK(table.ReclaimReadySlots() == 0);
  CHECK_FALSE(table.Allocate({30, 0}));

  const NativeDescriptorWriteBatch second_tombstone = table.BeginFrameCopyWrites(1);
  REQUIRE(second_tombstone);
  REQUIRE(second_tombstone.writes.size() == 1);
  CHECK(second_tombstone.writes.front().tombstone);
  CHECK(second_tombstone.writes.front().payload == table.fallback());
  REQUIRE(table.CommitFrameCopyWrites(second_tombstone) == NativeDescriptorStatus::kSuccess);
  CHECK(table.ReclaimReadySlots() == 1);
  CHECK(table.IsRetirementComplete(first.handle));

  const NativeDescriptorAllocation second = table.Allocate({30, 0});
  REQUIRE(second);
  CHECK(table.IsRetirementComplete(first.handle));
  CHECK(second.handle.index == first.handle.index);
  CHECK(second.handle.generation != first.handle.generation);
  CHECK_FALSE(table.IsLive(first.handle));
  CHECK(table.Update(first.handle, {50, 0}) == NativeDescriptorStatus::kStaleSlot);
}

TEST_CASE("native descriptor recycling also waits for GPU serial completion",
          "[gta4-native][descriptor]") {
  NativeDescriptorEpochTable table(TwoCopySingleSlotConfig());
  const NativeDescriptorAllocation allocation = table.Allocate({10, 0});
  REQUIRE(allocation);
  CommitAllWrites(table, 0);
  CommitAllWrites(table, 1);
  REQUIRE(table.Retire(allocation.handle, 9) == NativeDescriptorStatus::kSuccess);
  CommitAllWrites(table, 0);
  CommitAllWrites(table, 1);

  CHECK(table.ReclaimReadySlots() == 0);
  CHECK(table.retiring_count() == 1);
  REQUIRE(table.MarkGpuCompleted(8) == NativeDescriptorStatus::kSuccess);
  CHECK(table.ReclaimReadySlots() == 0);
  REQUIRE(table.MarkGpuCompleted(9) == NativeDescriptorStatus::kSuccess);
  CHECK(table.ReclaimReadySlots() == 1);
  CHECK(table.retiring_count() == 0);
}

TEST_CASE("native descriptor maintenance scales with dirty slots rather than table capacity",
          "[gta4-native][descriptor]") {
  NativeDescriptorEpochTable table({2, 65536, NativeDescriptorKind::kSampledImage, {1, 0}});
  REQUIRE(table.valid());
  const NativeDescriptorAllocation allocation = table.Allocate({10, 0});
  REQUIRE(allocation);
  CHECK(table.pending_write_slot_count(0) == 1);
  CHECK(table.pending_write_slot_count(1) == 1);

  REQUIRE(table.Update(allocation.handle, {20, 0}) == NativeDescriptorStatus::kSuccess);
  CHECK(table.pending_write_slot_count(0) == 1);
  CommitAllWrites(table, 0);
  CHECK(table.pending_write_slot_count(0) == 0);
  CHECK(table.pending_write_slot_count(1) == 1);

  CommitAllWrites(table, 1);
  REQUIRE(table.Retire(allocation.handle, 3) == NativeDescriptorStatus::kSuccess);
  CHECK(table.retirement_candidate_count() == 1);
  CommitAllWrites(table, 0);
  CommitAllWrites(table, 1);
  REQUIRE(table.MarkGpuCompleted(3) == NativeDescriptorStatus::kSuccess);
  CHECK(table.ReclaimReadySlots() == 1);
  CHECK(table.retirement_candidate_count() == 0);
}

TEST_CASE("native descriptor table reports capacity and handle errors",
          "[gta4-native][descriptor]") {
  NativeDescriptorEpochTable table(TwoCopySingleSlotConfig());
  const NativeDescriptorAllocation allocation = table.Allocate({10, 0});
  REQUIRE(allocation);
  CHECK(table.Allocate({30, 0}).status == NativeDescriptorStatus::kCapacityExhausted);
  CHECK(table.Update({99, 1}, {30, 0}) == NativeDescriptorStatus::kInvalidSlot);
  CHECK(table.Update(allocation.handle, {0, 0}) == NativeDescriptorStatus::kInvalidDescriptor);
  CHECK(table.BeginFrameCopyWrites(2).status == NativeDescriptorStatus::kInvalidFrameCopy);
}

}  // namespace rex::graphics::gta4_native
