#include "bbsolver/io/schema_contract.hpp"

#include <stdexcept>
#include <string>

namespace bbsolver {

void RequireSupportedBundleSchemaVersion(const char* bundle_kind,
                                         int schema_version) {
  if (schema_version == kSupportedBundleSchemaVersion) {
    return;
  }

  throw std::runtime_error(
      std::string("Unsupported ") + bundle_kind +
      " schema_version=" + std::to_string(schema_version) +
      "; bbsolver 1.0.1 supports schema_version=" +
      std::to_string(kSupportedBundleSchemaVersion));
}

}  // namespace bbsolver
