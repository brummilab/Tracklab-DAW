// STUB (test-writer). Every function fails with kNotImplemented so that the tests are red for the right
// reason (missing behaviour). The implementer replaces this file with the real engine code.
#include "spike_common.h"

#include <ostream>

namespace spike
{

ImportResult importFile(const std::filesystem::path&)
{
    return {};
}

RenderResult renderRegion(const std::filesystem::path&, double, double, const std::filesystem::path&)
{
    return {};
}

Record12Result record12(double, const std::filesystem::path&)
{
    return {};
}

Vst3Result loadVst3(const std::filesystem::path&, std::optional<double>)
{
    return {};
}

DumpPcmResult dumpPcm(const std::filesystem::path&, const std::filesystem::path&)
{
    return {};
}

int runCli(const std::vector<std::string>&, std::ostream& out, std::ostream&)
{
    out << "{\"ok\":false,\"error\":\"" << kNotImplemented << "\"}\n";
    return 70;
}

}  // namespace spike
