set(VCPKG_BUILD_TYPE release)

# Use the system-installed OpenSSL instead of building from source.
# Fedora 43 ships OpenSSL 3.5.4 with full headers at /usr/include/openssl
# and libraries at /usr/lib64/libssl.so + /usr/lib64/libcrypto.so.

set(prefix "${CURRENT_PACKAGES_DIR}")

# Write a cmake config so downstream find_package(OpenSSL) works.
file(WRITE "${prefix}/share/openssl/openssl-config.cmake" "
set(OPENSSL_FOUND TRUE)
set(OPENSSL_INCLUDE_DIR \"/usr/include\")
set(OPENSSL_VERSION \"3.5.4\")

if(NOT TARGET OpenSSL::Crypto)
    add_library(OpenSSL::Crypto SHARED IMPORTED)
    set_target_properties(OpenSSL::Crypto PROPERTIES
        IMPORTED_LOCATION \"/usr/lib64/libcrypto.so\"
        INTERFACE_INCLUDE_DIRECTORIES \"/usr/include\"
    )
endif()

if(NOT TARGET OpenSSL::SSL)
    add_library(OpenSSL::SSL SHARED IMPORTED)
    set_target_properties(OpenSSL::SSL PROPERTIES
        IMPORTED_LOCATION \"/usr/lib64/libssl.so\"
        INTERFACE_INCLUDE_DIRECTORIES \"/usr/include\"
        INTERFACE_LINK_LIBRARIES OpenSSL::Crypto
    )
endif()

set(OPENSSL_SSL_LIBRARY \"/usr/lib64/libssl.so\")
set(OPENSSL_CRYPTO_LIBRARY \"/usr/lib64/libcrypto.so\")
set(OPENSSL_LIBRARIES \"/usr/lib64/libssl.so;/usr/lib64/libcrypto.so\")
")

# Write pkg-config file for packages that use pkg-config to find OpenSSL.
file(MAKE_DIRECTORY "${prefix}/lib/pkgconfig")
file(WRITE "${prefix}/lib/pkgconfig/openssl.pc"
"Name: OpenSSL
Description: Secure Sockets Layer and cryptography libraries (system)
Version: 3.5.4
Libs: -L/usr/lib64 -lssl -lcrypto
Cflags: -I/usr/include
")
file(WRITE "${prefix}/lib/pkgconfig/libssl.pc"
"Name: libssl
Description: Secure Sockets Layer library (system)
Version: 3.5.4
Libs: -L/usr/lib64 -lssl
Cflags: -I/usr/include
Requires.private: libcrypto
")
file(WRITE "${prefix}/lib/pkgconfig/libcrypto.pc"
"Name: libcrypto
Description: OpenSSL cryptography library (system)
Version: 3.5.4
Libs: -L/usr/lib64 -lcrypto
Cflags: -I/usr/include
")

file(WRITE "${prefix}/share/openssl/copyright"
    "OpenSSL is distributed under the Apache License 2.0.\n"
    "See https://openssl.org/source/license.html\n"
)

set(VCPKG_POLICY_EMPTY_INCLUDE_FOLDER enabled)
set(VCPKG_POLICY_EMPTY_PACKAGE enabled)
