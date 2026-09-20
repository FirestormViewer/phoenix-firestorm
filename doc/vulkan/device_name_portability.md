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
