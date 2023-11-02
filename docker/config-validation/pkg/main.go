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

func check(pathToConfig string, prefix string) error {
	ctx := context.TODO()

	out, _ := exec.CommandContext(ctx, "/usr/bin/odyssey", pathToConfig, "--test").Output()
	if strOut := string(out); !strings.Contains(strOut, prefix) {
		return errors.New(strOut)
	}

	return nil
}

func testWorkers() error {
	pathToDir := pathPrefix + "/workers/valid"
	configs, _ := ioutil.ReadDir(pathToDir)

	for _, config := range configs {
		pathToConfig := pathToDir + "/" + config.Name()
		if err := check(pathToConfig, configIsValid); err != nil {
			return err
		}
	}

	return nil
}

func main() {
	if err := testWorkers(); err != nil {
		fmt.Println("error", err)
	} else {
		fmt.Println("testWorkers: Ok")
	}
}
