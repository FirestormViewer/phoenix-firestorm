# Linux launcher tests

Linux builds with `LL_TESTS=ON` register `linux_wrapper_syntax` and
`linux_wrapper_graphics_policy` with CTest. The existing CI test gate builds
`BUILD_TESTS` and runs these along with the other CTest tests. The test will "fake" the hardware detection tests. 

```sh
ctest --test-dir build-linux-x86_64 -L wrapper --output-on-failure
```

For standalone diagnosis:

```sh
python3 indra/newview/linux_tools/tests/test_wrapper.py
```

The suite runs the full production `wrapper.sh` as `firestorm` in an isolated
temporary installation, redirecting only its PCI sysfs glob to fixture devices.

Coverage includes NVIDIA vendor/display-class/driver detection, missing PCI
attributes, unbound devices, Nouveau, Intel, AMD and hybrid systems; both
Wayland detection signals; unset and empty session variables; modern and legacy
SDL overrides, their precedence, and backend lists; preservation of non-NVIDIA
settings; Mesa threading and launcher argument forwarding. 

The default assumes X11 when no Wayland or explicit SDL backend exists.
Unset and empty SDL variables both mean no preference.