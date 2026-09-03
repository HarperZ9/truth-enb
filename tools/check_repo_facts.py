"""Bind every number on the artwork card to the source that declares it.

The art gate settles whether the card fits its columns and matches its spec.
Whether the card is true of this preset is a different question, and this file
is where it gets answered: each row is measured again from the shader, the
header or the CMake script that declares it, and a row that no longer agrees
is a failure rather than a stale drawing nobody noticed.

Standard library only, so it runs anywhere the repository is checked out and
does not need MSVC, the Windows SDK or a graphics device.
"""
import io
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = ROOT / "docs" / "art" / "truth-enb.art.json"

WORDS = {
    1: "one", 2: "two", 3: "three", 4: "four", 5: "five", 6: "six",
    7: "seven", 8: "eight", 9: "nine", 10: "ten", 11: "eleven",
    12: "twelve", 13: "thirteen", 14: "fourteen", 15: "fifteen",
    16: "sixteen", 17: "seventeen", 18: "eighteen", 19: "nineteen",
}

QUALITY = "shaders/truth/TruthQuality.fxh"
AURORA = "include/truth/render/AuroraCurtain.hpp"
CONTROLLER = "runtime/include/truth/runtime/RuntimeController.hpp"
CAMERA = "runtime/include/truth/runtime/CameraFrame.hpp"
CONTRACT = "runtime/include/truth/runtime/RuntimeContract.hpp"
MATRIX = "cmake/CheckTruthStageMatrix.cmake"
BUILD_FILES = ("CMakeLists.txt", "runtime/CMakeLists.txt",
               "tools/sky-mesh/CMakeLists.txt")
DIGEST_DIRS = ("tests", "include", "src", "runtime")
DIGEST = re.compile(r'"[0-9a-f]{64}"')


def _read(relative: str) -> str:
    return io.open(ROOT / relative, encoding="utf-8", newline="").read()


def _word(count: int) -> str:
    """Spelled out, because the card draws words where it can."""
    if count not in WORDS:
        raise AssertionError(f"no word for {count}; widen WORDS or use digits")
    return WORDS[count]


def _enumerators(text: str, name: str) -> int:
    opener = re.search(rf"enum class {name} : [\w:]+ \{{", text)
    if opener is None:
        raise AssertionError(f"enum class {name} is gone from the source")
    start = opener.end()
    body = text[start:text.index("\n};", start)]
    return len(re.findall(r"^\s{4}\w+ = [^,]+,$", body, re.MULTILINE))


def _tier_branches(quality: str) -> list[str]:
    """The five preprocessor arms of the per-tier constant table."""
    start = quality.index("#if TRUTH_QUALITY_TIER == 0")
    body = quality[start:quality.index("\n#endif", start)]
    arms = re.split(r"^#(?:elif [^\n]*|else)$", body, flags=re.MULTILINE)
    return [arm for arm in arms if "TruthQuality" in arm]


def _knobs(arm: str) -> dict[str, int]:
    found = re.findall(r"static const uint TruthQuality(\w+) = (\d+)u;", arm)
    return {name: int(value) for name, value in found}


def _aurora_budgets(header: str) -> list[int]:
    start = header.index("AuroraSampleCount(")
    body = header[start:header.index("\n}", start)]
    return [int(n) for n in re.findall(r"return (\d+)U;", body)][:-1]


def _add_tests() -> list[str]:
    """Every declared CTest name, however the call is wrapped."""
    names = []
    for relative in BUILD_FILES:
        text = _read(relative)
        for call in re.finditer(r"add_test\(", text):
            window = text[call.end():call.end() + 240]
            name = re.search(r"NAME\s+([A-Za-z_0-9]+)", window)
            if name is None:
                raise AssertionError(f"an add_test in {relative} names nothing")
            names.append(name.group(1))
    return names


def measure() -> dict[str, str]:
    """Every card value, rebuilt from the source rather than from the card."""
    quality = _read(QUALITY)
    controller = _read(CONTROLLER)
    matrix = _read(MATRIX)
    top = _read("CMakeLists.txt")

    arms = _tier_branches(quality)
    knobs = _knobs(arms[0])
    budgets = _aurora_budgets(_read(AURORA))
    stages = len(re.findall(r'^  "\w+\.fx\|\w+"\)?$', matrix, re.MULTILINE))
    tiers = len(re.findall(r"foreach\(tier RANGE 0 (\d+)\)", matrix))
    negatives = matrix.count('expect_truth_stage_rejection("')
    keys = int(re.search(r"std::array<std::string_view, (\d+)> "
                         r"kShaderParameterKeys", controller).group(1))
    versions = [re.search(rf"{name} = (\d+\.\d+)F;", controller).group(1)
                for name in ("kProtocolVersion",
                             "kProtocolVersionWithCelestial")]
    ceiling = int(re.search(r"TRUTH_MAXIMUM_INSTRUCTION_SLOTS=(\d+)",
                            top).group(1))
    pinned = sum(len(DIGEST.findall(_read(str(p.relative_to(ROOT)))))
                 for d in DIGEST_DIRS
                 for p in sorted((ROOT / d).rglob("*"))
                 if p.suffix in (".cpp", ".hpp") and p.is_file())

    return {
        "quality tiers": f"{_word(len(arms))} of them",
        "tier knobs": f"{_word(len(knobs))} per tier",
        "aurora samples": ", ".join(str(n) for n in budgets),
        "compile matrix": f"{len(arms) * stages} compilations",
        "negative fixtures": f"{_word(negatives)} must fail",
        "shader parameters": f"{_word(keys)} float4 keys",
        "protocol versions": " and ".join(versions),
        "camera refusals": f"{_word(_enumerators(_read(CAMERA), 'CameraFrameDiagnostic'))} codes",
        "session states": f"{_word(_enumerators(controller, 'RuntimeSessionState'))} of them",
        "prepass ceiling": f"{ceiling:,} slots",
        "ctest targets": f"{len(_add_tests())} declared",
        "pinned image digest": "none in the tree" if pinned == 0
                               else f"{pinned} in the tree",
    }


