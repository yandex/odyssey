BUILD_TEST_DIR=build
BUILD_TEST_ASAN_DIR=build-asan
ODY_DIR=$(PWD)
TMP_BIN:=$(ODY_DIR)/tmp

FMT_BIN:=clang-format-9
CMAKE_BIN:=cmake

SKIP_CLEANUP_DOCKER:=

CMAKE_FLAGS:=-DCC_FLAGS="-Wextra -Wstrict-aliasing" -DUSE_SCRAM=YES
BUILD_TYPE=Release

COMPILE_CONCURRENCY=8

cleanup-docker:
	./cleanup-docker.sh -s$(SKIP_CLEANUP_DOCKER)

clean: cleanup-docker
	rm -fr $(BUILD_TEST_DIR)
	rm -fr $(BUILD_TEST_ASAN_DIR)

local_build:
	$(CMAKE_BIN) -S$(ODY_DIR) -B$(BUILD_TEST_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) $(CMAKE_FLAGS)
	make -C$(BUILD_TEST_DIR) -j$(COMPILE_CONCURRENCY)

local_run: local_build
	$(BUILD_TEST_DIR)/sources/odyssey ./odyssey-dev.conf

fmtinit:
	git submodule init
	git submodule update

fmt: fmtinit
	run-clang-format/run-clang-format.py -r --clang-format-executable $(FMT_BIN) modules sources stress test third_party

apply_fmt:
	find ./ -iname '*.h' -o -iname '*.c' | xargs $(FMT_BIN) -i

build_asan:
	mkdir -p $(BUILD_TEST_ASAN_DIR)
	cd $(BUILD_TEST_ASAN_DIR) && $(CMAKE_BIN) -DCMAKE_BUILD_TYPE=ASAN $(ODY_DIR) && make -j$(COMPILE_CONCURRENCY)

run_test:
	rm -fr $(BUILD_TEST_DIR) && mkdir $(BUILD_TEST_DIR) && cd $(BUILD_TEST_DIR) && $(CMAKE_BIN) -DCMAKE_BUILD_TYPE=Release $(CMAKE_FLAGS) .. && make -j$(COMPILE_CONCURRENCY) && cd test  && ./odyssey_test
	docker-compose -f docker-compose-test.yml build odyssey
	docker-compose -f docker-compose-test.yml up --exit-code-from odyssey

submit-cov:
	mkdir cov-build && cd cov-build
	$(COV-BIN-PATH)/cov-build --dir cov-int make -j 4 && tar czvf odyssey.tgz cov-int && curl --form token=$(COV_TOKEN) --form email=$(COV_ISSUER) --form file=@./odyssey.tgz --form version="2" --form description="scalable potgresql connection pooler"  https://scan.coverity.com/builds\?project\=yandex%2Fodyssey


PGSOURCEREPO:=
PGBR:=

fetch-custom-pg:
	rm -fr $(TMP_BIN)
	mkdir $(TMP_BIN)
	git clone $(PGSOURCEREPO) $(TMP_BIN) --single-branch -b $(PGBR)

BUILD_VERSION:=
BUILD_NUM:=

build-docker-pkg:
	docker build -f ./docker-build/Dockerfile . --tag odybuild:1.0 && docker run -e VERSION=$(BUILD_VERSION) -e BUILD_NUMBER=$(BUILD_NUM) odybuild:1.0
