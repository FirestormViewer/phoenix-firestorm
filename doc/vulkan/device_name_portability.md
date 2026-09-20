# Vulkan device-name array portability

Reference: master 25581de447, GCC diagnostics in llvkcontext.cpp:197 and :231;
the same pattern occurs in llvkprobe.cpp:92. The installed Vulkan header declares
VkPhysicalDeviceProperties::deviceName as char[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE].

NV-00 contract: after querying physical-device properties, log the device name
and copy it into the context/probe string. Testing the array's address is always
true, so the existing fallback strings are unreachable. Read the array directly
at all three sites. This preserves the name, including an empty name, without
changing device scoring, selection, errors, or resource ownership.

NV-01/NV-03: native CPU diagnostics/facts only; no GL oracle or GPU execution,
lifetime, synchronization or fallback changes. Keep warnings-as-errors enabled.
Validation: compile both real translation units in Windows RelWithDebInfo,
search for remaining Vulkan property-name array null checks, and run hosted
Linux GCC CI. Local compilation alone does not establish Linux build success.

## X11 font name and scroll tuple warnings

The next Linux diagnostics identify an anonymous-namespace Font type colliding
with X11's global Font typedef imported through llfontgl.h. Rename only the
private cache record and its local uses to LegacyFontState. Glyph rasterization,
FreeType face ownership, atlas uploads, measurement and shutdown remain intact.
The unused-function diagnostics follow the failed Font declarations at call sites.

Both llvkscroll.cpp loops read immutable tuple elements from initializer-list
snapshots and call the existing document/page-size setters with those values.
Bind the structured bindings as const references to those tuple elements;
initializer-list storage lives through the loop. Preserve callback order and
all widget-liveness checks. No GPU state, GL oracle or runtime policy changes.
Validate by compiling the affected translation units and rerunning Linux CI,
without disabling -Werror or individual diagnostics.

## Menu activation loop indentation

At ea2eea2adc, LLVKMenu::key(Key::Activate) searches root items, enables keyboard
mode and activates the first enabled branch, returning true. If none qualifies,
it returns false after the loop. The compressed loop and trailing return on one
line trigger GCC -Wmisleading-indentation. Expand the loop and condition into
explicit blocks and put the final return on its own line. Preserve root order,
predicate evaluation, activation arguments and return behavior. NV-00/NV-01/
NV-03: formatting-only CPU control-flow clarification; no GL oracle, rendering,
GPU lifetime or synchronization change. Validate compilation without suppressing
the warning and inspect the surrounding menu code for the same pattern.
