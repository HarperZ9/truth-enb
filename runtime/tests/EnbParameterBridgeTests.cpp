#include "TestHarness.hpp"

#include <truth/runtime/EnbParameterBridge.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace {

using namespace truth::runtime;
using truth::runtime::tests::Context;

struct CallRecord final {
    bool write{false};
    bool filename_was_null{false};
    std::string category;
    std::string key;
    enbcore::enb::Parameter parameter{};
};

std::array<CallRecord, 8> calls{};
std::size_t call_count = 0;
bool sdk_accepts = true;
Float4 sdk_get_value{11.0F, 12.0F, 13.0F, 14.0F};

enbcore::enb::SdkBoolean FakeGet(
    char* filename,
    char* category,
    char* key,
    enbcore::enb::Parameter* output)
{
    CallRecord& call = calls[call_count++];
    call.write = false;
    call.filename_was_null = filename == nullptr;
    call.category = category == nullptr ? "<null>" : category;
    call.key = key == nullptr ? "<null>" : key;
    if (!sdk_accepts || output == nullptr) {
        return 0;
    }
    output->type = enbcore::enb::ParameterKind::Color4;
    output->size = 16U;
    std::memcpy(output->data.data(), &sdk_get_value, sizeof(sdk_get_value));
    call.parameter = *output;
    return 1;
}

enbcore::enb::SdkBoolean FakeSet(
    char* filename,
    char* category,
    char* key,
    enbcore::enb::Parameter* input)
{
    CallRecord& call = calls[call_count++];
    call.write = true;
    call.filename_was_null = filename == nullptr;
    call.category = category == nullptr ? "<null>" : category;
    call.key = key == nullptr ? "<null>" : key;
    if (input != nullptr) {
        call.parameter = *input;
    }
    return sdk_accepts ? 1 : 0;
}

void Reset()
{
    calls = {};
    call_count = 0;
    sdk_accepts = true;
}

[[nodiscard]] enbcore::enb::SdkExports Exports()
{
    enbcore::enb::SdkExports exports;
    exports.get_parameter = &FakeGet;
    exports.set_parameter = &FakeSet;
    return exports;
}

void ParameterIoIsImpossibleOutsideAnEnbCallback(Context& context)
{
    Reset();
    EnbShaderParameterApi api{Exports()};
    Float4 output{};
    context.expect(!api.GetColor4(kShaderCategory, kShaderParameterKeys[0], output),
        "GetParameter escaped callback scope");
    context.expect(!api.SetColor4(
            kShaderCategory, kShaderParameterKeys[0], Float4{1, 2, 3, 4}),
        "SetParameter escaped callback scope");
    context.expect(call_count == 0U,
        "ENB SDK was called outside callback scope");
    context.expect(api.diagnostic() == EnbParameterDiagnostic::OutsideCallback,
        "outside-callback attempt lacked a diagnostic");
}

void GetAndSetUseShaderUiNamesAndExactColor4Abi(Context& context)
{
    Reset();
    static_assert(sizeof(enbcore::enb::Parameter::data) == 16U);
    static_assert(enbcore::enb::kParameterPayloadBytes == 16U);
    EnbShaderParameterApi api{Exports()};
    const auto scope = api.EnterCallback();
    context.expect(scope.active(), "callback scope was not admitted");

    Float4 output{};
    context.expect(api.GetColor4(kShaderCategory, kShaderParameterKeys[2], output),
        "valid COLOR4 GetParameter failed");
    context.expect(output == sdk_get_value,
        "COLOR4 GetParameter payload was decoded incorrectly");
    const Float4 input{101.0F, 102.0F, 103.0F, 104.0F};
    context.expect(api.SetColor4(kShaderCategory, kShaderParameterKeys[4], input),
        "valid COLOR4 SetParameter failed");

    context.expect(call_count == 2U, "unexpected ENB SDK call count");
    context.expect(calls[0].filename_was_null && calls[1].filename_was_null,
        "shader parameter call used a configuration filename");
    context.expect(calls[0].category == "ENBEFFECT.FX"
            && calls[1].category == "ENBEFFECT.FX",
        "shader category was not exact");
    context.expect(calls[0].key == "Truth Runtime | Inverse VP Row 2"
            && calls[1].key == "Truth Runtime | Camera World",
        "SDK binding used an HLSL identifier instead of UIName");
    context.expect(calls[1].parameter.type
            == enbcore::enb::ParameterKind::Color4
            && calls[1].parameter.size == 16U,
        "SetParameter did not use the SDK v1002 COLOR4 ABI");
    Float4 observed{};
    std::memcpy(&observed, calls[1].parameter.data.data(), sizeof(observed));
    context.expect(observed == input, "SetParameter payload bytes were wrong");
}

void SdkContractFailuresAreRejectedWithoutOutputMutation(Context& context)
{
    Reset();
    EnbShaderParameterApi api{Exports()};
    const auto scope = api.EnterCallback();
    Float4 output{7.0F, 8.0F, 9.0F, 10.0F};
    sdk_accepts = false;
    context.expect(!api.GetColor4(kShaderCategory, kShaderParameterKeys[0], output),
        "SDK rejection was ignored");
    context.expect(output == Float4{7.0F, 8.0F, 9.0F, 10.0F},
        "rejected GetParameter mutated output");
    context.expect(api.diagnostic() == EnbParameterDiagnostic::SdkRejected,
        "SDK rejection lacked a diagnostic");
}

void CallbackScopeCannotBeNestedOrOutliveItsOwnerState(Context& context)
{
    Reset();
    EnbShaderParameterApi api{Exports()};
    {
        const auto first = api.EnterCallback();
        const auto nested = api.EnterCallback();
        context.expect(first.active() && !nested.active(),
            "nested callback scope was admitted");
        context.expect(api.diagnostic() == EnbParameterDiagnostic::NestedCallback,
            "nested callback lacked a diagnostic");
    }
    const auto later = api.EnterCallback();
    context.expect(later.active(), "callback scope was not released");
}

constexpr truth::runtime::tests::TestCase kTests[]{
    {"parameter I/O is impossible outside an ENB callback",
        &ParameterIoIsImpossibleOutsideAnEnbCallback},
    {"Get and Set use shader UI names and exact COLOR4 ABI",
        &GetAndSetUseShaderUiNamesAndExactColor4Abi},
    {"SDK contract failures reject without output mutation",
        &SdkContractFailuresAreRejectedWithoutOutputMutation},
    {"callback scope cannot be nested or leak",
        &CallbackScopeCannotBeNestedOrOutliveItsOwnerState},
};

} // namespace

int main()
{
    return truth::runtime::tests::Run(
        "Truth ENB parameter bridge cases", kTests, std::size(kTests));
}
