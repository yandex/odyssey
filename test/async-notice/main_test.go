package asynctest

import (
	"context"
	"fmt"
	"os"
	"sync"
	"testing"
	"time"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgconn"
)

func TestWaitForNotificationImmediate(t *testing.T) {
	// export DATABASE_URL='postgres://rkhapov@localhost:5432/postgres?sslmode=disable'
	dsn := os.Getenv("DATABASE_URL")
	if dsn == "" {
		t.Error("DATABASE_URL is not set")
		t.FailNow()
	}

	ctx := context.TODO()

	listenerConn, err := pgx.Connect(ctx, dsn)
	if err != nil {
		t.Fatalf("connect listener: %v", err)
	}
	defer listenerConn.Close(ctx)

	senderConn, err := pgx.Connect(ctx, dsn)
	if err != nil {
		t.Fatalf("connect sender: %v", err)
	}
	defer senderConn.Close(ctx)

	channel := fmt.Sprintf("test_notify_%d", time.Now().UnixNano())

	_, err = listenerConn.Exec(ctx, "listen "+pgx.Identifier{channel}.Sanitize())
	if err != nil {
		t.Fatalf("listen: %v", err)
	}

	receivedCh := make(chan *pgconn.Notification, 1)
	errCh := make(chan error, 1)

	readyWg := &sync.WaitGroup{}
	readyWg.Add(1)

	go func() {
		waitCtx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
		defer cancel()

		readyWg.Done()

		n, err := listenerConn.WaitForNotification(waitCtx)
		if err != nil {
			errCh <- err
			return
		}
		receivedCh <- n
	}()

	readyWg.Wait()

	payload := "hello"

	_, err = senderConn.Exec(ctx, "select pg_notify($1, $2)", channel, payload)
	if err != nil {
		t.Fatalf("notify: %v", err)
	}

	select {
	case err := <-errCh:
		t.Fatalf("wait failed: %v", err)
	case n := <-receivedCh:
		if n.Channel != channel {
			t.Fatalf("unexpected channel: got %q want %q", n.Channel, channel)
		}
		if n.Payload != payload {
			t.Fatalf("unexpected payload: got %q want %q", n.Payload, payload)
		}
	case <-time.After(2 * time.Second):
		t.Fatal("timeout waiting for notification")
	}
}

func TestMain(m *testing.M) {
	m.Run()
}
