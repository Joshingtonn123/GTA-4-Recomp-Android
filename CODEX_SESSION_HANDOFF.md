# LibertyRecomp native-renderer debugging handoff

## Purpose

This file exports the actionable state of the current Codex session so another agent can continue without repeating the investigation. The immediate task is to fix the native renderer without cycling between these two regressions:

1. Opaque geometry renders, but water, vehicle glass, the diamond, emissive/light sprites, and other translucent objects disappear.
2. Those translucent objects render, but opaque geometry disappears.

The most recent user screenshot showing the second state is:

`/var/folders/vw/zkgh5tjj5lz07rzj05_hns500000gq/T/TemporaryItems/NSIRD_screencaptureui_HgVTO1/Screenshot 2026-08-14 at 10.27.20 AM.png`

## Non-negotiable user constraints

- Do not use Git for any operation.
- Do not use the pseudocode directory at all.
- Do not reintroduce EDRAM emulation, tile-range aliasing, or generic guest-address surface searches. This is a native renderer.
- Use deterministic execution logging and prove conclusions from live logs before changing behavior.
- Cross-reference claims with verified sources.
- Use generated runtime code as the authoritative compiled guest implementation:
  - `/Users/Ozordi/Downloads/LibertyRecomp/glue/rexglue-sdk-main/gta4-recomp/generated`
- Use retail excavation data for strings, RTTI, control flow, class hierarchy, and FLIRT labels:
  - `/Users/Ozordi/Downloads/LibertyRecomp/gta_iv/xex_excavation_retail/flirt_labeled_trusted.txt`
  - `/Users/Ozordi/Downloads/LibertyRecomp/gta_iv/xex_excavation_retail/call_graph.txt`
  - Other files under `/Users/Ozordi/Downloads/LibertyRecomp/gta_iv/xex_excavation_retail`
- Use the running `liberty-decomp` GTA IV MCP server when available.
- Do not use `/Users/Ozordi/Downloads/LibertyRecomp/gta_iv/xex_excavation_retail/pseudocode`.
- Every arithmetic calculation, including trivial arithmetic, hexadecimal arithmetic, sizes, alignment, masks, and coordinates, must be performed by a written Python script. Do not use mental arithmetic or inline Python.
- Do not create summary Markdown/text files unless explicitly requested. This handoff file is explicitly requested by the user.
- Do not use Computer Use; the user will visually inspect the application.
- Build without GTA code generation unless generated guest code was intentionally changed.
- Preserve existing instrumentation and add targeted joinable records rather than broad noisy assumptions.

## Relevant skills and build workflow

The applicable skills are:

- `/Users/Ozordi/Downloads/LibertyRecomp/.agents/skills/libertyrecomp/SKILL.md`
- `/Users/Ozordi/Downloads/LibertyRecomp/.agents/skills/libertyrecomp-build-run/SKILL.md`

Both were read completely in this session. Also read:

- `/Users/Ozordi/Downloads/LibertyRecomp/docs/BUILDING.md`
- `/Users/Ozordi/Downloads/LibertyRecomp/CMakePresets.json`

Existing build directory:

`/Users/Ozordi/Downloads/LibertyRecomp/out/build/macos-release`

Useful build commands:

```sh
cmake --build out/build/macos-release --target rexgpu-gta4-native
cmake --build out/build/macos-release --target LibertyRecomp
```

Direct native unit slice:

```sh
glue/rexglue-sdk-main/out/mac-arm64/unit_tests 'GTA IV native*' --reporter console
```

The latest direct unit run passed all 29 assertions in 6 test cases. Duplicate CVar registration messages are pre-existing test-process noise.

Launch the packaged app directly so the exact built binary is tested:

```sh
'out/build/macos-release/LibertyRecomp/Liberty Recompiled.app/Contents/MacOS/Liberty Recompiled' --gpu_plugin=gta4-native
```

Persistent logs are under:

`/Users/Ozordi/Downloads/LibertyRecomp/out/build/macos-release/LibertyRecomp/Liberty Recompiled.app/Contents/MacOS/logs`

Logs rotate rapidly because the instrumentation is verbose. Snapshot an active run before its relevant segment is overwritten. Historical closed logs may be compressed if disk space is needed, but never alter files from the active run.

## Earlier renderer fixes already present

### Native post-FX/transient fixes

Earlier traces localized the black-frame problem to invalid post-FX/transient surface handling and final composition. That work is already in the shared tree and should not be casually reverted.

