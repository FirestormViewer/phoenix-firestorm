# Widget factory constructor portability

Reference: master a3ee617385, Linux CI run 35540422864, job 106156937351.
GCC rejects llvkwidgetfactory.h:253's nested `Resources resources = {}` default
argument. The parallel llui target subsequently completes; the top-level make
error is only the consequence of the earlier llvkwidgets compilation failure.

NV-00 contract: the four-argument constructor supplies a value-initialized
Resources object and default panel/editor/checkbox parameters. Resources owns
shared CPU UI defaults, callbacks and font/skin references. The constructor
moves these inputs into the factory; it does not issue GPU work or GL calls.

Replace the Resources default argument with a four-argument overload defined
out of line. Delegate to the unchanged full constructor with Resources{} after
the enclosing class is complete. Existing four- through eight-argument call
sites retain the same initialization and ownership. Keep Resources aggregate
initialization and its member defaults intact rather than adding a user-defined
Resources constructor or moving/duplicating default expressions.

NV-01/NV-03: no GL oracle, rendering, GPU lifetime or synchronization changes.
Validation: compile the real factory translation unit with MSVC and rerun the
hosted GCC 14 Linux build. The previous default-argument error must disappear;
later failures remain separate failures, not evidence of a green full build.

Local validation: the actual llvkwidgetfactory.cpp translation unit compiles with
MSVC 19.44, Windows x64, RelWithDebInfo, against the repository headers and current
prebuilt headers. Linux GCC and full viewer CI validation are pending.
