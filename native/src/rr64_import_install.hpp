#pragma once
#include <filesystem>
#include <stop_token>
#include <string>

namespace rr64::mk64_import {
// Validate immutable converter output before touching an installed pack.
// Returns the catalogue digest used by online compatibility checks.
std::string validate_pack(const std::filesystem::path& pack, std::stop_token stop);

// Staging and backup are siblings on the destination volume. Retain the old
// pack in the job folder for recovery; never overwrite another backup.
void install_pack(const std::filesystem::path& pack,
    const std::filesystem::path& destination, const std::filesystem::path& backup);
}