### Full Xenos alpha-test semantics

A separate correctness fix was implemented end to end:

- All Xenos alpha compare functions are represented, not only `GreaterEqual`.
- Alpha-dependent draws select a late-fragment-test pixel-shader module so alpha discard occurs before depth/stencil writes.
- Disabled/Always alpha testing uses the early module.
- Stock and shader-override caches carry early and late SPIR-V variants.
- The specialization capability mask is `0x702`.
- Stock shader cache regeneration completed with 1332 entries and identical hash/filename coverage.
- Override compilation produced 13 early/late pixel-shader pairs.
- The native runtime validates that late modules do not declare `EarlyFragmentTests`.

Important touched files include:

- `/Users/Ozordi/Downloads/LibertyRecomp/tools/XenosRecomp/XenosRecomp/shader_recompiler.cpp`
- `/Users/Ozordi/Downloads/LibertyRecomp/tools/XenosRecomp/XenosRecomp/shader_common.h`
- `/Users/Ozordi/Downloads/LibertyRecomp/tools/XenosRecomp/XenosRecomp/dxc_compiler.cpp`
- `/Users/Ozordi/Downloads/LibertyRecomp/tools/XenosRecomp/XenosRecomp/main.cpp`
- `/Users/Ozordi/Downloads/LibertyRecomp/LibertyRecompLib/shader/shader_cache.h`
- `/Users/Ozordi/Downloads/LibertyRecomp/LibertyRecompLib/shader/shader_cache.cpp`
- `/Users/Ozordi/Downloads/LibertyRecomp/LibertyRecompLib/shader_overrides/shader_override_cache.h`
- `/Users/Ozordi/Downloads/LibertyRecomp/tools/XenosRecomp/shader_override_compiler.py`
- `/Users/Ozordi/Downloads/LibertyRecomp/glue/rexglue-sdk-main/src/graphics/gta4_native/graphics_system.h`
- `/Users/Ozordi/Downloads/LibertyRecomp/glue/rexglue-sdk-main/src/graphics/gta4_native/graphics_system.cpp`

This alpha fix previously restored glass/diamond/water correctness and should not be removed to mask the current depth handoff problem.

## Proven translucent-path evidence

Targeted occlusion-query instrumentation classifies water, vehicle glass, vehicle lights, glass, light sprites, emissive shaders, deferred lights, and alpha-reflect shaders. The split variants are:

- `original`
- `depth-only`
- `stencil-only`
- `raster-only`

Earlier broken runs proved that visible water, vehicle glass, and vehicle lights were rejected by depth rather than by shader submission, missing resources, stencil, culling, or blending. In the corrected-alpha path:

- Water `water_e2.fxc` uses VS `A9164AEAEDDD85FF` and PS `C64FD3C169FB7A5A`.
- Vehicle glass uses PS `96BB083E4D7232EA`.
- All required textures were bound for representative draws.
- The primary forward depth attachment is handle `400007C0`, address `00010000`, 1x D32S8.
- Deferred rendering writes the named `gbuffer-z-aa` surface `40226690`, address `00010000`, 2x.
- The title also creates an explicit resolved sampled depth/stencil texture, observed as `D9132320`.

## High-level title handoff currently in the tree

A title-specific command was introduced rather than generic address aliasing:

- `CommandType::kDepthSurfaceHandoff`
- `DepthSurfaceHandoffCommand`
- Submission from the GTA IV deferred-phase hook
- Native execution in `RecordDepthSurfaceHandoff`

Relevant files:

- `/Users/Ozordi/Downloads/LibertyRecomp/glue/rexglue-sdk-main/include/rex/graphics/gta4_native/title_commands.h`
- `/Users/Ozordi/Downloads/LibertyRecomp/glue/rexglue-sdk-main/gta4-recomp/src/gta4_native_hooks.cpp`
- `/Users/Ozordi/Downloads/LibertyRecomp/glue/rexglue-sdk-main/src/graphics/gta4_native/graphics_system.h`
- `/Users/Ozordi/Downloads/LibertyRecomp/glue/rexglue-sdk-main/src/graphics/gta4_native/graphics_system.cpp`

The hook currently names the source through:

```cpp
constexpr uint32_t kDeferredDepthAaWrapperGlobal = 0x83016B40;
```

Live wrapper/surface identities:

