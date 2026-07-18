#ifndef TRUTH_HOST_CAPABILITIES_FXH
#define TRUTH_HOST_CAPABILITIES_FXH

#define TRUTH_CAPABILITY_IDENTITY 0
#define TRUTH_CAPABILITY_SPATIAL  1
#define TRUTH_CAPABILITY_BRIDGE   2
#define TRUTH_CAPABILITY_NATIVE   3

#define TRUTH_SCRATCH_NONE        0
#define TRUTH_SCRATCH_PREPASS     1
#define TRUTH_SCRATCH_DOF         2
#define TRUTH_SCRATCH_BLOOM       3
#define TRUTH_SCRATCH_ADAPTATION  4
#define TRUTH_SCRATCH_LENS        5
#define TRUTH_SCRATCH_MAIN         6
#define TRUTH_SCRATCH_POSTPASS    7
#define TRUTH_SCRATCH_SUNSPRITE   8
#define TRUTH_SCRATCH_UNDERWATER  9

#ifndef TRUTH_STAGE_CAPABILITY
#error Truth stage must declare TRUTH_STAGE_CAPABILITY before including the contract
#endif
#ifndef TRUTH_STAGE_OWNS_COLOR
#error Truth stage must declare color ownership before including the contract
#endif
#ifndef TRUTH_STAGE_OWNS_DEPTH
#error Truth stage must declare depth ownership before including the contract
#endif
#ifndef TRUTH_STAGE_OWNS_NORMAL
#error Truth stage must declare normal ownership before including the contract
#endif
#ifndef TRUTH_STAGE_OWNS_MASK
#error Truth stage must declare mask ownership before including the contract
#endif
#ifndef TRUTH_STAGE_OWNS_NATIVE_CELESTIAL_VIEW
#error Truth stage must declare native celestial/view ownership before including the contract
#endif
#ifndef TRUTH_STAGE_OWNS_PREVIOUS_SCALAR_ADAPTATION
#error Truth stage must declare previous scalar adaptation ownership before including the contract
#endif
#ifndef TRUTH_STAGE_SCRATCH_OWNER
#error Truth stage must declare current-frame scratch ownership before including the contract
#endif
#ifndef TRUTH_STAGE_SCRATCH_READ
#error Truth stage must declare every scratch read before including the contract
#endif
#ifndef TRUTH_STAGE_OWNS_FULL_FRAME_HISTORY
#error Truth stage must declare full-frame history availability before including the contract
#endif
#ifndef TRUTH_STAGE_OWNS_OBJECT_MOTION
#error Truth stage must declare object-motion availability before including the contract
#endif
#ifndef TRUTH_STAGE_TREATS_SCRATCH_AS_HISTORY
#error Truth stage must declare scratch lifetime before including the contract
#endif
#ifndef TRUTH_STAGE_CROSS_EFFECT_ALPHA_PACKING
#error Truth stage must declare alpha packing before including the contract
#endif

#if TRUTH_STAGE_CAPABILITY < TRUTH_CAPABILITY_IDENTITY || TRUTH_STAGE_CAPABILITY > TRUTH_CAPABILITY_NATIVE
#error Truth stage capability must be ordered from identity through native
#endif
#if TRUTH_STAGE_OWNS_COLOR < 0 || TRUTH_STAGE_OWNS_COLOR > 1
#error Truth stage color ownership must be boolean
#endif
#if TRUTH_STAGE_OWNS_DEPTH < 0 || TRUTH_STAGE_OWNS_DEPTH > 1
#error Truth stage depth ownership must be boolean
#endif
#if TRUTH_STAGE_OWNS_NORMAL < 0 || TRUTH_STAGE_OWNS_NORMAL > 1
#error Truth stage normal ownership must be boolean
#endif
#if TRUTH_STAGE_OWNS_MASK < 0 || TRUTH_STAGE_OWNS_MASK > 1
#error Truth stage mask ownership must be boolean
#endif
#if TRUTH_STAGE_OWNS_NATIVE_CELESTIAL_VIEW < 0 || TRUTH_STAGE_OWNS_NATIVE_CELESTIAL_VIEW > 1
#error Truth stage native celestial/view ownership must be boolean
#endif
#if TRUTH_STAGE_OWNS_PREVIOUS_SCALAR_ADAPTATION < 0 || TRUTH_STAGE_OWNS_PREVIOUS_SCALAR_ADAPTATION > 1
#error Truth stage previous scalar adaptation ownership must be boolean
#endif
#if TRUTH_STAGE_SCRATCH_OWNER < TRUTH_SCRATCH_NONE || TRUTH_STAGE_SCRATCH_OWNER > TRUTH_SCRATCH_UNDERWATER
#error Truth stage scratch owner is not a named current-frame surface
#endif
#if TRUTH_STAGE_SCRATCH_READ < TRUTH_SCRATCH_NONE || TRUTH_STAGE_SCRATCH_READ > TRUTH_SCRATCH_UNDERWATER
#error Truth stage scratch read is not a named current-frame surface
#endif
#if TRUTH_STAGE_SCRATCH_READ != TRUTH_SCRATCH_NONE && TRUTH_STAGE_SCRATCH_READ != TRUTH_STAGE_SCRATCH_OWNER
#error Truth stage reads scratch it does not own
#endif
#if TRUTH_STAGE_OWNS_FULL_FRAME_HISTORY != 0
#error Full-frame history is unavailable in the initial public release
#endif
#if TRUTH_STAGE_OWNS_OBJECT_MOTION != 0
#error Object motion vectors are unavailable in the initial public release
#endif
#if TRUTH_STAGE_TREATS_SCRATCH_AS_HISTORY != 0
#error Current-frame scratch cannot be treated as persistent history
#endif
#if TRUTH_STAGE_CROSS_EFFECT_ALPHA_PACKING != 0
#error Cross-effect alpha packing requires an explicit round-trip contract
#endif

#endif
