BUILD_TEST_DIR=build
BUILD_TEST_ASAN_DIR=build-asan
ODY_DIR=..
BUILD_TYPE=Release
CMAKE_FLAGS:=

cleanup-docker:
	./cleanup-docker.sh

clean: cleanup-docker
	rm -fr $(BUILD_TEST_DIR)
	rm -fr $(BUILD_TEST_ASAN_DIR)

local_build: clean
	mkdir -p $(BUILD_TEST_DIR)
	cd $(BUILD_TEST_DIR) && cmake -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) $(ODY_DIR) && make -j4

local_run: local_build
	$(BUILD_TEST_DIR)/sources/odyssey ./odyssey-dev.conf

fmt:
	run-clang-format/run-clang-format.py -r --clang-format-executable clang-format-9 modules sources stress test third_party

apply_fmt:
	clang-format-9 ./third_party/machinarium/sources/*.c -i && clang-format-9 ./third_party/machinarium/sources/*.h -i
	clang-format-9 ./third_party/kiwi/kiwi/*.c -i && clang-format-9 ./third_party/kiwi/kiwi/*.h -i
	clang-format-9 ./sources/*.c -i && clang-format-9 ./sources/*.h -i

build_asan:
	mkdir -p $(BUILD_TEST_ASAN_DIR)
	cd $(BUILD_TEST_ASAN_DIR) && cmake -DCMAKE_BUILD_TYPE=ASAN $(ODY_DIR) && make -j4

run_test:
	rm -fr $(BUILD_TEST_DIR) && mkdir $(BUILD_TEST_DIR) && cd $(BUILD_TEST_DIR) && cmake -DCMAKE_BUILD_TYPE=Release "$(CMAKE_FLAGS)" .. && make -j2 && cd test && ./odyssey_test
	docker-compose -f docker-compose-test.yml up --force-recreate --build

submit-cov:
	mkdir cov-build && cd cov-build
	$(COV-BIN-PATH)/cov-build --dir cov-int make -j 4 && tar czvf odyssey.tgz cov-int && curl --form token=$(COV_TOKEN) --form email=$(COV_ISSUER) --form file=@./odyssey.tgz --form version="2" --form description="scalable potgresql connection pooler"  https://scan.coverity.com/builds\?project\=yandex%2Fodyssey