- `gbuffer-z-aa` wrapper: `DB56A220`
- Its attachment surface: `40226690`
- Primary destination wrapper/surface: `400007C0`

The hook comment already notes that `outputs[3]` is the resolved sampled-depth wrapper rather than the attachment surface. `sub_828D9608(outputs[3])` yielded the sampled depth texture `D9132320` in execution evidence.

## Exact regression cycle

### State A: opaque works, translucency fails

When the depth handoff is ineffective or rejected, the 1x primary depth attachment keeps stale/incorrect content. Opaque scene geometry is visible, but water, vehicle glass, vehicle lights, and other translucent draws fail depth.

Examples include run 465 and the old-image-preservation run 473. Run 473 summary:

- Water surface original: 12/12 zero.
- Visible raster/stencil controls showed that depth was the rejector for a subset.
- Primary-depth emissive draws were also universally zero.

### State B: translucency works, opaque disappears

When a combined-view SAMPLE_ZERO resolve copies the current `40226690` depth into `400007C0`, water/glass/light queries survive, but the opaque scene disappears. The latest user screenshot confirms this state.

The newest run, 475, showed:

- Water surface original: 259 nonzero of 375 observed draws.
- Vehicle glass: 3/3 original draws nonzero.
- Vehicle light: 1/1 original draw nonzero.
- Glass, emissive, and light-sprite categories also had surviving original draws.
- No Vulkan validation error or device loss was found in the preserved run.

However, the user visually confirmed that opaque geometry was absent. Therefore query survival alone is not an acceptance condition.

## Current stencil-preservation experiment

The latest implementation attempts to preserve the destination stencil while resolving depth:

- A per-destination scratch `VkBuffer` stores one byte per stencil texel.
- Stencil is copied image-to-buffer before the depth resolve.
- The combined-view depth resolve runs with no stencil resolve attachment.
- Stencil is copied buffer-to-image afterward.

Current fields in `NativeSurfaceImage` include:

```cpp
VkBuffer depth_handoff_stencil_scratch_buffer;
VkDeviceMemory depth_handoff_stencil_scratch_memory;
VkDeviceSize depth_handoff_stencil_scratch_size;
bool depth_handoff_stencil_scratch_initialized;
```

Current helper:

```cpp
EnsureDepthHandoffStencilScratch(...)
```

The live invariant proved that the destination stencil value 6 was byte-identical before and after the operation. This fixed the earlier stencil-zeroing failure, but it did not restore opaque geometry. Do not mistake successful stencil preservation for a complete fix.

Current instrumentation records:

- `point=explicit-depth-handoff-stencil-scratch`
- `point=explicit-depth-handoff-stencil-preserve phase=saved transport=buffer`
- `point=explicit-depth-handoff-stencil-preserve phase=restored transport=buffer`
- source depth/stencil probes
- destination-before depth/stencil probes
- destination-after depth/stencil probes
- translucent query results and split variants

## Decisive current diagnosis boundary

The current handoff source is almost certainly the wrong title object at the wrong semantic moment.

The handoff uses `gbuffer-z-aa` attachment surface `40226690`. Execution probes sampled it as reversed-Z far/clear depth 0 when the handoff ran. Copying that value into the primary 1x depth makes forward translucent tests pass, but removes the meaningful scene-depth relationship required by opaque/deferred composition. This matches the latest screenshot.

The title already exposes a distinct explicit pre-clear resolved sampled depth texture:

- `sub_828D9608(outputs[3])` returned `D9132320`.
- Earlier logs called it the depth/stencil resolve target.
- Water binds it as a depth input.
- Light-sprite/deferred paths also sample it.

An earlier attempt passed `D9132320` through the surface-only handoff, and the renderer correctly rejected it because it is a `NativeTextureImage`, not a `NativeSurfaceImage`. The subsequent change to use `40226690` made the command execute but selected the later-cleared attachment, creating the current regression.

Do not fix this with surface address matching, EDRAM tile ranges, or by disabling depth/stencil tests. The next implementation should teach the explicit GTA IV title handoff to consume the named resolved depth texture object directly and materialize its depth into the named primary depth surface while preserving the primary stencil.

This is the leading evidence-backed direction, but the next agent must still add/inspect ordering logs to prove that `D9132320` contains the required pre-clear scene depth at the exact handoff boundary. Do not assume based only on the handle name.

## Required next instrumentation before behavioral edits

Add one joined record at the deferred hook and native command boundary carrying:

