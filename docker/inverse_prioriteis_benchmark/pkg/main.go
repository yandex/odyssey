package main

import (
	"context"
	"fmt"
	"github.com/jackc/pgconn"
	"github.com/jackc/pgx/v4"
	"sync"
	"time"

	_ "github.com/lib/pq"
)

const clientsCount = 1000
const connectTimeoutSec = 2

func connect(successConnect *int, i int, wg *sync.WaitGroup) {
	defer wg.Done()

	userName := fmt.Sprintf("inverse_priorities_benchmark_%d", i)
	pgConString := fmt.Sprintf("postgres://%s@127.0.0.1:6432/ipb_db?sslmode=require&connect_timeout=%d",
		userName, connectTimeoutSec)
	config, err := pgx.ParseConfig(pgConString)
	if err != nil {
		fmt.Println("DCN error:", err.Error())
		return
	}

	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
	defer cancel()

	conn, err := pgx.ConnectConfig(ctx, config)
	if err != nil {
		if pgconn.Timeout(err) {
			fmt.Println("Connect timeout:", err.Error())
		} else {
			fmt.Println("Connect error:", err.Error())
		}
		return
	}

	defer conn.Close(ctx)
	*successConnect++
}

func main() {
	fmt.Printf("inverse_prioriteis_benchmark_start\n")
	successConnect := 0
	connectCount := clientsCount 
	var wg sync.WaitGroup
	wg.Add(connectCount)
	for i := 0; i < connectCount; i++ {
		client_number := i % clientsCount
		fmt.Printf("inverse_prioriteis_benchmark_connect_%d\n", client_number)
		go connect(&successConnect, client_number, &wg)
	}
	wg.Wait()
	resultPercent := (float64(successConnect) / float64(connectCount)) * 100
	fmt.Printf("inverse_prioriteis_benchmark_result: %d success connections out of %d (%.2f %%)\n", successConnect, connectCount, resultPercent)
}
