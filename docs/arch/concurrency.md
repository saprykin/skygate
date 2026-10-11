# Concurrency and threading

The concurrency model is intentionally narrow:

- asynchronous:
  - catalog download (`QNetworkAccessManager`)
  - catalog parsing (`QThreadPool`)
  - ephemeris data manifest/asset staging, cancellation, and verification
    (`QNetworkAccessManager` plus staged file IO)
- synchronous on the UI object graph:
  - ephemeris compute
  - ephemeris data activation and engine rebuilds
  - prepared projection creation
  - render-frame construction
- scene graph update path:
  - `SkyViewportItem` copies render data behind a mutex and rebuilds
    `QSGNode` geometry in `updatePaintNode()`

Large catalog import work is backgrounded; normal scene rebuilds happen
in-process and close to the UI layer.