- frame and command index
- deferred wrapper array entry
- `outputs[3]` wrapper handle
- `sub_828D9608(outputs[3])` sampled texture handle
- `gbuffer-z-aa` wrapper handle
- `gbuffer-z-aa` attachment surface handle
- primary destination wrapper/surface handle
- the exact resolve command that writes the sampled texture
- the exact clear command that changes the AA attachment
- write/resolve serials and ordering

Add probes in the same sampled frame for:

- resolved texture `D9132320` depth immediately after its explicit resolve
- AA attachment `40226690` depth before and after its clear
- primary `400007C0` depth/stencil before handoff
- primary `400007C0` depth/stencil after handoff

The record must prove which object retains scene depth after the title clear. It should be joinable without handle guessing.

## Proper implementation direction if the texture evidence confirms it

1. Extend `DepthSurfaceHandoffCommand` so the source is explicitly typed/identified as the title’s resolved depth texture, while the destination remains the primary depth surface.
2. Resolve/submit it from `outputs[3]` using the title’s wrapper relationship, not a global address search.
3. In the native renderer, find the exact registered `NativeTextureImage` by its explicit handle/generation.
4. Validate depth format, extent, current generation, GPU-produced status, and shader-readable/current layout.
5. Materialize only the depth aspect into `400007C0`.
6. Preserve the destination stencil independently. The current buffer round-trip is one tested mechanism, but retain it only if the new source path proves it is still needed and valid.
7. Record source/destination probes and reject on any ambiguous resource identity instead of silently falling back to `40226690`.

Do not:

- scan all surfaces by address
- calculate EDRAM ownership/tile ranges
- copy from the newest generic surface owner
- force depth clear values
- disable depth testing
- disable stencil testing
- hard-code a frame, command index, shader hash, or resolution

## Acceptance criteria

One fresh run in the same problematic scene must show all of the following simultaneously:

- Opaque geometry is visibly present.
- Water is visibly present.
- Vehicle windows/glass are visibly present.
- The diamond/alpha-reflect object is visibly present when its scene is active.
- Light bulbs and light sprites do not disappear merely because a dynamic object crosses in front of them.
- Lights do not render through opaque objects.
- Relevant original translucent queries survive when raster-only geometry exists.
- Opaque/deferred target probes show non-black scene contribution.
- The source resolved depth texture contains meaningful scene-depth data before handoff.
- Destination depth after handoff matches the explicit resolved texture, not the cleared AA attachment.
- Destination stencil before/after handoff is identical.
- No Vulkan validation error, device loss, `target-fail`, or `resolve-fail` is introduced.
- A control run without the added native features does not regress.

Do not accept a run only because `point=explicit-depth-handoff result=ok` appears. Do not accept a run only because translucent occlusion queries are nonzero. The latest regression demonstrated that both can be true while opaque geometry is missing.

## Preserved evidence

Current preserved captures:

- Broken opaque / passing translucency run 475:
  - `/tmp/liberty-run475-buffer-stencil.6D1cF9`
- Stencil-buffer invariant run 474:
  - `/tmp/liberty-run474-handoff-invariants.NgKZg8`
- Earlier translucent active capture:
  - `/tmp/liberty-run438-translucent-active`
- Earlier split capture:
  - `/tmp/liberty-run439-translucent-split`
- Earlier deep translucent capture:
  - `/tmp/liberty-run441-deep-translucent-live`

Temporary analysis scripts created during the session:

- `/tmp/analyze_handoff_invariants.py`
- `/tmp/compare_translucent_runs.py`
- `/tmp/calc_stencil_scratch_size.py`

The stencil scratch-size calculation was performed by the written Python script as required.

## Most recent working plan state

1. Join command ordering and probes for the pre-clear resolved depth texture versus the cleared AA attachment.
2. Extend the explicit title handoff to accept the named resolved depth texture without address/EDRAM inference.
3. Copy/materialize depth only into the primary depth surface while preserving its stencil.
4. Rebuild `rexgpu-gta4-native` and `LibertyRecomp` without code generation.
5. Run the exact scene and verify opaque and translucent output plus depth/stencil probes together.

No new implementation for the texture-source direction was started after the latest screenshot. The current tree still contains the buffer-backed stencil preservation plus the incorrect `40226690` source selection described above.

## Tool-call and command history

