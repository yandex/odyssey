package main

import (
	"context"
	"fmt"
	"os"

	_ "github.com/lib/pq"
)

func main() {
	ctx := context.Background()
	if len(os.Args) == 2 && os.Args[1] == "--pstmt-cache" {
		if err := odyPstmtCacheTestSet(ctx); err != nil {
			fmt.Fprintln(os.Stderr, err)
			os.Exit(3)
		}
		return
	}

	for _, f := range []func(ctx2 context.Context) error{
		odyClientServerInteractionsTestSet,
		odyPkgSyncTestSet,
		odyShowErrsTestSet,
		odyCoresTestSet,
	} {
		err := f(ctx)
		if err != nil {
			os.Exit(3)
		}
	}

	fmt.Println("done")
}
