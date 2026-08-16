# W3: Community Shaders Sky Feature Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Offer Truth's atmosphere, sky-field, and aurora work to Community Shaders as a user-facing feature, with SkyrimBridge as an optional runtime-detected input.

**Architecture:** Community Shaders compiles features into its binary (`Feature::GetFeatureList()` is a static vector at `src/Feature.cpp:219`), so this work lives in a fork branch and reaches users through pull requests. `CONTRIBUTING.md` rules that backend systems are not features, so the offered feature is the sky rendering itself and not a state carrier. The 1,415 lines of HLSL port to Community Shaders conventions; the 4,044 lines of CPU mirror stay in truth-enb and serve as the parity oracle behind every correctness claim in a PR.

**Tech Stack:** C++23, Community Shaders `dev` branch, HLSL `cs_5_0` and `ps_5_0`, ImGui, nlohmann JSON, EASTL.

## Global Constraints

- Community Shaders is GPL-3.0-or-later with a modding exception. All work in this plan lives in the fork. **No Community Shaders source is ever copied into truth-enb or any other repository in this workspace.** Flow is one-way.
- Truth's HLSL is MIT and may be contributed into a GPL project. Preserve the existing author lines when porting a header.
- `CONTRIBUTING.md` requirements, all mandatory: conventional commit titles, atomic commits, no dead code, no work-in-progress code, no AI-slop comments, CPU and GPU numbers, at most 4,000 lines per PR, profile markers where possible.
- Feature settings ship in the same PR as the feature. Do not raise a feature whose settings arrive later.
- A consumer of SkyrimBridge must treat its absence as a supported configuration. The feature never hard-depends on a second mod.

---

### Task 1: Establish viability before writing any feature code

`CONTRIBUTING.md` directs contributors to Discord to discuss potential contributions. W1 Task 5 raises a data-only PR that answers the same question far more cheaply. This task is a gate: a negative answer here means Tasks 2 through 5 do not run, and the sky work stays in truth-enb.

**Files:**
- Create: `docs/DECISION-effects11-upstream.md` (in truth-enb)

**Interfaces:**
- Consumes: the outcome of W1 Task 5.
- Produces: a recorded go or no-go that Tasks 2 through 5 depend on.

- [ ] **Step 1: Confirm the W1 data PR has a response**

Do not start this task until the `SettingsPatches.json` PR from W1 Task 5 has been reviewed, merged, or rejected. Its outcome is the cheapest available evidence about whether larger contributions are worth writing.

- [ ] **Step 2: Open the Discord conversation**

Post in the Community Shaders Discord describing: what the sky work does, that it targets sky scattering which the Effects 11 page names as unsupported, that it carries a CPU reference implementation with parity tests, and the proposed three-PR split. Ask whether they want it and in what shape.

- [ ] **Step 3: Record the answer**

Create `truth-enb/docs/DECISION-effects11-upstream.md` recording the W1 PR URL and outcome, the Discord thread, the answer, and the decision. If the answer is no, record what would change it and stop here. If the answer is yes, record any shape changes they asked for, because those override this plan.

- [ ] **Step 4: Commit**

```bash
cd /c/dev/truth-enb && git add docs/DECISION-effects11-upstream.md
git commit -m "docs: record the upstream viability answer"
```

---

### Task 2: Feature scaffold, atmosphere core, sky-view adapter, tier selection

This is PR 2. It produces a feature that renders something visible on its own, because a scaffold with no output is dead code and would be rejected.

**Files (all in the Community Shaders fork):**
- Create: `src/Features/TruthAtmosphere.h`
- Create: `src/Features/TruthAtmosphere.cpp`
- Create: `features/TruthAtmosphere/Shaders/Features/TruthAtmosphere.ini`
- Create: `features/TruthAtmosphere/Shaders/TruthAtmosphere/AtmosphereCore.hlsli`
- Create: `features/TruthAtmosphere/Shaders/TruthAtmosphere/SkyViewAdapter.hlsli`
- Create: `features/TruthAtmosphere/Shaders/TruthAtmosphere/Quality.hlsli`
- Create: `features/TruthAtmosphere/Shaders/TruthAtmosphere/SkyCS.hlsl`
- Modify: `src/Feature.cpp:221`
- Modify: `src/Globals.h`, `src/Globals.cpp`

**Interfaces:**
- Consumes: `TruthAtmosphereCore.fxh` (94 lines), `TruthSkyViewAdapter.fxh` (105), `TruthQuality.fxh` (71) from truth-enb as port sources.
- Produces: `struct TruthAtmosphere : Feature` with short name `TruthAtmosphere`, shader define `TRUTH_ATMOSPHERE`, and `CbData` carrying tier, celestial direction, and time. Tasks 3 and 4 extend `CbData` additively and append to `SkyCS.hlsl`.

