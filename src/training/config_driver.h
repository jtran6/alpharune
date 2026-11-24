#pragma once
/// @file config_driver.h
/// V3 self-play and AdamW loop driven by a parsed training config.

#include "config/config.h"

namespace riftbound::training {

/// Runs every iteration in the config. Returns how many iterations finished.
int runConfigTraining(const ::riftbound::config::Config& cfg);

}  // namespace riftbound::training
