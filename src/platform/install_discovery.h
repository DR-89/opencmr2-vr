#pragma once
#include <string>
#include <vector>

// Potential installation roots only. The caller validates original game files
// and resolves ambiguity before selecting anything. Empty on non-Windows hosts.
std::vector<std::string> FindInstalledGameCandidates();
