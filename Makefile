BUILD_TEST_DIR=build
BUILD_REL_DIR=build
BUILD_TEST_ASAN_DIR=build

FMT_BIN:=clang-format-18
CMAKE_BIN:=cmake

SKIP_CLEANUP_DOCKER:=

CMAKE_FLAGS:=-DCC_FLAGS="-Wextra -Wstrict-aliasing"
BUILD_TYPE=Release

DEV_CONF=./config-examples/odyssey-dev.conf

ODYSSEY_BUILD_TYPE ?= build_release
ODYSSEY_TEST_CODENAME ?= noble
ODYSSEY_TEST_POSTGRES_VERSION ?= 17

CONCURRENCY:=1
OS:=$(shell uname -s)
ifeq ($(OS), Linux)
	CONCURRENCY:=$(shell nproc)
endif
ifeq ($(OS), Darwin)
	CONCURRENCY:=$(shell sysctl -n hw.logicalcpu')
endif

.PHONY: clean apply_fmt

clean:
	rm -fr $(BUILD_REL_DIR)
	rm -fr $(BUILD_TEST_DIR)
	rm -fr $(BUILD_TEST_ASAN_DIR)

local_build: clean
	+$(CMAKE_BIN) -B$(BUILD_TEST_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) $(CMAKE_FLAGS)
	+make -C$(BUILD_TEST_DIR) -j$(CONCURRENCY)

local_run: 
	$(BUILD_TEST_DIR)/sources/odyssey $(DEV_CONF)

console_run: 
	$(BUILD_TEST_DIR)/sources/odyssey $(DEV_CONF) --verbose --console --log_to_stdout

check-format:
	docker build -f docker/format/Dockerfile --tag=odyssey/clang-format-runner .
	docker run -v .:/odyssey:ro odyssey/clang-format-runner modules sources stress test third_party

format:
	docker build -f docker/format/Dockerfile --tag=odyssey/clang-format-runner .
	docker run --user=`stat -c "%u:%g" .` -v .:/odyssey:rw odyssey/clang-format-runner -i modules sources stress test third_party

build_asan:
	rm -rf $(BUILD_TEST_ASAN_DIR)
	mkdir -p $(BUILD_TEST_ASAN_DIR)
	cd $(BUILD_TEST_ASAN_DIR) && $(CMAKE_BIN) .. -DCMAKE_BUILD_TYPE=ASAN $(CMAKE_FLAGS) && make -j$(CONCURRENCY)

build_release:
	rm -rf $(BUILD_REL_DIR)
	mkdir -p $(BUILD_REL_DIR)
	cd $(BUILD_REL_DIR) && $(CMAKE_BIN) .. -DCMAKE_BUILD_TYPE=Release $(CMAKE_FLAGS) && make -j$(CONCURRENCY)

build_dbg:
	rm -rf $(BUILD_TEST_DIR)
	mkdir -p $(BUILD_TEST_DIR)
	cd $(BUILD_TEST_DIR) && $(CMAKE_BIN) .. -DCMAKE_BUILD_TYPE=Debug && make -j$(CONCURRENCY)

gdb: build_dbg
	gdb --args ./build/sources/odyssey $(DEV_CONF)  --verbose --console --log_to_stdout

submit-cov:
	mkdir cov-build && cd cov-build
	$(COV-BIN-PATH)/cov-build --dir cov-int make -j 4 && tar czvf odyssey.tgz cov-int && curl --form token=$(COV_TOKEN) --form email=$(COV_ISSUER) --form file=@./odyssey.tgz --form version="2" --form description="scalable potgresql connection pooler"  https://scan.coverity.com/builds\?project\=yandex%2Fodyssey

cpack-deb: build_release
	cd $(BUILD_REL_DIR) && cpack -G DEB

cpack-deb-debug: build_dbg
	cd $(BUILD_REL_DIR) && cpack -G DEB

cpack-rpm: build_release
	cd $(BUILD_REL_DIR) && cpack -G RPM

cpack-rpm-debug: build_dbg
	cd $(BUILD_REL_DIR) && cpack -G RPM

package-bionic:
	mkdir -p build
	./docker/dpkg/runner.sh -c bionic -o build

package-jammy:
	mkdir -p build
	./docker/dpkg/runner.sh -c jammy -o build

install:
	install -D build/sources/odyssey $(DESTDIR)/usr/bin/odyssey

dev_run: format local_build console_run

start-dev-env-release:
	docker compose down || true
	ODYSSEY_FUNCTIONAL_BUILD_TYPE=build_release \
	ODYSSEY_TEST_TARGET=dev-env \
	docker compose -f ./docker/functional/docker-compose.yml up --force-recreate --build -d

start-dev-env-dbg:
	docker compose down || true
	ODYSSEY_FUNCTIONAL_BUILD_TYPE=build_dbg \
	ODYSSEY_TEST_TARGET=dev-env \
	docker compose -f ./docker/functional/docker-compose.yml up --force-recreate --build -d

start-dev-env-asan:
	docker compose down || true
	ODYSSEY_FUNCTIONAL_BUILD_TYPE=build_asan \
	ODYSSEY_TEST_TARGET=dev-env \
	docker compose -f ./docker/functional/docker-compose.yml up --force-recreate --build -d

functional-test:
	ODYSSEY_FUNCTIONAL_BUILD_TYPE=$(ODYSSEY_BUILD_TYPE) \
	ODYSSEY_TEST_TARGET=functional-entrypoint \
	ODYSSEY_FUNCTIONAL_TESTS_SELECTOR="$(ODYSSEY_TEST_SELECTOR)" \
	docker compose -f ./docker/functional/docker-compose.yml up --exit-code-from odyssey --build

ci-unittests:
	docker build \
		-f ./docker/unit/Dockerfile \
		--build-arg build_type=$(ODYSSEY_BUILD_TYPE) \
		--tag=odyssey/unit-test-runner .
	docker run odyssey/unit-test-runner

ci-build-check:
	docker build \
		-f docker/build-test/Dockerfile \
		--build-arg codename=$(ODYSSEY_TEST_CODENAME) \
		--build-arg postgres_version=$(ODYSSEY_TEST_POSTGRES_VERSION) \
		--tag=odyssey/$(ODYSSEY_TEST_CODENAME)-pg$(ODYSSEY_TEST_POSTGRES_VERSION)-builder .
	docker run -e ODYSSEY_BUILD_TYPE=$(ODYSSEY_BUILD_TYPE) odyssey/$(ODYSSEY_TEST_CODENAME)-pg$(ODYSSEY_TEST_POSTGRES_VERSION)-builder

fedora-build-check:
	docker build \
		-f docker/fedora-build/Dockerfile \
		--tag=odyssey/fedora-img .
