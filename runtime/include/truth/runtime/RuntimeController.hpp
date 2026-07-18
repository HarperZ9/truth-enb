#pragma once

#include <enbcore/enb/SdkContract.hpp>

#include <truth/runtime/CameraFrame.hpp>

#include <array>
#include <cstdint>
#include <string_view>

namespace truth::runtime {

inline constexpr std::string_view kShaderCategory = "ENBEFFECT.FX";
inline constexpr std::array<std::string_view, 7> kShaderParameterKeys{
    "Truth Runtime | Inverse VP Row 0",
    "Truth Runtime | Inverse VP Row 1",
    "Truth Runtime | Inverse VP Row 2",
    "Truth Runtime | Inverse VP Row 3",
    "Truth Runtime | Camera World",
    "Truth Runtime | Celestial",
    "Truth Runtime | Status",
};
inline constexpr std::string_view kCelestialParameterKey =
    "Truth Runtime | Celestial";
inline constexpr float kProtocolVersion = 1.0F;
inline constexpr float kProtocolVersionWithCelestial = 1.1F;
inline constexpr float kDefaultEngineWorldUnitsPerAuroraUnit = 4096.0F;
inline constexpr std::uint64_t kExactFloatGenerationModulus = 16'777'216U;

[[nodiscard]] float FoldGenerationForShader(
    std::uint64_t generation) noexcept;

class ShaderParameterApi {
public:
    virtual ~ShaderParameterApi() = default;
    [[nodiscard]] virtual bool GetColor4(
        std::string_view category,
        std::string_view key,
        Float4& value) noexcept = 0;
    [[nodiscard]] virtual bool SetColor4(
        std::string_view category,
        std::string_view key,
        const Float4& value) noexcept = 0;
};

class CameraFrameProvider {
public:
    virtual ~CameraFrameProvider() = default;
    [[nodiscard]] virtual CameraFrameResult Sample() noexcept = 0;
};

enum class RuntimeSessionState : std::uint8_t {
    Cold = 0,
    AwaitingPostLoad = 1,
    Active = 2,
    SaveQuiesced = 3,
    ResetQuiesced = 4,
    Stopped = 5,
    Failed = 6,
};

enum class RuntimeDiagnostic : std::uint16_t {
    None = 0,
    InvalidCallbackForState = 1,
    BaselineCaptureFailed = 2,
    BaselineInvalid = 3,
    LiveWriteFailed = 4,
    BaselineRestoreFailed = 5,
    InvalidWorldScale = 6,
    CameraFrameRejected = 7,
};

struct RuntimeDiagnostics final {
    RuntimeSessionState state{RuntimeSessionState::Cold};
    RuntimeDiagnostic diagnostic{RuntimeDiagnostic::None};
    CameraFrameDiagnostic camera_diagnostic{CameraFrameDiagnostic::None};
    std::uint64_t callbacks_received{0};
    std::uint64_t generation{0};
    std::uint64_t parameter_get_failures{0};
    std::uint64_t parameter_set_failures{0};
    bool baseline_captured{false};
    bool baseline_restore_needed{false};
    bool frame_data_valid{false};
};

class RuntimeController final {
public:
    RuntimeController(
        ShaderParameterApi& parameters,
        CameraFrameProvider& camera,
        float engine_world_units_per_aurora_unit =
            kDefaultEngineWorldUnitsPerAuroraUnit) noexcept;

    void HandleCallback(enbcore::enb::CallbackId callback) noexcept;
    [[nodiscard]] RuntimeDiagnostics diagnostics() const noexcept;

private:
    [[nodiscard]] bool PublishPayload(
        const std::array<Float4, 7>& payload) noexcept;
    [[nodiscard]] bool RestoreBaseline() noexcept;

    ShaderParameterApi& parameters_;
    CameraFrameProvider& camera_;
    float engine_world_units_per_aurora_unit_;
    RuntimeDiagnostics diagnostics_{};
    std::array<Float4, 7> baseline_{};
};

} // namespace truth::runtime
