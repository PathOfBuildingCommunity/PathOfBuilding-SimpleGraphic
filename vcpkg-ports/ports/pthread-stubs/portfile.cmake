# On Linux, all pthread functions are provided by glibc.
# pthread-stubs is only needed on systems where they aren't — we just
# emit an empty pkg-config file so downstream ports that require
# pthread-stubs find something to link against.
set(VCPKG_BUILD_TYPE release)

file(WRITE "${CURRENT_PACKAGES_DIR}/lib/pkgconfig/pthread-stubs.pc"
"Name: pthread-stubs
Description: Weak aliases for pthread functions (native pthreads available)
Version: 0.4
Libs:
Cflags:
")

file(WRITE "${CURRENT_PACKAGES_DIR}/share/pthread-stubs/copyright"
"pthread-stubs is in the public domain / X11 license.
On this system, native pthreads (glibc) are used instead.")

set(VCPKG_POLICY_EMPTY_INCLUDE_FOLDER enabled)
set(VCPKG_POLICY_EMPTY_PACKAGE enabled)
