#include <catch2/catch_test_macros.hpp>

#include <rex/graphics/gta4_native/anti_aliasing_policy.h>

#include "graphics/gta4_native/native_msaa_policy.h"

namespace rex::graphics::gta4_native {
namespace {

TEST_CASE("GTA IV native MSAA overrides only title-multisampled scene families") {
  CHECK(ShouldApplyNativeSceneSampleOverride(false, true, true));

  CHECK_FALSE(ShouldApplyNativeSceneSampleOverride(false, true, false));
  CHECK_FALSE(ShouldApplyNativeSceneSampleOverride(false, false, true));
  CHECK_FALSE(ShouldApplyNativeSceneSampleOverride(true, true, true));
}

TEST_CASE("GTA IV native MSAA preserves mixed and forward sample topology") {
  // A family containing any guest 1x attachment is title-authored mixed or
  // forward topology and must remain untouched as a coherent family.
  CHECK_FALSE(ShouldApplyNativeSceneSampleOverride(false, true, false));

  // Reflection families own an independent sample-count policy.
  CHECK_FALSE(ShouldApplyNativeSceneSampleOverride(true, true, true));
}

TEST_CASE("GTA IV native MSAA selects the highest supported requested count") {
  CHECK(SelectSupportedNativeSceneSampleCount(4, 1 | 2 | 4) == 4);
  CHECK(SelectSupportedNativeSceneSampleCount(4, 1 | 2) == 2);
  CHECK(SelectSupportedNativeSceneSampleCount(4, 1) == 1);
  CHECK(SelectSupportedNativeSceneSampleCount(2, 1 | 2 | 4) == 2);
  CHECK(SelectSupportedNativeSceneSampleCount(2, 1) == 1);
  CHECK(SelectSupportedNativeSceneSampleCount(1, 1 | 2 | 4) == 1);
  CHECK(SelectSupportedNativeSceneSampleCount(4, 0) == 0);
}

TEST_CASE("GTA IV native color resolve-all consumes every physical host sample") {
  const NativeColorResolveSampleMapping mapping = NormalizeColorResolveSampleMapping(
      xenos::MsaaSamples::k2X, xenos::MsaaSamples::k2X, xenos::MsaaSamples::k4X,
      xenos::CopySampleSelect::k01);
  CHECK(mapping.content_samples == xenos::MsaaSamples::k4X);
  CHECK(mapping.requested_samples == xenos::MsaaSamples::k4X);
  CHECK(mapping.sample_select == xenos::CopySampleSelect::k0123);
  CHECK(mapping.expanded_to_physical_samples);
}

TEST_CASE("GTA IV native color resolve preserves explicit guest sample selection") {
  const NativeColorResolveSampleMapping explicit_sample = NormalizeColorResolveSampleMapping(
      xenos::MsaaSamples::k2X, xenos::MsaaSamples::k2X, xenos::MsaaSamples::k4X,
      xenos::CopySampleSelect::k0);
  CHECK(explicit_sample.content_samples == xenos::MsaaSamples::k2X);
  CHECK(explicit_sample.requested_samples == xenos::MsaaSamples::k2X);
  CHECK(explicit_sample.sample_select == xenos::CopySampleSelect::k0);
  CHECK_FALSE(explicit_sample.expanded_to_physical_samples);
}

TEST_CASE("GTA IV native color resolve preserves placement reinterpretation topology") {
  const NativeColorResolveSampleMapping alternate_view = NormalizeColorResolveSampleMapping(
      xenos::MsaaSamples::k4X, xenos::MsaaSamples::k2X, xenos::MsaaSamples::k4X,
      xenos::CopySampleSelect::k01);
  CHECK(alternate_view.content_samples == xenos::MsaaSamples::k4X);
  CHECK(alternate_view.requested_samples == xenos::MsaaSamples::k2X);
  CHECK(alternate_view.sample_select == xenos::CopySampleSelect::k01);
  CHECK_FALSE(alternate_view.expanded_to_physical_samples);
}

TEST_CASE("GTA IV native color resolve leaves a matching physical fallback unchanged") {
  const NativeColorResolveSampleMapping fallback = NormalizeColorResolveSampleMapping(
      xenos::MsaaSamples::k2X, xenos::MsaaSamples::k2X, xenos::MsaaSamples::k2X,
      xenos::CopySampleSelect::k01);
  CHECK(fallback.content_samples == xenos::MsaaSamples::k2X);
  CHECK(fallback.requested_samples == xenos::MsaaSamples::k2X);
  CHECK(fallback.sample_select == xenos::CopySampleSelect::k01);
  CHECK_FALSE(fallback.expanded_to_physical_samples);
}

TEST_CASE("GTA IV native placement materialization uses a direct physical copy only when safe") {
  CHECK(CanDirectlyMaterializePhysicalSamples(true, 4u, 4u));
  CHECK(CanDirectlyMaterializePhysicalSamples(true, 2u, 2u));
  CHECK_FALSE(CanDirectlyMaterializePhysicalSamples(false, 4u, 4u));
  CHECK_FALSE(CanDirectlyMaterializePhysicalSamples(true, 2u, 4u));
}

TEST_CASE("GTA IV unified anti-aliasing exposes only canonical public values") {
  CHECK(ParseAntiAliasingMode("off") == AntiAliasingMode::kOff);
  CHECK(ParseAntiAliasingMode("fxaa") == AntiAliasingMode::kFxaa);
  CHECK(ParseAntiAliasingMode("smaa") == AntiAliasingMode::kSmaa);
  CHECK(ParseAntiAliasingMode("msaa2x") == AntiAliasingMode::kMsaa2x);
  CHECK(ParseAntiAliasingMode("msaa4x") == AntiAliasingMode::kMsaa4x);

  CHECK_FALSE(ParseAntiAliasingMode("spatial").has_value());
  CHECK_FALSE(ParseAntiAliasingMode("2x").has_value());
  CHECK_FALSE(ParseAntiAliasingMode("4x").has_value());
}

TEST_CASE("GTA IV unified anti-aliasing resolves legacy configurations deterministically") {
  const auto fxaa_over_msaa = ResolveAntiAliasingConfiguration("fxaa", "4x", true);
  CHECK(fxaa_over_msaa.mode == AntiAliasingMode::kFxaa);
  CHECK(fxaa_over_msaa.ignored_legacy_scene_msaa);

  const auto smaa_over_msaa = ResolveAntiAliasingConfiguration("smaa", "2x", true);
  CHECK(smaa_over_msaa.mode == AntiAliasingMode::kSmaa);
  CHECK(smaa_over_msaa.ignored_legacy_scene_msaa);

  CHECK(ResolveAntiAliasingConfiguration("off", "2x", false).mode ==
        AntiAliasingMode::kMsaa2x);
  CHECK(ResolveAntiAliasingConfiguration("off", "4x", false).mode ==
        AntiAliasingMode::kMsaa4x);
  CHECK(ResolveAntiAliasingConfiguration("off", "4x", false, true).mode ==
        AntiAliasingMode::kOff);
  CHECK(ResolveAntiAliasingConfiguration("off", "original", false).mode ==
        AntiAliasingMode::kOff);
  CHECK(ResolveAntiAliasingConfiguration("spatial", "original", false).mode ==
        AntiAliasingMode::kFxaa);
  CHECK(ResolveAntiAliasingConfiguration("invalid", "original", true).mode ==
        AntiAliasingMode::kFxaa);
  CHECK(ResolveAntiAliasingConfiguration("invalid", "original", false).mode ==
        AntiAliasingMode::kOff);
}

TEST_CASE("GTA IV unified anti-aliasing routes exactly one implementation") {
  for (AntiAliasingMode mode : {AntiAliasingMode::kOff, AntiAliasingMode::kFxaa,
                                AntiAliasingMode::kSmaa, AntiAliasingMode::kMsaa2x,
                                AntiAliasingMode::kMsaa4x}) {
    CHECK(HasExclusiveAntiAliasingRoute(GetAntiAliasingRoute(mode)));
  }

  const auto off = GetAntiAliasingRoute(AntiAliasingMode::kOff);
  CHECK_FALSE(off.presentation_fxaa);
  CHECK_FALSE(off.presentation_smaa);
  CHECK(off.scene_sample_count == 1u);

  const auto fxaa = GetAntiAliasingRoute(AntiAliasingMode::kFxaa);
  CHECK(fxaa.presentation_fxaa);
  CHECK_FALSE(fxaa.presentation_smaa);
  CHECK(fxaa.scene_sample_count == 1u);

  const auto smaa = GetAntiAliasingRoute(AntiAliasingMode::kSmaa);
  CHECK_FALSE(smaa.presentation_fxaa);
  CHECK(smaa.presentation_smaa);
  CHECK(smaa.scene_sample_count == 1u);

  CHECK(GetAntiAliasingRoute(AntiAliasingMode::kMsaa2x).scene_sample_count == 2u);
  CHECK(GetAntiAliasingRoute(AntiAliasingMode::kMsaa4x).scene_sample_count == 4u);
}

TEST_CASE("GTA IV unified anti-aliasing applies only topology-compatible changes live") {
  CHECK(CanApplyAntiAliasingLive(AntiAliasingMode::kOff, AntiAliasingMode::kFxaa));
  CHECK(CanApplyAntiAliasingLive(AntiAliasingMode::kFxaa, AntiAliasingMode::kSmaa));
  CHECK(CanApplyAntiAliasingLive(AntiAliasingMode::kMsaa2x, AntiAliasingMode::kMsaa2x));

  CHECK_FALSE(CanApplyAntiAliasingLive(AntiAliasingMode::kSmaa,
                                      AntiAliasingMode::kMsaa2x));
  CHECK_FALSE(CanApplyAntiAliasingLive(AntiAliasingMode::kMsaa4x,
                                      AntiAliasingMode::kOff));
  CHECK_FALSE(CanApplyAntiAliasingLive(AntiAliasingMode::kMsaa2x,
                                      AntiAliasingMode::kMsaa4x));
}

}  // namespace
}  // namespace rex::graphics::gta4_native
