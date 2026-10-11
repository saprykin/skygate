# Skygate Architecture

The architecture documentation lives in `docs/arch/`, one topic per file:

- [Overview](arch/overview.md): module layout, dependency direction, and the
  runtime flow from input to rendered frame.
- [skygate-ui module](arch/ui.md): presentation objects, QML composition, and
  the catalog/settings subsystem.
- [Catalog collection lifecycle](arch/catalog-lifecycle.md): source identity,
  atomic activation, related data, persistence, migration, and recovery.
- [skygate-core module](arch/core.md): core types, the projection subsystem,
  and time abstractions.
- [skygate-ephemeris module](arch/ephemeris-engine.md): catalog and snapshot
  models, engine implementations, and the high-precision interface layer.
- [Catalog composition](arch/catalog-composition.md): schemas and containers,
  object identity, merge precedence, composition policies, and constellation
  data.
- [Result semantics](arch/result-semantics.md): coordinate meaning per engine,
  failures, supported date ranges, and thread safety.
- [Caching and performance](arch/caching.md): the in-memory caches and
  screen-space star decimation.
- [Concurrency and threading](arch/concurrency.md): what runs async and what
  stays on the UI thread.
- [Extension points](arch/extension-points.md): how to add projections,
  catalog sources and schemas, and ephemeris engines.
- [Testing](arch/testing.md): where each test suite lives and what it owns.
