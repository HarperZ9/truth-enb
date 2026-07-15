# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT EXISTS "${INSPECTOR}")
  message(FATAL_ERROR "Independent inspector is unavailable: ${INSPECTOR}")
endif()
if(NOT EXISTS "${NIF}")
  message(FATAL_ERROR "Generated atmosphere NIF is unavailable: ${NIF}")
endif()

execute_process(
  COMMAND "${INSPECTOR}" "${NIF}"
  RESULT_VARIABLE inspector_result
  OUTPUT_VARIABLE inspector_output
  ERROR_VARIABLE inspector_error
)
if(NOT inspector_result EQUAL 0)
  message(FATAL_ERROR "Independent inspector failed (${inspector_result}): ${inspector_error}")
endif()

set(required_facts
  "\tOK\tversion=Gamebryo File Format, Version 20.2.0.7\tuser=12\tstream=100\tblocks=3\tshapes=1\tnodes=1\tunknown=0"
  "\ttype=BSTriShape\tname=TruthAtmosphereDome"
  "\tvertices=2562\ttriangles=5120\tuvs=0\tuv2=0\tnormals=1\tcolors=1"
  "\tunique_positions=2562\tduplicate_position_groups=0\tuv_seam_groups=0"
  "\twinding_out=0\twinding_in=5120\twinding_planar=0"
  "\tshader=BSSkyShaderProperty\tshader_type=1\tshader_flags1=0x80000000\tshader_flags2=0x00000021"
  "\tztest=1\tzwrite=1"
  "\tsky_flags=0x00000002"
)

foreach(required_fact IN LISTS required_facts)
  string(FIND "${inspector_output}" "${required_fact}" fact_position)
  if(fact_position EQUAL -1)
    message(FATAL_ERROR
      "Independent inspector did not report required fact: ${required_fact}\n${inspector_output}"
    )
  endif()
endforeach()

message(STATUS "Independent inspector verified the Truth atmosphere NIF contract")
