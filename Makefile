BUILD_TARGET_DIR=build
ODY_DIR=..

clean:
	rm -fr build

local_build: clean
	mkdir -p $(BUILD_TARGET_DIR)
	cd $(BUILD_TARGET_DIR) && cmake -DCMAKE_BUILD_TYPE=Release $(ODY_DIR) && make

local_run:
	$(BUILD_TARGET_DIR)/sources/odyssey ./odyssey.conf

fmt:
	run-clang-format/run-clang-format.py -r --clang-format-executable clang-format-9 modules sources stress test third_party


cleanup-docker:
	./cleanup-docker.sh

run_test: clean cleanup-docker
	docker-compose up --force-recreate
