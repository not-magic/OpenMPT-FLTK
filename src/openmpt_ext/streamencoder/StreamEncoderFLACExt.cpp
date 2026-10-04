// Upstream's FLAC stream encoder with MPT_WITH_FLAC set after BuildSettings.h has been checked,
// as library mode rejects the define.

#include "openmpt/all/BuildSettings.hpp"

#define MPT_WITH_FLAC

#include "../../../openmpt/src/openmpt/streamencoder/StreamEncoderFLAC.cpp"
