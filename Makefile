BUILD_TEST_DIR=build
BUILD_REL_DIR=build
BUILD_TEST_ASAN_DIR=build
BUILD_TEST_TSAN_DIR=build

FMT_BIN:=clang-format-18
CMAKE_BIN:=cmake

SKIP_CLEANUP_DOCKER:=

CMAKE_FLAGS:=-DCC_FLAGS="-Wextra -Wstrict-aliasing"
BUILD_TYPE=Release

DEV_CONF=./config-examples/odyssey-dev.conf

ODYSSEY_BUILD_TYPE ?= build_release
ODYSSEY_TEST_CODENAME ?= noble
ODYSSEY_TEST_DEBIAN_DISTRO ?= bookworm
ODYSSEY_TEST_TARGET_PLATFORM ?= linux/$(shell uname -m)
ODYSSEY_ORACLELINUX_VERSION ?= 8
ODYSSEY_CC ?= gcc

CONCURRENCY:=1
CURRENT_USER_UID_GID:=$(shell id -u):$(shell id -g)
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
	docker run -v .:/odyssey:ro odyssey/clang-format-runner modules sources

format:
	docker build -f docker/format/Dockerfile --tag=odyssey/clang-format-runner .
	docker run --user=`stat -c "%u:%g" .` -v .:/odyssey:rw odyssey/clang-format-runner -i modules sources

build_images:
	docker build -f docker/quickstart/Dockerfile . --tag=odyssey

build_asan:
	mkdir -p $(BUILD_TEST_ASAN_DIR)
	cd $(BUILD_TEST_ASAN_DIR) && $(CMAKE_BIN) .. -DCMAKE_BUILD_TYPE=ASAN $(CMAKE_FLAGS) && make -j$(CONCURRENCY)

# setarch `uname -m` -R ./odyssey ...
build_tsan:
	mkdir -p $(BUILD_TEST_TSAN_DIR)
	cd $(BUILD_TEST_TSAN_DIR) && $(CMAKE_BIN) .. -DCMAKE_BUILD_TYPE=TSAN $(CMAKE_FLAGS) && make -j$(CONCURRENCY)

build_release:
	mkdir -p $(BUILD_REL_DIR)
	cd $(BUILD_REL_DIR) && $(CMAKE_BIN) .. -DCMAKE_BUILD_TYPE=Release $(CMAKE_FLAGS) && make -j$(CONCURRENCY)

build_relwithdbginfo:
	mkdir -p $(BUILD_REL_DIR)
	cd $(BUILD_REL_DIR) && $(CMAKE_BIN) .. -DCMAKE_BUILD_TYPE=RelWithDbgInfo && make -j$(CONCURRENCY)

build_dbg:
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

quickstart: build_images
	docker run -d \
		--rm \
		--name "odyssey" \
		-p "6432:6432" \
	 	-v ./docker/quickstart/config.conf:/etc/odyssey/odyssey.conf \
		odyssey

dev_run: format local_build console_run

start-dev-env-release:
	docker compose -f ./test/functional/docker-compose.yml down || true
	ODYSSEY_FUNCTIONAL_BUILD_TYPE=build_release \
	ODYSSEY_TEST_TARGET=dev-env \
	ODYSSEY_CC="$(ODYSSEY_CC)" \
	docker compose -f ./test/functional/docker-compose.yml up --force-recreate --build -d --remove-orphans

start-dev-env-dbg:
	docker compose -f ./test/functional/docker-compose.yml down || true
	ODYSSEY_FUNCTIONAL_BUILD_TYPE=build_dbg \
	ODYSSEY_TEST_TARGET=dev-env \
	ODYSSEY_CC="$(ODYSSEY_CC)" \
	docker compose -f ./test/functional/docker-compose.yml up --force-recreate --build -d --remove-orphans

start-dev-env-asan:
	docker compose -f ./test/functional/docker-compose.yml down || true
	ODYSSEY_FUNCTIONAL_BUILD_TYPE=build_asan \
	ODYSSEY_TEST_TARGET=dev-env \
	ODYSSEY_CC="$(ODYSSEY_CC)" \
	docker compose -f ./test/functional/docker-compose.yml up --force-recreate --build -d --remove-orphans

quickstart_test:
	docker build -f docker/quickstart/Dockerfile . --tag=odyssey
	docker compose -f ./test/quickstart/docker-compose.yml up --exit-code-from tester --force-recreate --build --remove-orphans

prometheus-legacy-test:
	docker compose -f ./test/prometheus-legacy/docker-compose.yml down || true
	ODYSSEY_PROM_BUILD_TYPE=$(ODYSSEY_BUILD_TYPE) \
	docker compose -f ./test/prometheus-legacy/docker-compose.yml up --exit-code-from odyssey --force-recreate --build --remove-orphans

soft-oom-test: build_images
	docker compose -f ./test/oom/docker-compose.yml down || true
	docker compose -f ./test/oom/docker-compose.yml up --exit-code-from runner --force-recreate --build --remove-orphans

prom-exporter-test: build_images
	docker compose -f ./test/prom-exporter/docker-compose.yml up --exit-code-from tester --force-recreate --build --remove-orphans

