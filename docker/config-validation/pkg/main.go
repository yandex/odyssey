package main

import (
	"context"
	"errors"
	"fmt"
	"io/ioutil"
	"os/exec"
	"strings"
)

const pathPrefix = "/etc/odyssey/configs"

const configIsValid = "config is valid"
const configWithInvalidValuePass = "config with invalid value pass"

func makeTest(pathToConfig string, isValidConfig bool) error {
	ctx := context.TODO()

	out, _ := exec.CommandContext(ctx, "/usr/bin/odyssey", pathToConfig, "--test").Output()
	strOut := string(out)

	if isValidConfig && !strings.Contains(strOut, configIsValid) {
		return errors.New(strOut)
	}

	if !isValidConfig && strings.Contains(strOut, configIsValid) {
		return errors.New(configWithInvalidValuePass)
	}

	return nil
}

func makeTests(field string, isValid bool) {
	var group string
	if isValid {
		group = "valid"
	} else {
		group = "invalid"
	}

	pathToDir := pathPrefix + "/" + field + "/" + group
	configs, _ := ioutil.ReadDir(pathToDir)

	for ind, config := range configs {
		pathToConfig := pathToDir + "/" + config.Name()
		if err := makeTest(pathToConfig, isValid); err != nil {
			fmt.Printf("%s_test_%s_%d (ERROR): %s\n", field, group, ind, err)
		} else {
			fmt.Printf("%s_test_%s_%d: Ok\n", field, group, ind)
		}
	}
}

func runTests() {
	tests := []string{
		"workers",
		"resolvers",
		"coroutine_stack_size",
		"log_format",
		"unix_socket_mode",
		"listen_empty",
		"listen_tls",
		"storage_type",
		"storage_tls",
		"storage_name",
		"pool_type",
		"pool_reserve_prepared_statement",
		"pool_routing",
		"authentication",
		"auth_query",
		"rules_empty",
	}

	for _, test := range tests {
		makeTests(test, true)
		makeTests(test, false)
	}
}

func main() {
	runTests()
}