def check_card_rows_match_the_source() -> list[str]:
    card = json.load(io.open(SPEC, encoding="utf-8"))["cards"][0]
    drawn = {field["key"]: field["value"] for field in card["fields"]}
    measured = measure()
    bad = []
    for key, value in sorted(measured.items()):
        if key not in drawn:
            bad.append(f"the card no longer has a {key!r} row")
        elif drawn[key] != value:
            bad.append(f"{key}: the card says {drawn[key]!r}, "
                       f"the source says {value!r}")
    for key in sorted(set(drawn) - set(measured)):
        bad.append(f"{key!r} is drawn but nothing measures it")
    return bad


def check_the_counters_are_read_not_guessed() -> list[str]:
    """A regex that matches nothing reports zero and passes every row.

    So each parser is aimed at a shape it must refuse. If somebody reformats
    an enum or renames the constant table, this fails before the card does.
    """
    bad = []
    try:
        _enumerators(_read(CAMERA), "NoSuchEnumExists")
    except AssertionError:
        pass
    else:
        bad.append("a missing enum was counted instead of refused")
    if _enumerators(_read(CONTRACT), "RuntimeFamily") != 3:
        bad.append("RuntimeFamily no longer reads as three families")
    if _enumerators(_read(CONTROLLER), "RuntimeDiagnostic") != 8:
        bad.append("RuntimeDiagnostic no longer reads as eight codes")
    if len(_add_tests()) != len(set(_add_tests())):
        bad.append("two CTest targets share a name, so the count is inflated")
    return bad


def check_the_tiers_agree_across_the_language_boundary() -> list[str]:
    """The C++ reference and the shader table draw the same sample budgets.

    The card prints one row of numbers. Two files declare them, in different
    languages, and a change to either one alone is the drift worth catching.
    """
    arms = _tier_branches(_read(QUALITY))
    bad = []
    shader = [_knobs(arm).get("AuroraSamples") for arm in arms]
    if shader != _aurora_budgets(_read(AURORA)):
        bad.append(f"the shader tiers sample {shader} and the C++ reference "
                   f"samples {_aurora_budgets(_read(AURORA))}")
    names = [sorted(_knobs(arm)) for arm in arms]
    if any(row != names[0] for row in names):
        bad.append("the tiers no longer declare the same set of constants")
    clouds = [_knobs(arm)["UsesVolumeClouds"] for arm in arms]
    if clouds != [0, 0, 1, 1, 1]:
        bad.append(f"volume clouds now switch on at {clouds}, and the drawing "
                   "says the first two tiers go without them")
    return bad


def check_the_matrix_the_flow_draws_is_the_one_cmake_runs() -> list[str]:
    """The flow names nine stages, five tiers and six refusals, by name."""
    matrix = _read(MATRIX)
    top = _read("CMakeLists.txt")
    drawn = ("full-frame-history", "object-motion", "foreign-scratch-read",
             "cross-effect-alpha-packing", "non-adaptation-scalar-owner",
             "non-adaptation-texture-previous")
    bad = []
    for case in drawn:
        if f'expect_truth_stage_rejection("{case}"' not in matrix:
            bad.append(f"the {case} fixture is gone from the matrix")
    if "foreach(tier RANGE 0 4)" not in matrix:
        bad.append("the matrix no longer sweeps tiers zero through four")
    if "TRUTH_BASELINE_INSTRUCTION_SLOTS=6499" not in top:
        bad.append("the pinned prepass baseline is no longer 6,499 slots")
    if not re.search(r"TRUTH_BASELINE_REVISION=[0-9a-f]{40}", top):
        bad.append("the prepass baseline no longer names the revision that "
                   "set it, so the ceiling has nothing to be measured against")
    return bad


def check_the_marked_row_is_still_an_honest_null() -> list[str]:
    """The one toned row says no reference digest is pinned anywhere.

    The WARP render is compared against a second run of itself. If somebody
    pins an expected digest, this fails, and the right repair is to redraw the
    card rather than to loosen the check.
    """
    renderer = _read("tests/ReferenceRendererTests.cpp")
    bad = []
    if "first_probe.sha256_hex == second_probe.sha256_hex" not in renderer:
        bad.append("the two-probe determinism comparison is gone, so the "
                   "render is no longer checked against anything at all")
    if measure()["pinned image digest"] != "none in the tree":
        bad.append("a reference digest is pinned now, and the card still "
                   "draws this row as an honest null")
    return bad


CHECKS = (
    check_card_rows_match_the_source,
    check_the_counters_are_read_not_guessed,
    check_the_tiers_agree_across_the_language_boundary,
    check_the_matrix_the_flow_draws_is_the_one_cmake_runs,
    check_the_marked_row_is_still_an_honest_null,
)


def main() -> int:
    worst = 0
    for check in CHECKS:
        failures = check()
        name = check.__name__.removeprefix("check_")
        print(("ok   " if not failures else "FAIL ") + f"facts.{name}")
        for failure in failures:
            print(f"       {failure}")
        worst = max(worst, 1 if failures else 0)
    return worst


if __name__ == "__main__":
    raise SystemExit(main())
