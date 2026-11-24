// SPDX-License-Identifier: GPL-3.0-or-later
#include "titus/pre1_amiga_audio.hpp"
#include <algorithm>
#include <stdexcept>
namespace titus::pre1_amiga {
fox_amiga::PatSong
song(const Bytes &bytes,
     const std::function<Bytes(const std::string &)> &read_sample) {
  if (bytes.size() < 600 ||
      !std::equal(bytes.begin(), bytes.begin() + 8, "prehisto"))
    throw std::runtime_error("Unsupported Prehistorik PAT header");
  // Match the original loader: reserve two zero bytes, then read the declared
  // sample extent from the external file. Do not silently replace its bytes
  // with a different waveform obtained from an IFF reinterpretation.
  auto result = fox_amiga::external_pat_song(bytes, read_sample);
  result.initial_speed = 6;
  result.dialect = fox_amiga::PatDialect::prehistorik;
  return result;
}
} // namespace titus::pre1_amiga