The Codex application automatically compacted this unusually long session multiple times, and several terminal outputs were truncated because native tracing produced millions of characters. The following is the retained/reconstructed tool-call history and the exact commands visible in the surviving session context. It is intended to let the next agent reproduce the work without relying on an incomplete prose summary.

### Skills and instruction reads

The agent announced and read these skills completely before renderer/build actions:

```text
/Users/Ozordi/Downloads/LibertyRecomp/.agents/skills/libertyrecomp/SKILL.md
/Users/Ozordi/Downloads/LibertyRecomp/.agents/skills/libertyrecomp-build-run/SKILL.md
```

It also read:

```text
/Users/Ozordi/Downloads/LibertyRecomp/docs/BUILDING.md
/Users/Ozordi/Downloads/LibertyRecomp/CMakePresets.json
```

The user-provided transcript imported into the session was read from:

```text
/Users/ozordi/.codex/attachments/9cd40b8a-32f6-432e-9afc-47525f10e6f0/pasted-text.txt
```

### Planning calls

`update_plan` was used throughout the work. The final plan update before this handoff was:

```text
Explanation: The buffer transport preserved stencil but the live screenshot and logs show the chosen source is the post-resolve cleared MSAA attachment. The fix must hand off the title's explicit pre-clear resolved depth texture to the primary depth attachment.

1. Join command ordering and probes for the pre-clear resolved depth texture versus the cleared AA attachment — in progress
2. Extend the explicit title handoff to accept the named resolved depth texture without address/EDRAM inference — pending
3. Copy/materialize depth only into the primary depth surface while preserving its stencil — pending
4. Rebuild renderer and app without code generation — pending
5. Run the scene and verify opaque plus translucent output and depth/stencil probes together — pending
```

### Build commands executed

Native renderer build:

```sh
cmake --build out/build/macos-release --target rexgpu-gta4-native
```

This compiled `graphics_system.cpp` and linked:

```text
/Users/Ozordi/Downloads/LibertyRecomp/glue/rexglue-sdk-main/out/mac-arm64/librexgpu-gta4-native.dylib
```

It completed successfully with three known unrelated warnings:

- ARM FPCR inline-assembly operand width
- unused `one_reg`
- pessimizing `std::move` in presenter code

Full packaged app build:

```sh
cmake --build out/build/macos-release --target LibertyRecomp
```

The retained successful output showed:

```text
[1/4] Building .../gta4_native/graphics_system.cpp.o
[2/4] Linking .../librexgpu-gta4-native.dylib
[3/4] Linking .../Liberty Recompiled.app/Contents/MacOS/Liberty Recompiled
```

No GTA code generation command was run for the depth-handoff work.

Earlier alpha-cache work used these successful commands:

```sh
cmake --build out/build/xenosrecomp-macos --target XenosRecomp
out/build/xenosrecomp-macos/XenosRecomp/XenosRecomp \
  /Users/Ozordi/Downloads/LibertyRecomp/LibertyRecompLib/shader/rage_shaders \
  /tmp/liberty_shader_cache.generated.cpp \
  /Users/Ozordi/Downloads/LibertyRecomp/tools/XenosRecomp/XenosRecomp/shader_common.h
cp /tmp/liberty_shader_cache.generated.cpp LibertyRecompLib/shader/shader_cache.cpp
python3 tools/XenosRecomp/tests/validate_shader_cache_alpha.py \
  /tmp/liberty_shader_cache.before.cpp \
  LibertyRecompLib/shader/shader_cache.cpp
/usr/bin/c++ -std=c++17 -fsyntax-only \
  -I LibertyRecompLib/shader \
  LibertyRecompLib/shader/shader_cache.cpp
```

### Run command executed

The freshly packaged native app was launched directly with:

```sh
'out/build/macos-release/LibertyRecomp/Liberty Recompiled.app/Contents/MacOS/Liberty Recompiled' \
  --gpu_plugin=gta4-native
```

The unified execution call yielded because the app continued running. Its terminal output was extremely large and truncated, but the persistent app logs were used for all conclusions.

A separate pre-existing bounded profiling process was discovered at the same time:

```sh
profile_capture_dir=$(mktemp -d /tmp/liberty-native-profile-60s-fixed.XXXXXX)
profile_binary='out/build/macos-release/LibertyRecomp/Liberty Recompiled.app/Contents/MacOS/Liberty Recompiled'
gtimeout 60 "$profile_binary" \
  --gpu_plugin=gta4-native \
  --gta4_profile_native_renderer \
  --gta4_profile_native_interval=120 \
  --gta4_profile_native_detail=phase \
  --gta4_profile_native_top=20 \
  > "$profile_capture_dir/combined.log" 2>&1
```

