# Running things locally

---

Here's a concise list of useful development commands from the Makefile:

### Build & Run
- `make local_build` - Clean and build locally in debug mode
- `make build_release` - Build release version
- `make build_dbg` - Build debug version
- `make build_asan` - Build with ASAN for memory debugging
- `make local_run` - Run with dev config
- `make console_run` - Run in console mode with verbose logging

### Testing
- `make functional-test` - Run functional tests
- `make stress-tests` - Run stress tests
- `make jemalloc-test` - Run tests with jemalloc
- `make ci-unittests` - Run unit tests in CI environment

### Debugging
- `make gdb` - Build debug version and run in GDB

### Formatting
- `make format` - Format code using clang-format
- `make check-format` - Check code formatting

### Packaging
- `make cpack-deb` - Build DEB package
- `make cpack-rpm` - Build RPM package
- `make install` - Install to system

### Documentation
- `make build-docs-web` - Build documentation
- `make serve-docs` - Serve docs locally

### Development Environment
- `make dev_run` - Format, build and run (all-in-one)
- `make start-dev-env-dbg` - Start debug docker environment
- `make start-dev-env-asan` - Start ASAN docker environment
