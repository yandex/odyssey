BUILD_TEST_DIR=build
BUILD_REL_DIR=build
BUILD_TEST_ASAN_DIR=build
ODY_DIR=$(PWD)
TMP_BIN:=$(ODY_DIR)/tmp

FMT_BIN:=clang-format-18
CMAKE_BIN:=cmake

SKIP_CLEANUP_DOCKER:=

CMAKE_FLAGS:=-DCC_FLAGS="-Wextra -Wstrict-aliasing" -DUSE_SCRAM=YES
BUILD_TYPE=Release

DEV_CONF=./config-examples/odyssey-dev.conf

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
	rm -fr $(TMP_BIN)
	rm -fr $(BUILD_REL_DIR)
	rm -fr $(BUILD_TEST_DIR)
	rm -fr $(BUILD_TEST_ASAN_DIR)

local_build: clean
	+$(CMAKE_BIN) -S $(ODY_DIR) -B$(BUILD_TEST_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) $(CMAKE_FLAGS)
	+make -C$(BUILD_TEST_DIR) -j$(CONCURRENCY)

local_run: 
	$(BUILD_TEST_DIR)/sources/odyssey $(DEV_CONF)

console_run: 
	$(BUILD_TEST_DIR)/sources/odyssey $(DEV_CONF) --verbose --console --log_to_stdout

fmt:
	docker build -f docker/format/Dockerfile --tag=odyssey/clang-format-runner .
	docker run -v .:/odyssey:ro odyssey/clang-format-runner modules sources stress test third_party

format:
	docker build -f docker/format/Dockerfile --tag=odyssey/clang-format-runner .
	docker run --user=`stat -c "%u:%g" .` -v .:/odyssey:rw odyssey/clang-format-runner -i modules sources stress test third_party

apply_fmt:
	for d in sources test third_party stress modules ; do \
		find $$d -maxdepth 5 -iname '*.h' -o -iname '*.c'  | xargs -n 1 -t -P $(CONCURRENCY) $(FMT_BIN) -i ; \
	done

build_asan:
	rm -rf $(BUILD_TEST_ASAN_DIR)
	mkdir -p $(BUILD_TEST_ASAN_DIR)
	cd $(BUILD_TEST_ASAN_DIR) && $(CMAKE_BIN) -DCMAKE_BUILD_TYPE=ASAN $(ODY_DIR) $(CMAKE_FLAGS) && make -j$(CONCURRENCY)

build_release:
	rm -rf $(BUILD_REL_DIR)
	mkdir -p $(BUILD_REL_DIR)
	cd $(BUILD_REL_DIR) && $(CMAKE_BIN) -DCMAKE_BUILD_TYPE=Release $(ODY_DIR) $(CMAKE_FLAGS) && make -j$(CONCURRENCY)

build_dbg:
	rm -rf $(BUILD_TEST_DIR)
	mkdir -p $(BUILD_TEST_DIR)
	cd $(BUILD_TEST_DIR) && $(CMAKE_BIN) -DCMAKE_BUILD_TYPE=Debug -DUSE_SCRAM=YES $(ODY_DIR) && make -j$(CONCURRENCY)

gdb: build_dbg
	gdb --args ./build/sources/odyssey $(DEV_CONF)  --verbose --console --log_to_stdout

run_test:
	# change dir, test would not work with absolute path
	./cleanup-docker.sh
	ODYSSEY_TEST_BUILD_TYPE=build_release docker compose -f ./docker-compose-test.yml up --exit-code-from odyssey

run_test_asan:
	./cleanup-docker.sh
	ODYSSEY_TEST_BUILD_TYPE=build_asan docker compose -f ./docker-compose-test.yml up --exit-code-from odyssey

run_test_dbg:
	./cleanup-docker.sh
	ODYSSEY_TEST_BUILD_TYPE=build_dbg docker compose -f ./docker-compose-test.yml up --exit-code-from odyssey

submit-cov:
	mkdir cov-build && cd cov-build
	$(COV-BIN-PATH)/cov-build --dir cov-int make -j 4 && tar czvf odyssey.tgz cov-int && curl --form token=$(COV_TOKEN) --form email=$(COV_ISSUER) --form file=@./odyssey.tgz --form version="2" --form description="scalable potgresql connection pooler"  https://scan.coverity.com/builds\?project\=yandex%2Fodyssey

deb-release: build_release
	rm -rf packages
	cd $(BUILD_REL_DIR) && cpack -G DEB && cpack --config CPackSourceConfig.cmake

deb-release-docker-bionic:
	rm -rf packages
	./docker/dpkg/ubuntu.sh -c bionic -o packages -l libldap-2.4-2

deb-release-docker-jammy:
	rm -rf packages
	./docker/dpkg/ubuntu.sh -c jammy -o packages -l libldap-2.5-0

start-dev-env:
	docker compose build dev
	docker compose up -d dev