This produced run 474 while the direct verification instance produced run 475. The runs were analyzed separately.

### Process-inspection and cleanup calls

Commands used to distinguish the concurrent instances included:

```sh
pgrep -af 'Liberty Recompiled' || true
ps -p 44228,44241,44242,44281,44285 \
  -o pid=,ppid=,lstart=,etime=,state=,command= || true
```

The final verification process had been PID `44285`. A later cleanup attempt returned `no such process`, meaning it had already exited:

```sh
kill -TERM 44285
sleep 2
ps -p 44285 -o pid=,state=,command= || true
```

### Log discovery and inspection calls

Representative log-listing calls:

```sh
ls -1t 'out/build/macos-release/LibertyRecomp/Liberty Recompiled.app/Contents/MacOS/logs' | head -n 8
ls -lt 'out/build/macos-release/LibertyRecomp/Liberty Recompiled.app/Contents/MacOS/logs' | head -n 20
```

Handoff/error searches:

```sh
rg -n \
  'point=explicit-depth-handoff(-stencil-scratch|-stencil-preserve|-probe)?|VUID-|Validation Error|device lost|Device lost' \
  'out/build/macos-release/LibertyRecomp/Liberty Recompiled.app/Contents/MacOS/logs/Liberty Recompiled_474'*.log

rg -n \
  'point=explicit-depth-handoff(-stencil-scratch|-stencil-preserve|-probe)?|VUID-|Validation Error|device lost|Device lost' \
  'out/build/macos-release/LibertyRecomp/Liberty Recompiled.app/Contents/MacOS/logs/Liberty Recompiled_475'*.log
```

Exact source/destination depth/stencil probes:

```sh
rg -n \
  'frame=(3360|3480) .*stage=(depth|stencil)-explicit-handoff' \
  'out/build/macos-release/LibertyRecomp/Liberty Recompiled.app/Contents/MacOS/logs/Liberty Recompiled_474'*.log

rg -n \
  'frame=(4800|4920|5040) .*stage=(depth|stencil)-explicit-handoff' \
  'out/build/macos-release/LibertyRecomp/Liberty Recompiled.app/Contents/MacOS/logs/Liberty Recompiled_475'*.log
```

Affected-family query searches:

```sh
rg -n \
  'point=translucent-query-result .*category=water-surface variant=original .*depth=400007C0/00010000 .*samples-passed=[1-9]' \
  'out/build/macos-release/LibertyRecomp/Liberty Recompiled.app/Contents/MacOS/logs/Liberty Recompiled_475'*.log

rg -n \
  'point=translucent-query-result .*category=(vehicle-glass|vehicle-light|glass|light-sprite|emissive) variant=original .*depth=400007C0/00010000 .*samples-passed=[1-9]' \
  'out/build/macos-release/LibertyRecomp/Liberty Recompiled.app/Contents/MacOS/logs/Liberty Recompiled_475'*.log
```

Final source-code/log relationship search before interruption:

```sh
rg -n \
  'DepthSurfaceHandoff|explicit-depth-handoff|83016B40|D9132320|sub_828D9608|gbuffer-z-aa|gbuffer-z' \
  glue/rexglue-sdk-main/gta4-recomp/src/gta4_native_hooks.cpp \
  glue/rexglue-sdk-main/include \
  glue/rexglue-sdk-main/src/graphics/gta4_native/graphics_system.{h,cpp}

rg -n \
  'depth-stencil-resolve-target|source=40226690|destination=D9132320|deferred-wrapper-pair' \
  /tmp/liberty-run475-buffer-stencil.6D1cF9/*.log \
  /tmp/liberty-run474-handoff-invariants.NgKZg8/*.log
```

The last search produced an enormous result and was truncated. It confirmed that run 475 repeatedly executed the handoff from `40226690` to `400007C0` while opaque geometry was visually missing.

### Python analysis scripts and calls

All arithmetic/counting was delegated to written Python scripts.

Stencil scratch-size script:

```text
/tmp/calc_stencil_scratch_size.py
```

It calculated the full 3456x2168 one-byte stencil scratch size as 7,492,608 bytes and checked integer fit.

Depth/stencil invariant script:

