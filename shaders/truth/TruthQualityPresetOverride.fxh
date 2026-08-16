#ifndef TRUTH_QUALITY_PRESET_OVERRIDE_FXH
#define TRUTH_QUALITY_PRESET_OVERRIDE_FXH

// Default installed-tree quality override for the authored Balanced preset.
// Preset assembly may replace this file with one that defines another tier.
// Command-line /DTRUTH_QUALITY_TIER=N remains authoritative because this file
// only defines the tier while it is still absent.

#ifndef TRUTH_QUALITY_TIER
#define TRUTH_QUALITY_TIER 1
#endif

#endif  // TRUTH_QUALITY_PRESET_OVERRIDE_FXH
