package main

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"time"
)

const (
	workingBranch  = "master"
	testingRepoDir = "test"
	repoName       = "odyssey"
	githubAPIURL   = "https://api.github.com/repos/yandex/" + repoName + "/git/ref/heads/" + workingBranch
)

func check(err error) {
	if err != nil {
		panic(err)
	}
}

type RefResponse struct {
	Object struct {
		Sha string `json:"sha"`
	} `json:"object"`
}

func getRemoteSHA() (sha string) {
	resp, err := http.Get(githubAPIURL)
	check(err)
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	check(err)

	var gitRefResponse RefResponse
	err = json.Unmarshal(body, &gitRefResponse)
	check(err)
	remoteSha := gitRefResponse.Object.Sha
	return remoteSha
}

func git(repoPath string, args ...string) (string, error) {
	fmt.Printf("-> git %s\n", strings.Join(args, " "))

	cmd := exec.Command("git", args...)
	cmd.Dir = repoPath

	out, err := cmd.CombinedOutput()
	check(err)
	return strings.TrimSpace(string(out)), nil
}

func syncTestRepo() (string, error) {
	homeDir, err := os.UserHomeDir()
	check(err)
	testingRepoPath := filepath.Join(homeDir, testingRepoDir, repoName)

	fmt.Printf("Checking branch %s for new commits...\n", workingBranch)

	localSHA, err := git("", "rev-parse", "HEAD")
	check(err)
	remoteSHA := getRemoteSHA()
	fmt.Println(localSHA, remoteSHA)
	if _, err := git(testingRepoPath, "fetch", "origin", workingBranch); err != nil {
		return "", err
	}

	if localSHA == remoteSHA {
		return "", nil
	}
	fmt.Printf("Spotted a new commit: %.8s -> %.8s\n", localSHA, remoteSHA)

	if _, err := git(testingRepoPath, "checkout", workingBranch); err != nil {
		return "", err
	}
	if _, err := git(testingRepoPath, "reset", "--hard", remoteSHA); err != nil {
		return "", err
	}
	return remoteSHA, nil
}

func runTests() error {
	// запустить тесты ~test/odyssey
	return nil
}

func checkForUpdates() {
	sha, err := syncTestRepo()
	check(err)
	if sha == "" {
		fmt.Println("No new commits yet")
		return
	}

	fmt.Println("New commits detected, running odyssey tests...")
	if err := runTests(); err != nil {
		fmt.Printf("Tests failed: %v\n", err)
		return
	}
	fmt.Println("Tests passed")
}

func main() {
	ticker := time.NewTicker(30 * time.Second)
	defer ticker.Stop()

	checkForUpdates()
	for range ticker.C {
		fmt.Println()
		checkForUpdates()
	}
}
