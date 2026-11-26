#pragma once

namespace Envoy {
namespace Tracing {

/**
 * The reasons why trace sampling may or may not be performed.
 */
enum class Reason {
  // Not sampled based on supplied request id or other reason.
  NotTraceable,
  // Not sampled due to being a health check.
  HealthCheck,
  // Sampling enabled.
  Sampling,
  // Sampling forced by the service.
  ServiceForced,
  // Sampling forced by the client.
  ClientForced,
};

/**
 * The decision regarding whether traces should be sampled, and the reason for it.
 */
struct Decision {
  Reason reason;
  bool traced;
  // True only if, in this HTTP connection manager's evaluation, the request is
  // ``Reason::NotTraceable`` because random sampling did not select it, it was not rejected by
  // client sampling, and it passed overall sampling. A tracer may then honor a sampling decision
  // propagated by the downstream instead. A request ID that a previous hop marked as not traceable
  // is evaluated the same way, since it does not record why it was not traced. False in all other
  // cases, including explicit rejections in this evaluation and when no sampling decision could be
  // computed. Only set by the HTTP connection manager.
  bool parent_sampling_allowed{false};
};

} // namespace Tracing
} // namespace Envoy