- [ ] **Step 1: Fork and branch**

```bash
gh repo fork community-shaders/skyrim-community-shaders --clone --remote
cd skyrim-community-shaders && git checkout -b feat/truth-atmosphere dev
```

- [ ] **Step 2: Write the feature header**

Create `src/Features/TruthAtmosphere.h`:

```cpp
#pragma once

struct TruthAtmosphere : public Feature
{
	virtual inline std::string GetName() override { return "Truth Atmosphere"; }
	virtual inline std::string GetShortName() override { return "TruthAtmosphere"; }
	virtual inline std::string_view GetCategory() const override { return "Sky"; }
	virtual inline std::string_view GetShaderDefineName() override { return "TRUTH_ATMOSPHERE"; }
	virtual inline bool HasShaderDefine(RE::BSShader::Type t) override { return t == RE::BSShader::Type::Sky; }

	virtual void RestoreDefaultSettings() override;
	virtual void LoadSettings(json& o_json) override;
	virtual void SaveSettings(json& o_json) override;
	virtual void DrawSettings() override;

	virtual void SetupResources() override;
	virtual void ClearShaderCache() override;
	void CompileShaders();

	struct Settings
	{
		uint  QualityTier = 2;     // 0 performance to 4 cinematic, matching Truth
		float Turbidity = 2.5f;
		bool  UseBridgeInput = true;
	} settings;

	struct CbData
	{
		DirectX::XMFLOAT3 CelestialDirection;
		float             Turbidity;
		uint              QualityTier;
		float             GameHour;
		float             InteriorFactor;
		float             _pad0;
	};
	static_assert(sizeof(CbData) % 16 == 0,
		"CbData must be aligned to 16 bytes.");

	eastl::unique_ptr<ConstantBuffer> atmosphereCb = nullptr;
	winrt::com_ptr<ID3D11ComputeShader> skyCs = nullptr;
};
```

- [ ] **Step 3: Write the feature implementation**

Create `src/Features/TruthAtmosphere.cpp`:

```cpp
#include "TruthAtmosphere.h"

#include "Globals.h"
#include "State.h"

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	TruthAtmosphere::Settings,
	QualityTier,
	Turbidity,
	UseBridgeInput)

void TruthAtmosphere::RestoreDefaultSettings()
{
	settings = {};
}

void TruthAtmosphere::LoadSettings(json& o_json)
{
	settings = o_json;
}

void TruthAtmosphere::SaveSettings(json& o_json)
{
	o_json = settings;
}

void TruthAtmosphere::DrawSettings()
{
	ImGui::SeparatorText("Atmosphere");
	uint step = 1;
	ImGui::InputScalar("Quality Tier", ImGuiDataType_U32, &settings.QualityTier, &step);
	ImGui::SliderFloat("Turbidity", &settings.Turbidity, 1.0f, 10.0f);
	ImGui::Checkbox("Use SkyrimBridge input when available", &settings.UseBridgeInput);
}

void TruthAtmosphere::SetupResources()
{
	atmosphereCb = eastl::make_unique<ConstantBuffer>(ConstantBufferDesc<CbData>());
	CompileShaders();
}

void TruthAtmosphere::ClearShaderCache()
{
	skyCs = nullptr;
	CompileShaders();
}

void TruthAtmosphere::CompileShaders()
{
	if (auto rawPtr = reinterpret_cast<ID3D11ComputeShader*>(
			Util::CompileShader(L"Data\\Shaders\\TruthAtmosphere\\SkyCS.hlsl", {}, "cs_5_0")))
		skyCs.attach(rawPtr);
}
```

- [ ] **Step 4: Register the feature**

Three edits. In `src/Feature.cpp`, add the include at the top and the singleton to the static vector that `GetFeatureList()` returns at line 221:

```cpp
#include "Features/TruthAtmosphere.h"

const std::vector<Feature*>& Feature::GetFeatureList()
{
	static std::vector<Feature*> features = {
		// ... existing entries, unchanged ...
		TruthAtmosphere::GetSingleton(),
	};
```

In `src/Globals.h`, declare the accessor alongside the other feature globals:

```cpp
struct TruthAtmosphere;
namespace globals::features
{
	extern TruthAtmosphere* truthAtmosphere;
}
```

In `src/Globals.cpp`, define it:

```cpp
namespace globals::features
{
	TruthAtmosphere* truthAtmosphere = nullptr;
}
```

