package main

import (
	"context"
	"log"
	"sync"

	"github.com/jmoiron/sqlx"

	"fmt"
	"time"

	_ "github.com/lib/pq"
)

var dbname = "db1"

func prep() {
	ctx := context.TODO()

	var err error
	db, err := sqlx.ConnectContext(ctx, "postgres", fmt.Sprintf("host=localhost port=6432 user=user1 dbname=%s sslmode=disable", dbname))
	if err != nil {
		log.Fatal(err)
	}

	for j := range 10 {

		var wg sync.WaitGroup
		wg.Add(10)

		stmt, err := db.Prepare(fmt.Sprintf("INSERT INTO sh1.foo%d VALUES($1)", j))
		if err != nil {
			log.Fatal(err)
		}

		for i := range 10 {
			go func(wg *sync.WaitGroup) {
				defer wg.Done()

				_, err = stmt.Exec(i)
				if err != nil {
					log.Fatal(err)
				}
			}(&wg)
		}

		wg.Add(10)
		stmt2, err := db.Prepare("SELECT pg_sleep($1)")
		if err != nil {
			log.Fatal(err)
		}

		for range 10 {
			go func(wg *sync.WaitGroup) {
				defer wg.Done()

				_, err = stmt2.Exec(1)
				if err != nil {
					log.Fatal(err)
				}
			}(&wg)
		}

		wg.Wait()

		err = stmt.Close()
		if err != nil {
			log.Fatal(err)
		}

		err = stmt2.Close()
		if err != nil {
			log.Fatal(err)
		}
	}
	db.Close()
	log.Println("OK")
}

func main() {

	for range 10 {
		var wg sync.WaitGroup

		for range 10 {
			wg.Add(1)
			go func() {
				defer wg.Done()
				prep()
			}()
		}

		wg.Wait()
		log.Println("ITER DONE")
		time.Sleep(2 * time.Second)
	}
	log.Println("TEST OK")
}
