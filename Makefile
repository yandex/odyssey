BUILD_TARGET_DIR=build
ODY_DIR=..
BUILD_TYPE=Release

clean:
	rm -fr build

local_build: clean
	mkdir -p $(BUILD_TARGET_DIR)
	cd $(BUILD_TARGET_DIR) && cmake -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) $(ODY_DIR) && make

local_run: local_build
	$(BUILD_TARGET_DIR)/sources/odyssey ./odyssey-dev.conf

fmt:
	run-clang-format/run-clang-format.py -r --clang-format-executable clang-format-9 modules sources stress test third_party

apply_fmt:
	clang-format-9 ./third_party/machinarium/sources/*.c -i && clang-format-9 ./third_party/machinarium/sources/*.h -i
	clang-format-9 ./sources/*.c -i && clang-format-9 ./sources/*.h -i

cleanup-docker:
	./cleanup-docker.sh

run_test:
	kill -9 $(ps aux | grep odyssey | awk '{print $2}') || true
	docker-compose -f docker-compose-test.yml up --force-recreate --build
