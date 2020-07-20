package main

import (
	"context"
	"fmt"
	"github.com/jackc/pgproto3"
	pgx "github.com/jackc/pgx/v4"
	"github.com/jmoiron/sqlx"
	_ "github.com/lib/pq"
	"os"
	"time"
)


func getConn(ctx context.Context, ){

}

func f(ctx context.Context) {
	conn, err := pgx.Connect(ctx, "host=localhost port=6432 user=postgres database=postgres")
	if err != nil {
		_, _ = fmt.Fprintf(os.Stderr, "Unable to connection to database: %v\n", err)
		return
	}
	pgConn := conn.PgConn().Conn()
	buf := make([]byte, 8192)
	buf = (&pgproto3.Query{String: "select 1"}).Encode(buf)
	buf[0] = 0x80
	buf[1] = 0x80
	buf[2] = 0x80
	buf[3] = 0x80
	_, err = pgConn.Write(buf)
}


func simpleSelect(ctx context.Context) {
	conn, err := pgx.Connect(ctx, "host=localhost port=6432 user=postgres database=postgres")
	if err != nil {
		_, _ = fmt.Fprintf(os.Stderr, "Unable to connection to database: %v\n", err)
		return
	}
	pgConn := conn.PgConn().Conn()
	buf := make([]byte, 8192)
	buf = (&pgproto3.Query{String: "select 1"}).Encode(buf)
	_, err = pgConn.Write(buf)

	p := make([]byte, 8192)
	pgConn.Read(p)

	_, _ = fmt.Fprintf(os.Stdout, "here is your out %s", buf)
}



const (
	hostname     = "localhost"
	hostPort     = 5432
	odyPort      = 6432
	username     = "postgres"
	password     = ""
	databaseName = "postgres"
)

func simpleSelectWithSQLX(ctx context.Context) {

	pgConString := fmt.Sprintf("host=%s port=%d dbname=%s sslmode=disable user=%s", hostname, hostPort, databaseName, username)

	db, err := sqlx.Connect("postgres", pgConString)
	if err != nil {
		fmt.Println(err)
		return
	}

	rows, err := db.Queryx("select False") // select * from users
	if err != nil {
		fmt.Println(err)
	}

	var isCascadeReplica bool
	parser := func(rows *sqlx.Rows) error {
		for rows.Next() {
			err := rows.Scan(&isCascadeReplica)
			if err != nil {
				return err
			}
		}
		return nil
	}

	_ = parser(rows)

	_, _ = fmt.Fprintf(os.Stdout, "here is your out %s", isCascadeReplica)
}


func showErrors(ctx context.Context) error {

	console := "console"

	pgConString := fmt.Sprintf("host=%s port=%d dbname=%s sslmode=disable user=%s", hostname, odyPort, console, username)

	db, err := sqlx.Connect("postgres", pgConString)
	if err != nil {
		return fmt.Errorf("error while connecting to postgresql %w", err)
	}

	qry := "SHOW ERRORS"

	rows, err := db.Queryx(qry)
	if err != nil {
		return fmt.Errorf("error while exec query %s: %w", qry, err)
	}

	var isCascadeReplica bool
	parser := func(rows *sqlx.Rows) error {
		for rows.Next() {
			err := rows.Scan(&isCascadeReplica)
			if err != nil {
				return err
			}
		}
		return nil
	}

	if err := parser(rows); err != nil {
		return fmt.Errorf("error while parsing %w", err)
	}

	_, err = fmt.Fprintf(os.Stdout, "here is your out %s", isCascadeReplica)
	return err
}

func main() {

	ctx := context.Background()

	//f(ctx)
	err := ensurePostgresqlRunning()
	if err != nil {
		_ = fmt.Errorf("exit because of error %w", err)
		os.Exit(3)
	}

	//simpleSelect(ctx)
	//simpleSelectWithSQLX(ctx)
	if err := showErrors(ctx); err != nil {
		_ = fmt.Errorf("exit because of error %w", err)
		os.Exit(3)
	}

	fmt.Printf("done, waiting")
	time.Sleep(100 * time.Minute)
	fmt.Println("vim-go")
}
