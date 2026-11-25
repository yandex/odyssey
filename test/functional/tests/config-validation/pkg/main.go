package main

import (
	"context"
	"errors"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"time"
)

const pathPrefix = "/tests/config-validation/configs"

const configIsValid = "config is valid"

// Invalid fixtures declare their expected diagnostic on the first line.
func readExpectedError(pathToConfig string) (string, error) {
	config, err := os.ReadFile(pathToConfig)
	if err != nil {
		return "", fmt.Errorf("read expected error: %w", err)
	}
	line, _, _ := strings.Cut(string(config), "\n")
	expected, ok := strings.CutPrefix(line, "# expect-error:")
	expected = strings.TrimSpace(expected)
	if !ok || expected == "" {
		return "", errors.New("first line must contain a non-empty '# expect-error:'")
	}
	return expected, nil
}

func makeTest(ctx context.Context, binary, pathToConfig string, isValidConfig bool) error {
	var expectedError string
	if !isValidConfig {
		var err error
		expectedError, err = readExpectedError(pathToConfig)
		if err != nil {
			return err
		}
	}

	ctx, cancel := context.WithTimeout(ctx, 30*time.Second)
	defer cancel()

	out, err := exec.CommandContext(ctx, binary, pathToConfig, "--test").CombinedOutput()
	strOut := string(out)
	if ctx.Err() != nil {
		return fmt.Errorf("validation did not finish: %w\n%s", ctx.Err(), strOut)
	}

	if isValidConfig {
		if err != nil {
			return fmt.Errorf("valid config rejected: %w\n%s", err, strOut)
		}
		if !strings.Contains(strOut, configIsValid) {
			return fmt.Errorf("missing validation success message:\n%s", strOut)
		}
		return nil
	}

	if err == nil || strings.Contains(strOut, configIsValid) {
		return fmt.Errorf("invalid config accepted:\n%s", strOut)
	}
	var exitErr *exec.ExitError
	if !errors.As(err, &exitErr) || exitErr.ExitCode() != 1 {
		return fmt.Errorf("unexpected validation failure: %w\n%s", err, strOut)
	}
	if !strings.Contains(strOut, expectedError) {
		return fmt.Errorf("expected validation error %q not found:\n%s", expectedError, strOut)
	}

	return nil
}

func makeTests(binary, configsRoot, field string, isValid bool) (int, int) {
	var group string
	if isValid {
		group = "valid"
	} else {
		group = "invalid"
	}

	pathToDir := filepath.Join(configsRoot, field, group)
	configs, err := os.ReadDir(pathToDir)
	if err != nil {
		fmt.Printf("%s (ERROR): %s\n", pathToDir, err)
		return 0, 1
	}
	if len(configs) == 0 {
		fmt.Printf("%s (ERROR): no configs found\n", pathToDir)
		return 0, 1
	}

	total, failed := 0, 0
	for _, config := range configs {
		pathToConfig := filepath.Join(pathToDir, config.Name())
		if config.IsDir() {
			fmt.Printf("%s (ERROR): expected a config file, got a directory\n", pathToConfig)
			failed++
			continue
		}
		total++
		if err := makeTest(context.Background(), binary, pathToConfig, isValid); err != nil {
			fmt.Printf("%s (ERROR): %s\n", pathToConfig, err)
			failed++
		} else {
			fmt.Printf("%s: Ok\n", pathToConfig)
		}
	}
	return total, failed
}

func runTests(binary, configsRoot string) int {
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
		"auth_query_cache",
		"rules_empty",
		"group_rules",
		"watchdog",
		"duration",
	}

	total, failed := 0, 0
	for _, field := range tests {
		for _, isValid := range []bool{true, false} {
			count, failures := makeTests(binary, configsRoot, field, isValid)
			total += count
			failed += failures
		}
	}
	fmt.Printf("config-validation: %d configs checked, %d errors\n", total, failed)
	if total == 0 || failed != 0 {
		return 1
	}
	return 0
}

func main() {
	os.Exit(runTests("/usr/bin/odyssey", pathPrefix))
}
