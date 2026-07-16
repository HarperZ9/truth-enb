#include <truth/runtime/EnbParameterBridge.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace truth::runtime {
namespace {

constexpr std::size_t kMaximumSdkNameBytes = 128U;

[[nodiscard]] bool CopyName(
    const std::string_view source,
    std::array<char, kMaximumSdkNameBytes>& destination) noexcept
{
    if (source.empty() || source.size() >= destination.size()
        || source.find('\0') != std::string_view::npos) {
        return false;
    }
    std::ranges::copy(source, destination.begin());
    destination[source.size()] = '\0';
    return true;
}

[[nodiscard]] bool Finite(const Float4 value) noexcept
{
    return std::isfinite(value.x)
        && std::isfinite(value.y)
        && std::isfinite(value.z)
        && std::isfinite(value.w);
}

} // namespace

EnbShaderParameterApi::CallbackScope::CallbackScope(
    EnbShaderParameterApi* const owner) noexcept:
    owner_(owner)
{}

EnbShaderParameterApi::CallbackScope::~CallbackScope() noexcept
{
    if (owner_ != nullptr) {
        owner_->LeaveCallback();
    }
}

EnbShaderParameterApi::CallbackScope::CallbackScope(
    CallbackScope&& other) noexcept:
    owner_(other.owner_)
{
    other.owner_ = nullptr;
}

EnbShaderParameterApi::CallbackScope&
EnbShaderParameterApi::CallbackScope::operator=(CallbackScope&& other) noexcept
{
    if (this == &other) {
        return *this;
    }
    if (owner_ != nullptr) {
        owner_->LeaveCallback();
    }
    owner_ = other.owner_;
    other.owner_ = nullptr;
    return *this;
}

bool EnbShaderParameterApi::CallbackScope::active() const noexcept
{
    return owner_ != nullptr;
}

EnbShaderParameterApi::EnbShaderParameterApi(
    const enbcore::enb::SdkExports exports) noexcept:
    exports_(exports)
{}

void EnbShaderParameterApi::SetExports(
    const enbcore::enb::SdkExports exports) noexcept
{
    if (in_callback_) {
        diagnostic_ = EnbParameterDiagnostic::NestedCallback;
        return;
    }
    exports_ = exports;
    diagnostic_ = EnbParameterDiagnostic::None;
}

EnbShaderParameterApi::CallbackScope
EnbShaderParameterApi::EnterCallback() noexcept
{
    if (in_callback_) {
        diagnostic_ = EnbParameterDiagnostic::NestedCallback;
        return {};
    }
    in_callback_ = true;
    diagnostic_ = EnbParameterDiagnostic::None;
    return CallbackScope{this};
}

bool EnbShaderParameterApi::GetColor4(
    const std::string_view category,
    const std::string_view key,
    Float4& value) noexcept
{
    if (!in_callback_) {
        diagnostic_ = EnbParameterDiagnostic::OutsideCallback;
        return false;
    }
    if (exports_.get_parameter == nullptr) {
        diagnostic_ = EnbParameterDiagnostic::MissingExport;
        return false;
    }
    std::array<char, kMaximumSdkNameBytes> category_buffer{};
    std::array<char, kMaximumSdkNameBytes> key_buffer{};
    if (!CopyName(category, category_buffer) || !CopyName(key, key_buffer)) {
        diagnostic_ = EnbParameterDiagnostic::InvalidName;
        return false;
    }

    enbcore::enb::Parameter parameter;
    if (exports_.get_parameter(
            nullptr,
            category_buffer.data(),
            key_buffer.data(),
            &parameter) == 0) {
        diagnostic_ = EnbParameterDiagnostic::SdkRejected;
        return false;
    }
    if (parameter.type != enbcore::enb::ParameterKind::Color4
        || parameter.size != sizeof(Float4)
        || !enbcore::enb::ValidateParameter(parameter).accepted()) {
        diagnostic_ = EnbParameterDiagnostic::WrongParameterContract;
        return false;
    }
    Float4 candidate{};
    std::memcpy(&candidate, parameter.data.data(), sizeof(candidate));
    if (!Finite(candidate)) {
        diagnostic_ = EnbParameterDiagnostic::NonFiniteValue;
        return false;
    }
    value = candidate;
    diagnostic_ = EnbParameterDiagnostic::None;
    return true;
}

bool EnbShaderParameterApi::SetColor4(
    const std::string_view category,
    const std::string_view key,
    const Float4& value) noexcept
{
    if (!in_callback_) {
        diagnostic_ = EnbParameterDiagnostic::OutsideCallback;
        return false;
    }
    if (exports_.set_parameter == nullptr) {
        diagnostic_ = EnbParameterDiagnostic::MissingExport;
        return false;
    }
    if (!Finite(value)) {
        diagnostic_ = EnbParameterDiagnostic::NonFiniteValue;
        return false;
    }
    std::array<char, kMaximumSdkNameBytes> category_buffer{};
    std::array<char, kMaximumSdkNameBytes> key_buffer{};
    if (!CopyName(category, category_buffer) || !CopyName(key, key_buffer)) {
        diagnostic_ = EnbParameterDiagnostic::InvalidName;
        return false;
    }

    enbcore::enb::Parameter parameter;
    parameter.type = enbcore::enb::ParameterKind::Color4;
    parameter.size = sizeof(Float4);
    std::memcpy(parameter.data.data(), &value, sizeof(value));
    if (!enbcore::enb::ValidateParameter(parameter).accepted()) {
        diagnostic_ = EnbParameterDiagnostic::WrongParameterContract;
        return false;
    }
    if (exports_.set_parameter(
            nullptr,
            category_buffer.data(),
            key_buffer.data(),
            &parameter) == 0) {
        diagnostic_ = EnbParameterDiagnostic::SdkRejected;
        return false;
    }
    diagnostic_ = EnbParameterDiagnostic::None;
    return true;
}

EnbParameterDiagnostic EnbShaderParameterApi::diagnostic() const noexcept
{
    return diagnostic_;
}

void EnbShaderParameterApi::LeaveCallback() noexcept
{
    in_callback_ = false;
}

} // namespace truth::runtime
