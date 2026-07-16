#pragma once

#include <enbcore/enb/SdkContract.hpp>

#include <truth/runtime/RuntimeController.hpp>

#include <cstdint>

namespace truth::runtime {

enum class EnbParameterDiagnostic : std::uint16_t {
    None = 0,
    OutsideCallback = 1,
    NestedCallback = 2,
    InvalidName = 3,
    MissingExport = 4,
    SdkRejected = 5,
    WrongParameterContract = 6,
    NonFiniteValue = 7,
};

class EnbShaderParameterApi final : public ShaderParameterApi {
public:
    class CallbackScope final {
    public:
        CallbackScope() noexcept = default;
        ~CallbackScope() noexcept;

        CallbackScope(const CallbackScope&) = delete;
        CallbackScope& operator=(const CallbackScope&) = delete;
        CallbackScope(CallbackScope&& other) noexcept;
        CallbackScope& operator=(CallbackScope&& other) noexcept;

        [[nodiscard]] bool active() const noexcept;

    private:
        explicit CallbackScope(EnbShaderParameterApi* owner) noexcept;

        EnbShaderParameterApi* owner_{nullptr};

        friend class EnbShaderParameterApi;
    };

    explicit EnbShaderParameterApi(
        enbcore::enb::SdkExports exports = {}) noexcept;

    void SetExports(enbcore::enb::SdkExports exports) noexcept;
    [[nodiscard]] CallbackScope EnterCallback() noexcept;

    [[nodiscard]] bool GetColor4(
        std::string_view category,
        std::string_view key,
        Float4& value) noexcept override;
    [[nodiscard]] bool SetColor4(
        std::string_view category,
        std::string_view key,
        const Float4& value) noexcept override;

    [[nodiscard]] EnbParameterDiagnostic diagnostic() const noexcept;

private:
    void LeaveCallback() noexcept;

    enbcore::enb::SdkExports exports_{};
    EnbParameterDiagnostic diagnostic_{EnbParameterDiagnostic::None};
    bool in_callback_{false};
};

} // namespace truth::runtime
