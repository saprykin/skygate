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
  - `SkySceneModel` publishes an immutable `SkyViewportSnapshot` and keeps
    the previous one alive through `shared_ptr` for the readers that still
    hold it
  - `SkyViewportItem` adopts the newest snapshot behind a mutex and
    rebuilds `QSGNode` geometry in `updatePaintNode()`

Only the model's UI-thread rebuild writes a snapshot, and it fills the
snapshot before publishing it. The viewport reads the shared pointer under
its mutex, and the snapshot contents never change after publication, so
the scene-graph thread can keep using a snapshot while the UI thread
publishes the next one.

Large catalog import work is backgrounded; normal scene rebuilds happen
in-process and close to the UI layer.

Catalog downloads are bound to a callback context. Status and completion
callbacks are delivered only while that context is alive; destroying it stops
delivery and releases the download state. Each reply deletes itself when it
finishes, even after the context is gone.

A catalog transfer also carries an operation handle retained by the source
record that started it. The record stores the handle before startup emits a
notification or calls a status handler, and those callbacks run synchronously
on the UI thread, so a receiver can cancel from inside one. Startup also marks
its instance as the active download before the first row notification, so the
public cancel recognizes that phase even though no transfer is running yet.
Cancelling from that notification advances the revision, marks the row
canceled, and clears the public downloading and processing flags; the startup
then finds the changed revision and returns without issuing a request. A later
retry of the same source starts with a new revision and completes the
download. After a callback the startup reacquires the operation by instance ID
and revision; if the revision moved or the handle is canceled, the startup
stops. The download service checks the handle after its own status callback
and before issuing the next candidate, and the coordinator checks it before
dispatching the parser.

Cancelling marks the operation terminal before aborting the reply the handle
currently points to, so the abort is read as a cancellation and no fallback
transfer or parse starts from it. The handle aborts only its own reply, which
leaves transfers of unrelated owners on a shared network manager alone.

Releasing ownership of a source record without retiring its handles leaves
the transfer alive: callbacks keep the cancellation object referenced, and
revision checks then suppress only the publication of its result. Retirement
closes that gap by advancing the revision, aborting and releasing both
handles, and resetting pending related state, so a stale result and the work
producing it end together.

An ephemeris data update runs on the UI thread and stages each transfer
through the same event loop. The workflow owns the refreshed manifest, and
pending transfers hold raw asset pointers into it. Those pointers do not
extend the manifest's lifetime, and nothing shares ownership of it.
Continuations hold a weak handle to the workflow, so destroying the
workflow invalidates the guards; later callbacks return before they read
the manifest or other workflow state. `SkyEphemerisDataManager` keeps
owning and releasing the pending reply.

Ephemeris engine rebuilds run synchronously on the UI thread in
`SkyEphemerisSession`. The session holds the selected engine in a
`shared_ptr` and installs a replacement under a mutex. `engineHandle()`
returns an owning copy under the same mutex, so a handle taken before a
replacement stays valid for the caller's lifetime. The raw-pointer
accessor is unlocked and read-only; consumers use it on the UI thread.
