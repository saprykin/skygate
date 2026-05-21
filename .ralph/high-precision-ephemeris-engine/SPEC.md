## Scoping

Our task would be to develop a new high-precision ephemeris engine for our
application. Currently, we have only a simple implementation which lacks any
precision.

Interface of current ephemeris engine is in
`libs/skygate-ephemeris/include/skygate/ephemeris/IEphemerisEngine.hpp`. Study
it.

Current simple ephemeris engine implementation is in
`libs/skygate-ephemeris/src/engine/*.cpp`. Study them.

Your first task would be to do field research on how ephemeris calculations are
performed with high-precision. Search for astronomical standards and tables
used to implement calculations. Probably international astronomical union
publishes something. Use oracle to search. You can use up to 5 subagents in
parallel to do the research. The goal of this task is to understand how a
high-precision engine can be implemented.

Your second task would be to study source codes in `libs/*` and `apps/*` to
understand how current ephemeris engine is used and what are its capabilities
and limitations. Use up to 5 subagents in parallel to study the source codes.

Based on the outcomes of the both previous tasks, your goal is to come up with
a technical specification (product requirements document) for implementation
and integration of new high-precision ephemeris engine. After doing research
and studying source codes, come back and ask me questions to clarify the
details. Do not hesitate to make me an interview.

After we finished with clarification questions, write final specifications into
`spec/*.md` file, create specs folder if does not exist.

Initial thoughts on the new engine:

- codes should go under `libs/skygate-ephemeris/src/engine/<engine_name>`,
  while source codes for simple one should go into another subfolder;
- implementation of `IEphemerisEngine` should become a facade to a set of
  calculator classes as it is done now;
- we should define supported date ranges for engine interface such that
  date/time setup procedure can use this information for validation of inputs;
- new engine should be fully and extensively covered by tests;
- we need to find reference data sources to test with high precision;
- engine factory would be responsible for creating a requested type of engine;
- preferences should allow user to select and save the choice of which engine
  to use in the app;
- engine object must be cleanly integrated into the whole app as single point
  of truth when calculation any sky object position;
- think about how to efficiently perform calculations with high precision for
  tens of thousands of objects or more, probably engine would need to cache
  some calculations;
- we definitely want to precisely model time in past of thousand of years.
