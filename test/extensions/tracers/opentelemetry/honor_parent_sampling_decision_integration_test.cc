#include <memory>
#include <string>

#include "envoy/extensions/filters/network/http_connection_manager/v3/http_connection_manager.pb.h"

#include "test/integration/http_integration.h"
#include "test/test_common/utility.h"

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "fmt/format.h"
#include "gtest/gtest.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {
namespace {

constexpr absl::string_view TraceId = "0af7651916cd43dd8448eb211c80319c";
constexpr absl::string_view ParentSpanId = "b7ad6b7169203331";

// Verifies the whole path: the HTTP connection manager's tracing decision, the OpenTelemetry span
// and the traceparent propagated to the upstream. Random sampling never selects a request, so the
// outcome depends only on the parent's decision and honor_parent_sampling_decision.
class HonorParentSamplingDecisionIntegrationTest
    : public Envoy::HttpIntegrationTest,
      public testing::TestWithParam<Network::Address::IpVersion> {
public:
  HonorParentSamplingDecisionIntegrationTest()
      : HttpIntegrationTest(Http::CodecType::HTTP1, GetParam()) {}

  void initializeTracing(bool honor_parent_sampling_decision, uint32_t overall_sampling) {
    const std::string yaml_string = fmt::format(R"EOF(
  random_sampling:
    value: 0
  overall_sampling:
    value: {}
  provider:
    name: envoy.tracers.opentelemetry
    typed_config:
      "@type": type.googleapis.com/envoy.config.trace.v3.OpenTelemetryConfig
      grpc_service:
        envoy_grpc:
          cluster_name: opentelemetry_collector
        timeout: 0.250s
      service_name: "a_service_name"
      honor_parent_sampling_decision: {}
  )EOF",
                                                overall_sampling, honor_parent_sampling_decision);

    auto tracing_config =
        std::make_unique<::envoy::extensions::filters::network::http_connection_manager::v3::
                             HttpConnectionManager_Tracing>();
    TestUtility::loadFromYaml(yaml_string, *tracing_config.get());
    config_helper_.addConfigModifier(
        [&](envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
                hcm) -> void { hcm.set_allocated_tracing(tracing_config.release()); });

    initialize();
    codec_client_ = makeHttpConnection(lookupPort("http"));
  }

  // Sends a request with a parent that has the given trace flags and returns the traceparent sent
  // to the upstream.
  std::string sendRequestWithParent(absl::string_view trace_flags) {
    Http::TestRequestHeaderMapImpl request_headers{
        {":method", "GET"},
        {":path", "/test/long/url"},
        {":scheme", "http"},
        {":authority", "host"},
        {"traceparent", absl::StrCat("00-", TraceId, "-", ParentSpanId, "-", trace_flags)}};
    auto response = sendRequestAndWaitForResponse(request_headers, 0, default_response_headers_, 0);
    EXPECT_TRUE(response->complete());
    EXPECT_EQ("200", response->headers().getStatusValue());

    const auto traceparent = upstream_request_->headers().get(Http::LowerCaseString("traceparent"));
    EXPECT_EQ(1, traceparent.size());
    const std::string value(traceparent[0]->value().getStringView());
    // The trace continues with a new span.
    EXPECT_TRUE(absl::StartsWith(value, absl::StrCat("00-", TraceId, "-")));
    EXPECT_FALSE(absl::StrContains(value, ParentSpanId));
    return value;
  }
};

INSTANTIATE_TEST_SUITE_P(IpVersions, HonorParentSamplingDecisionIntegrationTest,
                         testing::ValuesIn(TestEnvironment::getIpVersionsForTest()),
                         TestUtility::ipTestParamsToString);

// By default Envoy's decision is final, so a sampled parent is propagated as not sampled.
TEST_P(HonorParentSamplingDecisionIntegrationTest, DisabledPropagatesEnvoyDecision) {
  initializeTracing(false, 100);
  EXPECT_TRUE(absl::EndsWith(sendRequestWithParent("01"), "-00"));
}

// When enabled, a sampled parent stays sampled.
TEST_P(HonorParentSamplingDecisionIntegrationTest, EnabledKeepsSampledParent) {
  initializeTracing(true, 100);
  EXPECT_TRUE(absl::EndsWith(sendRequestWithParent("01"), "-01"));
}

// When enabled, a parent that is not sampled stays not sampled.
TEST_P(HonorParentSamplingDecisionIntegrationTest, EnabledKeepsNotSampledParent) {
  initializeTracing(true, 100);
  EXPECT_TRUE(absl::EndsWith(sendRequestWithParent("00"), "-00"));
}

// Overall sampling still rejects a sampled parent when enabled.
TEST_P(HonorParentSamplingDecisionIntegrationTest, EnabledOverallSamplingRejects) {
  initializeTracing(true, 0);
  EXPECT_TRUE(absl::EndsWith(sendRequestWithParent("01"), "-00"));
}

} // namespace
} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