functional-test:
	ODYSSEY_FUNCTIONAL_BUILD_TYPE=$(ODYSSEY_BUILD_TYPE) \
	ODYSSEY_TEST_TARGET=functional-entrypoint \
	ODYSSEY_FUNCTIONAL_TESTS_SELECTOR="$(ODYSSEY_TEST_SELECTOR)" \
	ODYSSEY_CC="$(ODYSSEY_CC)" \
	docker compose -f ./test/functional/docker-compose.yml up --exit-code-from odyssey --build --remove-orphans

jemalloc-test:
	docker compose -f ./test/jeamalloc/docker-compose.yml down || true
	ODYSSEY_JEMALLOC_BUILD_TYPE=$(ODYSSEY_BUILD_TYPE) \
	docker compose -f ./test/jemalloc/docker-compose.yml up --exit-code-from runner --build --remove-orphans

stress-tests:
	docker compose -f ./test/stress/docker-compose.yml down || true

	ODYSSEY_STRESS_BUILD_TYPE=$(ODYSSEY_BUILD_TYPE) \
	ODYSSEY_STRESS_TEST_TARGET=stress-entrypoint \
	docker compose -f ./test/stress/docker-compose.yml up --exit-code-from runner --build --remove-orphans

stress-tests-dev-env:
	docker compose -f ./test/stress/docker-compose.yml down || true

	ODYSSEY_STRESS_BUILD_TYPE=build_release \
	ODYSSEY_STRESS_TEST_TARGET=dev-env \
	docker compose -f ./test/stress/docker-compose.yml up --force-recreate --build -d --remove-orphans

stress-tests-dev-env-dbg:
	docker compose -f ./test/stress/docker-compose.yml down || true

	ODYSSEY_STRESS_BUILD_TYPE=build_dbg \
	ODYSSEY_STRESS_TEST_TARGET=dev-env \
	docker compose -f ./test/stress/docker-compose.yml up --force-recreate --build -d --remove-orphans

jdbc_test: build_images
	docker compose -f ./test/drivers/jdbc/docker-compose.yml up --exit-code-from regress_test --build --remove-orphans --force-recreate

ci-unittests:
	docker build \
		--platform $(ODYSSEY_TEST_TARGET_PLATFORM) \
		-f ./test/unit/Dockerfile \
		--build-arg build_type=$(ODYSSEY_BUILD_TYPE) \
		--build-arg odyssey_cc=$(ODYSSEY_CC) \
		--tag=odyssey/unit-test-runner .
	docker run odyssey/unit-test-runner

ci-build-check-ubuntu:
	docker build \
		--platform $(ODYSSEY_TEST_TARGET_PLATFORM) \
		-f test/build-test/Dockerfile.ubuntu \
		--build-arg codename=$(ODYSSEY_TEST_CODENAME) \
		--tag=odyssey/$(ODYSSEY_TEST_CODENAME)-builder-$(ODYSSEY_TEST_TARGET_PLATFORM) .
	docker run -e ODYSSEY_BUILD_TYPE=$(ODYSSEY_BUILD_TYPE) odyssey/$(ODYSSEY_TEST_CODENAME)-builder-$(ODYSSEY_TEST_TARGET_PLATFORM)

ci-build-check-debian:
	docker build \
		--platform $(ODYSSEY_TEST_TARGET_PLATFORM) \
		-f test/build-test/Dockerfile.debian \
		--build-arg codename=$(ODYSSEY_TEST_DEBIAN_DISTRO) \
		--tag=odyssey/$(ODYSSEY_TEST_DEBIAN_DISTRO)-builder-$(ODYSSEY_TEST_TARGET_PLATFORM) .
	docker run -e ODYSSEY_BUILD_TYPE=$(ODYSSEY_BUILD_TYPE) odyssey/$(ODYSSEY_TEST_DEBIAN_DISTRO)-builder-$(ODYSSEY_TEST_TARGET_PLATFORM)

ci-build-check-fedora:
	docker build \
		-f test/build-test/Dockerfile.fedora \
		--tag=odyssey/fedora-img .

ci-build-check-oracle-linux:
	docker build \
		-f test/build-test/Dockerfile.oraclelinux \
		--build-arg version=$(ODYSSEY_ORACLELINUX_VERSION) \
		--tag=odyssey/oraclelinux-$(ODYSSEY_ORACLELINUX_VERSION)-builder .
	docker run -e ODYSSEY_BUILD_TYPE=$(ODYSSEY_BUILD_TYPE) odyssey/oraclelinux-$(ODYSSEY_ORACLELINUX_VERSION)-builder

build-docs-web:
	docker build -f docs/Dockerfile --tag=odyssey/docs-builder .
	docker run --user="$(CURRENT_USER_UID_GID)" -v .:/odyssey:rw odyssey/docs-builder

serve-docs: build-docs-web
	docker stop odyssey_docs_web || true
	docker run \
		-it --rm \
		--name odyssey_docs_web \
		-p "80:80" -p "443:443" \
		-v "${PWD}/site:/usr/share/nginx/html:ro" \
		-v "${PWD}/docs/nginx.conf:/etc/nginx/nginx.conf:ro" \
		-v "${PWD}/certificate.pem":/etc/certificate.pem:ro \
		-d nginx:alpine