Add the singleton accessor to `src/Features/TruthAtmosphere.h`, inside the struct:

```cpp
	static TruthAtmosphere* GetSingleton()
	{
		static TruthAtmosphere singleton;
		return &singleton;
	}
```

Read the neighbouring entries in all three files before editing and match their exact spelling and ordering convention. If upstream has changed the registration shape since 2026-08-15, upstream wins.

- [ ] **Step 5: Create the feature ini**

Create `features/TruthAtmosphere/Shaders/Features/TruthAtmosphere.ini`:

```ini
[Info]
Version = 1-0-0
```

Omit the `[Nexus]` block until the feature has a Nexus page. An `autoupload` entry pointing at nothing is dead configuration.

- [ ] **Step 6: Port the three headers**

Port `TruthAtmosphereCore.fxh`, `TruthSkyViewAdapter.fxh`, and `TruthQuality.fxh` to `.hlsli` files under `features/TruthAtmosphere/Shaders/TruthAtmosphere/`. The mechanical changes are: ENB UI uniforms become `CbData` fields read from the constant buffer, `TRUTH_QUALITY_TIER` becomes the `QualityTier` field rather than a compile-time define, and ENB's supplied scene and depth textures become the Community Shaders equivalents. Preserve every existing author and provenance comment.

Write `SkyCS.hlsl` as the compute entry point that includes all three and writes the sky term.

- [ ] **Step 7: Verify it builds and runs**

Build the fork and launch. Expected: the feature appears in the Community Shaders menu under Sky, its settings draw, and the sky term is visible. A feature that compiles but renders nothing is not ready to raise.

- [ ] **Step 8: Capture parity and performance evidence**

Render the same scene and tier through truth-enb's WARP reference renderer and through the ported feature, and compare. Record the delta.

```bash
cd /c/dev/truth-enb && cmake --build --preset vs2026-x64-debug --target truth_reference_renderer
```

Capture GPU timing from the Community Shaders performance overlay at each of the five tiers, and CPU cost of the per-frame constant buffer update. `CONTRIBUTING.md` states that without concrete numbers it will be hard or impossible to get the work merged, so this step is not optional.

- [ ] **Step 9: Commit atomically and raise the PR**

Commit as separate atomic commits: scaffold, then each ported header, then the entry shader. Their reviewers reject single-large-commit PRs.

```bash
gh pr create --repo community-shaders/skyrim-community-shaders --base dev \
  --title "feat: add Truth Atmosphere sky feature"
```

The PR body carries the tier table, the measured GPU cost per tier, the parity result against the CPU reference, and the Alpha flag.

---

### Task 3: Sky fields, cloud volume, cloud lighting

This is PR 3, 762 lines of HLSL. Raise it only after PR 2 is merged, since it extends that feature.

**Files (in the fork):**
- Create: `features/TruthAtmosphere/Shaders/TruthAtmosphere/SkyFields.hlsli`
- Create: `features/TruthAtmosphere/Shaders/TruthAtmosphere/CloudVolume.hlsli`
- Create: `features/TruthAtmosphere/Shaders/TruthAtmosphere/CloudLighting.hlsli`
- Modify: `src/Features/TruthAtmosphere.h`, `src/Features/TruthAtmosphere.cpp`
- Modify: `features/TruthAtmosphere/Shaders/TruthAtmosphere/SkyCS.hlsl`

**Interfaces:**
- Consumes: `CbData` and `SkyCS.hlsl` from Task 2.
- Produces: appended `CbData` fields for cloud coverage and density. Task 4 appends after these; it never reorders them.

- [ ] **Step 1: Port the three headers**

Port `TruthSkyFields.fxh` (170), `TruthCloudVolume.fxh` (444), and `TruthCloudLighting.fxh` (148) with the same mechanical changes as Task 2, Step 6.

- [ ] **Step 2: Extend CbData additively**

Append to `CbData`, never reordering the Task 2 fields, and keep the 16-byte assertion satisfied:

```cpp
		float CloudCoverage;
		float CloudDensity;
		float CloudPhase;
		float _pad1;
```

- [ ] **Step 3: Carry the tier budgets**

The volume march budgets are the contract: tiers 0 and 1 are analytic with no marching, tiers 2, 3, and 4 march at 8/2, 12/3, and 16/4. In truth-enb these are compile-time. Here they are a runtime branch on `QualityTier`, so verify the analytic tiers genuinely skip the march loop rather than running it with a zero count.

- [ ] **Step 4: Verify the tiers differ**

