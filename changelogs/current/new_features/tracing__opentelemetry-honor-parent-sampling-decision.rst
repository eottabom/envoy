Added :ref:`honor_parent_sampling_decision
<envoy_v3_api_field_config.trace.v3.OpenTelemetryConfig.honor_parent_sampling_decision>` to the
OpenTelemetry tracer. When enabled, a request that Envoy's tracing decision did not select by
``random_sampling``, and did not reject otherwise, is sampled if its parent span context (such as
``traceparent``) is sampled and the configured sampler, if any, also samples it. A parent that is
not sampled is never overridden. Requests rejected by ``overall_sampling`` or by
``client_sampling`` and requests without a valid request ID are still not sampled. The default
keeps the existing behavior.