```text
/tmp/analyze_handoff_invariants.py
```

Calls:

```sh
python3 /tmp/analyze_handoff_invariants.py \
  "out/build/macos-release/LibertyRecomp/Liberty Recompiled.app/Contents/MacOS/logs/Liberty Recompiled_474*.log"

python3 /tmp/analyze_handoff_invariants.py \
  "out/build/macos-release/LibertyRecomp/Liberty Recompiled.app/Contents/MacOS/logs/Liberty Recompiled_475*.log"
```

Retained run-474 result:

```text
stencil_pairs=2 stencil_mismatches=0
depth_pairs=2 depth_changes=0
nonzero_stencil_pairs=2
```

Both nonzero stencil pairs were value 6 before and after.

Translucent comparison script:

```text
/tmp/compare_translucent_runs.py
```

Call comparing the previous broken run with the current buffer-preservation run:

```sh
python3 /tmp/compare_translucent_runs.py \
  "out/build/macos-release/LibertyRecomp/Liberty Recompiled.app/Contents/MacOS/logs/Liberty Recompiled_473*.log" \
  '/tmp/liberty-run475-buffer-stencil.6D1cF9/*.log'
```

The resulting counts are summarized earlier in this file.

### Log-preservation calls

Run 475 was snapshotted before active rotation overwrote the relevant scene:

```sh
capture_dir=$(mktemp -d /tmp/liberty-run475-buffer-stencil.XXXXXX)
cp 'out/build/macos-release/LibertyRecomp/Liberty Recompiled.app/Contents/MacOS/logs/Liberty Recompiled_475'*.log "$capture_dir"/
du -sh "$capture_dir"
```

Created:

```text
/tmp/liberty-run475-buffer-stencil.6D1cF9
```

Run 474 handoff-invariant logs were also snapshotted:

```sh
capture_dir=$(mktemp -d /tmp/liberty-run474-handoff-invariants.XXXXXX)
cp 'out/build/macos-release/LibertyRecomp/Liberty Recompiled.app/Contents/MacOS/logs/Liberty Recompiled_474'*.log "$capture_dir"/
```

Created:

```text
/tmp/liberty-run474-handoff-invariants.NgKZg8
```

### Unit-test call

```sh
glue/rexglue-sdk-main/out/mac-arm64/unit_tests \
  'GTA IV native*' \
  --reporter console
```

Result:

```text
All tests passed (29 assertions in 6 test cases)
```

## File edit inventory

This section distinguishes edits made during this investigation from research-only observations. All edits were made without Git.

### Explicit title depth-handoff edits already in the shared tree

`/Users/Ozordi/Downloads/LibertyRecomp/glue/rexglue-sdk-main/include/rex/graphics/gta4_native/title_commands.h`

- Added `CommandType::kDepthSurfaceHandoff`.
- Added trivially copyable `DepthSurfaceHandoffCommand`.
- Current compile-time size assertion is 96 bytes.

`/Users/Ozordi/Downloads/LibertyRecomp/glue/rexglue-sdk-main/gta4-recomp/src/gta4_native_hooks.cpp`

- Added named deferred-wrapper globals and diagnostics.
- Added `kDeferredDepthAaWrapperGlobal = 0x83016B40`.
- Added deferred-wrapper and source/destination association logging.
- Added submission of `DepthSurfaceHandoffCommand` after deferred-phase handling.
- Current source selection resolves the `gbuffer-z-aa` wrapper to attachment surface `40226690`.
- This source selection is now believed to be the regression boundary because that attachment has already been cleared when the queued handoff executes.

`/Users/Ozordi/Downloads/LibertyRecomp/glue/rexglue-sdk-main/src/graphics/gta4_native/graphics_system.h`

- Added `RecordDepthSurfaceHandoff(...)` declaration.
- Added `EnsureDepthHandoffStencilScratch(...)` declaration.
- Added `NativeSurfaceImage` stencil-scratch fields:

```cpp
VkBuffer depth_handoff_stencil_scratch_buffer = VK_NULL_HANDLE;
VkDeviceMemory depth_handoff_stencil_scratch_memory = VK_NULL_HANDLE;
VkDeviceSize depth_handoff_stencil_scratch_size = 0;
bool depth_handoff_stencil_scratch_initialized = false;
```

`/Users/Ozordi/Downloads/LibertyRecomp/glue/rexglue-sdk-main/src/graphics/gta4_native/graphics_system.cpp`