Render all five tiers and compare content hashes. Expected: the two analytic tiers differ from all three marching tiers, the three marching tiers differ from each other, and the same tier rendered twice agrees. This mirrors `RenderedTierReferences` in truth-enb and is the assertion that catches a tier that silently collapsed.

- [ ] **Step 5: Capture performance across tiers**

Record GPU cost at each of the five tiers. The point of tiering is that cost scales; a table showing flat cost is a defect, not a result.

- [ ] **Step 6: Commit atomically and raise the PR**

```bash
gh pr create --repo community-shaders/skyrim-community-shaders --base dev \
  --title "feat: add volumetric clouds to Truth Atmosphere"
```

---

### Task 4: Aurora curtain

This is PR 4, 383 lines.

**Files (in the fork):**
- Create: `features/TruthAtmosphere/Shaders/TruthAtmosphere/AuroraCurtain.hlsli`
- Modify: `src/Features/TruthAtmosphere.h`, `src/Features/TruthAtmosphere.cpp`
- Modify: `features/TruthAtmosphere/Shaders/TruthAtmosphere/SkyCS.hlsl`

**Interfaces:**
- Consumes: `CbData` from Tasks 2 and 3.
- Produces: appended `CbData` field `AuroraIntensity`.

- [ ] **Step 1: Port the header**

Port `TruthAuroraCurtain.fxh` (383). It is night-only and world-space, so it needs the celestial direction already in `CbData` from Task 2.

- [ ] **Step 2: Extend CbData additively**

```cpp
		float AuroraIntensity;
		float _pad2;
		float _pad3;
		float _pad4;
```

- [ ] **Step 3: Verify the night gate**

Assert that daytime output is bit-identical with the aurora enabled and disabled. An emission term that leaks into daylight is the failure mode here, and an identity assertion catches it without a golden image.

- [ ] **Step 4: Capture performance**

Record GPU cost with the aurora active and inactive, at each tier. The aurora sample counts per tier are 1, 2, 4, 7, and 10.

- [ ] **Step 5: Commit atomically and raise the PR**

```bash
gh pr create --repo community-shaders/skyrim-community-shaders --base dev \
  --title "feat: add aurora curtain to Truth Atmosphere"
```

---

### Task 5: Optional SkyrimBridge input

`CONTRIBUTING.md` forbids raising code ahead of the PR that uses it, so this lands only once a shader stage consumes the richer data. It follows Task 3, whose cloud coverage and density are the first fields that benefit.

**Files (in the fork):**
- Create: `src/Features/TruthAtmosphere/BridgeInput.h`
- Create: `src/Features/TruthAtmosphere/BridgeInput.cpp`
- Modify: `src/Features/TruthAtmosphere.cpp`

**Interfaces:**
- Consumes: `SB_GetBridgeInterface` from W2, resolved at runtime. Never linked.
- Produces: `TruthAtmosphere::Bridge::TryAcquire()` returning `const SB::AllData*` or `nullptr`.

- [ ] **Step 1: Write the resolver**

Follow the consumer contract in W2's `docs/BRIDGE-ABI.md` exactly: resolve the module, resolve the symbol, check `version`, check `allDataSize`, and only then dereference. Vendor a copy of `SkyrimBridgeAPI.h` and `BridgeData.h` into the fork under their MIT licence with the licence header intact, since the fork cannot depend on a sibling checkout.

- [ ] **Step 2: Verify the absent case first**

Run the feature with SkyrimBridge not installed. Expected: `TryAcquire` returns `nullptr`, the feature renders from Community Shaders' own weather and time data, and nothing logs an error. This is the common configuration and it must be the well-tested one.

- [ ] **Step 3: Verify the present case**

Install SkyrimBridge and confirm the feature picks up live weather state, and that the settings checkbox disables it.

- [ ] **Step 4: Verify the mismatch case**

Temporarily change `kBridgeInterfaceVersion` in the vendored header and confirm `TryAcquire` returns `nullptr` rather than reading a mismatched layout. Revert.

- [ ] **Step 5: Commit atomically and raise the PR**

```bash
gh pr create --repo community-shaders/skyrim-community-shaders --base dev \
  --title "feat: read optional SkyrimBridge weather state"
```

---

## Notes for the executor

Task 1 is a hard gate. Do not write feature code before it returns a yes.

Tasks 2, 3, and 4 are strictly ordered and each waits for the previous PR to merge, because each extends the same feature and `CbData` grows additively. Task 5 follows Task 3.

Nothing in this plan modifies any repository in this workspace except `truth-enb/docs/DECISION-effects11-upstream.md` in Task 1. All other work happens in the Community Shaders fork under GPL.
