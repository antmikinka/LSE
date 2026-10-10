// Startup registration smoke: every self-registering archive that made it into
// this binary must have actually landed. Each of these components registers
// from a static initializer in a TU that nothing references, so a linker doing
// symbol-driven member extraction drops it silently — the build stays green and
// the registry comes up empty at run time. This is the tripwire for that class
// of failure (whole-archive/force-load/WHOLEARCHIVE link hygiene).
//
// The counts are printed so they can be diffed across platforms; the asserts
// only require non-empty, since the exact census grows with the tree.
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "lse/backend/backend.hpp"
#include "lse/communication/transport.hpp"
#include "lse/dist/transport.hpp"
#include "lse/graph/graph.hpp"
#include "lse/graph/primitive.hpp"
#include "lse/model/registry.hpp"

#include "harness.hpp"

namespace {

template <typename Names>
void print_census(const char* what, const Names& names) {
  std::cout << "[smoke] " << what << ": " << names.size();
  for (const auto& n : names) std::cout << " " << n;
  std::cout << "\n";
}

}  // namespace

LSE_TEST(cpu_backend_enumerable_by_name) {
  const std::vector<std::string> backends = lse::backend::available_backends();
  print_census("backends", backends);
  LSE_EXPECT(!backends.empty());
  LSE_EXPECT(std::find(backends.begin(), backends.end(), std::string("cpu")) !=
             backends.end());
}

LSE_TEST(kernel_primitive_census_nonzero) {
  // lse_kernels (dispatch primitives) and lse_graph (builtin primitives) both
  // register here; an empty list means at least one archive was dropped.
  const std::vector<std::string> prims = lse::graph::registered_primitives();
  std::cout << "[smoke] kernel primitives: " << prims.size() << "\n";
  LSE_EXPECT(!prims.empty());
}

LSE_TEST(comm_transports_registered) {
  // lse_communication: the tcp and unix socket transports.
  const std::vector<std::string> transports =
      lse::comm::available_transports();
  print_census("comm transports", transports);
  LSE_EXPECT(std::find(transports.begin(), transports.end(),
                       std::string("tcp")) != transports.end());
}

LSE_TEST(dist_transports_registered) {
  // lse_dist: the loopback transport at minimum.
  const std::vector<std::string> transports =
      lse::dist::available_transports();
  print_census("dist transports", transports);
  LSE_EXPECT(!transports.empty());
}

LSE_TEST(model_architectures_registered) {
  // lse_model: every model kernel registers its architecture test.
  const auto archs = lse::model::registered_architectures();
  std::cout << "[smoke] model architectures: " << archs.size() << "\n";
  LSE_EXPECT(!archs.empty());
}

LSE_TEST(device_set_factory_registered) {
  // lse_place registers graph's device-set factory from a TU with no
  // referenced symbols — the same extraction trap, one layer up.
  const bool registered = lse::graph::device_set_factory_registered();
  std::cout << "[smoke] device-set factory: "
            << (registered ? "registered" : "MISSING") << "\n";
  LSE_EXPECT(registered);
}

LSE_TEST_MAIN()
