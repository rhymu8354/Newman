# Newman

This is a stand-alone program which sends an e-mail using the Simple Mail
Transfer Protocol (SMTP), defined in [RFC 5321](https://tools.ietf.org/html/rfc5321).

## Usage

    Usage: Newman MAIL CERTS

    Send an e-mail using SMTP.

      MAIL   Path to file (in Electronic Mail Format, or .eml) containing
             the e-mail to send.  The e-mail should contain custom headers
             (X-SMTP-Hostname, X-SMTP-Port, X-SMTP-Username, X_SMTP-Password)
             which are stripped out before sending, and used to configure
             the SMTP client.

      CERTS  Path to file (in Privacy Enhanced Mail format, or .pem)
             containing one or more SSL certificates which the client should
             consider trusted and root certificate authorites.

## Supported platforms / recommended toolchains

This is a portable C++11 program which depends only on the C++11 compiler, the
C and C++ standard libraries, and other C++11 libraries with similar
dependencies, so it should be supported on almost any platform.  The following
are recommended toolchains for popular platforms.

* Windows -- [Visual Studio](https://www.visualstudio.com/) (Microsoft Visual C++)
* Linux -- clang or gcc
* MacOS -- Xcode (clang)

## Building

This library is not intended to stand alone.  It is intended to be included in a larger solution which uses [CMake](https://cmake.org/) to generate the build system and build applications which will link with the library.

There are two distinct steps in the build process:

1. Generation of the build system, using CMake
2. Compiling, linking, etc., using CMake-compatible toolchain

### Prerequisites

* [CMake](https://cmake.org/) version 3.8 or newer
* C++11 toolchain compatible with CMake for your development platform (e.g. [Visual Studio](https://www.visualstudio.com/) on Windows)
* [Sasl](https://github.com/rhymu8354/Sasl.git) - a library which implements
  the Simple Authentication and Security Layer protocol.
* [Smtp](https://github.com/rhymu8354/Smtp.git) - a library which implements
  the Simple Mail Transport Protocol.
* [SmtpAuth](https://github.com/rhymu8354/SmtpAuth.git) - a library which
  implements the SMTP Service Extension for Authentication, defined in
  [RFC 4954](https://tools.ietf.org/html/rfc4954).

### Build system generation

Generate the build system using [CMake](https://cmake.org/) from the solution root.  For example:

```bash
mkdir build
cd build
cmake -G "Visual Studio 15 2017" -A "x64" ..
```

### Compiling, linking, et cetera

Either use [CMake](https://cmake.org/) or your toolchain's IDE to build.
For [CMake](https://cmake.org/):

```bash
cd build
cmake --build . --config Release
```