- Added command-size/device lookup/recording paths for `kDepthSurfaceHandoff`.
- Added deterministic command-begin/result records.
- Added `RecordDepthSurfaceHandoff(...)` validation and execution.
- Added full source/destination depth/stencil probes.
- Added `EnsureDepthHandoffStencilScratch(...)`.
- Allocates a device-local transfer-source/transfer-destination `VkBuffer` sized to one byte per stencil texel.
- Before depth resolve:
  - transitions the destination image to transfer-source
  - copies the stencil aspect image-to-buffer
  - transitions the buffer from transfer-write to transfer-read
- Executes the combined-view Vulkan SAMPLE_ZERO depth resolve with no stencil resolve attachment.
- After depth resolve:
  - transitions the destination image to transfer-destination
  - copies the saved stencil buffer back to the stencil aspect
  - restores depth/stencil attachment layout
- Added deterministic records:
  - `point=explicit-depth-handoff-stencil-scratch`
  - `point=explicit-depth-handoff-stencil-preserve phase=saved transport=buffer`
  - `point=explicit-depth-handoff-stencil-preserve phase=restored transport=buffer`
- Added shutdown destruction of the scratch buffer and its memory.

The buffer edit compiled and ran successfully, and it preserved stencil exactly. It did not solve opaque rendering because the selected depth source is semantically wrong.

### Earlier failed preservation variant

Before the current buffer implementation, stencil was temporarily preserved with image-to-image stencil copies. Execution evidence showed that MoltenVK restored the combined Metal depth/stencil backing and thereby undid the depth resolve. That image scratch implementation was replaced by the buffer implementation and should not be restored.

### Earlier depth-only-view variant

A depth-only sampled-view resolve was tested. MoltenVK left destination depth unchanged, so the operation was effectively a no-op for this path. The combined-view resolve is why the later path changed depth, but it exposed the wrong-source problem. Do not return to the depth-only-view variant as a supposed fix; it merely returns to the state where translucency fails.

### Alpha-test/cache edits made by delegated agents

The session used high-intelligence subagents for bounded research and implementation. Their edits are shared in the same workspace.

Stock shader generator/cache agent:

- Extended `ShaderCacheEntry` with `lateSpirvOffset` and `lateSpirvSize`.
- Emitted alpha specialization capability `0x702` for RT0-writing pixel shaders.
- Implemented all eight alpha compare predicates, including unordered `NotEqual` using vector `isnan` logic.
- Serialized DXC invocation after discovering concurrent DXC crashes.
- Generated early and late SPIR-V modules.
- Regenerated the 1332-entry stock cache with identical hash/filename coverage.

Override agent:

- Converted 12 unique alpha-capable override HLSL files covering 13 manifest entries.
- Updated override manifest capability masks to `0x702`.
- Generated early/late override pairs.
- Validated that late override modules omit `EarlyFragmentTests`.

Native runtime alpha agent:

- Added dual shader modules to native shader state.
- Added stock/override decode, creation, destruction, and selection.
- Packed alpha enable/function specialization data.
- Selected late modules for enabled non-Always alpha tests.
- Rejected missing capability/module cases.
- Added runtime SPIR-V execution-mode validation and deterministic alpha-semantics records.

### Instrumentation edits made during translucent research

`graphics_system.cpp` contains targeted query/probe instrumentation added across prior passes:

- Shader-family classification for water texture versus water surface.
- Vehicle glass, generic glass, vehicle lights, light sprites, emissive, deferred-light, and alpha-reflect classification.
- Occlusion query allocation, begin/end, and asynchronous result collection.
- Original/depth-only/stencil-only/raster-only split variants.
- Complete guest and effective depth/stencil/cull/blend/alpha state in result records.
- Source/destination texture and depth-lineage records.
- Water input/resource probes and final surface/frontbuffer/presenter probes.

Preserve these records until the simultaneous opaque/translucent acceptance run succeeds.

### Temporary file edits

Written Python scripts:

```text
/tmp/calc_stencil_scratch_size.py
/tmp/analyze_handoff_invariants.py
/tmp/compare_translucent_runs.py
```

Handoff export requested by the user:

```text
/Users/Ozordi/Downloads/LibertyRecomp/CODEX_SESSION_HANDOFF.md
```

No Git branch, commit, reset, checkout, staging operation, or diff command was intentionally used for this work.
