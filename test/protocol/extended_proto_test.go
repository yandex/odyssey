package protocol

import (
	"context"
	"fmt"
	"os"
	"testing"
	"time"

	"github.com/jackc/pgx/v5/pgconn"
	"github.com/jackc/pgx/v5/pgproto3"
)

func TestExtendedQueryProtocol(t *testing.T) {
	ctx := context.Background()
	connStr := os.Getenv("PGCONNSTR")
	if len(os.Getenv("PGCONNSTR")) == 0 {
		connStr = "postgres://postgres@odyssey:6432/postgres"
	}

	pgConn, err := pgconn.Connect(ctx, connStr)
	if err != nil {
		t.Fatalf("connect: %v", err)
	}

	hijacked, err := pgConn.Hijack()
	if err != nil {
		t.Fatalf("hijack: %v", err)
	}
	defer hijacked.Conn.Close()

	fe := hijacked.Frontend

	t.Log(">>> CREATE TABLE IF NOT EXISTS t(id int)")
	fe.SendQuery(&pgproto3.Query{
		String: "CREATE TABLE IF NOT EXISTS t(id int)",
	})
	if err := fe.Flush(); err != nil {
		t.Fatalf("flush for create table: %v", err)
	}
	if err := drainUntilReady(t, fe); err != nil {
		t.Fatalf("create table: %v", err)
	}

	// ── Parse  S_1  "SELECT * FROM t WHERE id = $1" ──
	t.Log(">>> Parse S_1")
	fe.SendParse(&pgproto3.Parse{
		Name:          "S_1",
		Query:         "SELECT * FROM t WHERE id = $1",
		ParameterOIDs: []uint32{23}, // 23 = INT4
	})

	// ── Bind  S_1 → Portal_A  (param: 1) ──
	t.Log(">>> Bind S_1 -> Portal_A (param=1)")
	fe.SendBind(&pgproto3.Bind{
		DestinationPortal:    "Portal_A",
		PreparedStatement:    "S_1",
		ParameterFormatCodes: []int16{0},
		Parameters:           [][]byte{[]byte("1")},
		ResultFormatCodes:    []int16{0},
	})

	// ── Execute  Portal_A ──
	t.Log(">>> Execute Portal_A")
	fe.SendExecute(&pgproto3.Execute{
		Portal:  "Portal_A",
		MaxRows: 0,
	})

	// ── Sync ──
	t.Log(">>> Sync #1")
	fe.SendSync(&pgproto3.Sync{})

	// ── Bind  S_1 → Portal_B  (param: 2) ──
	t.Log(">>> Bind S_1 -> Portal_B (param=2)")
	fe.SendBind(&pgproto3.Bind{
		DestinationPortal:    "Portal_B",
		PreparedStatement:    "S_1",
		ParameterFormatCodes: []int16{0},
		Parameters:           [][]byte{[]byte("2")},
		ResultFormatCodes:    []int16{0},
	})

	// Flush Parse+Bind(A)+Execute(A)+Sync+Bind(B) onto the write
	if err := fe.Flush(); err != nil {
		t.Fatalf("flush #1: %v", err)
	}

	// The client detaching can happen here
	time.Sleep(2 * time.Second)

	// ── Execute  Portal_B ──
	t.Log(">>> Execute Portal_B")
	fe.SendExecute(&pgproto3.Execute{
		Portal:  "Portal_B",
		MaxRows: 0,
	})

	// ── Bind  S_1 → Portal_C  (param: 3) ──
	t.Log(">>> Bind S_1 -> Portal_C (param=3)")
	fe.SendBind(&pgproto3.Bind{
		DestinationPortal:    "Portal_C",
		PreparedStatement:    "S_1",
		ParameterFormatCodes: []int16{0},
		Parameters:           [][]byte{[]byte("3")},
		ResultFormatCodes:    []int16{0},
	})

	t.Log(">>> Execute Portal_C")
	fe.SendExecute(&pgproto3.Execute{
		Portal:  "Portal_C",
		MaxRows: 0,
	})

	t.Log(">>> Sync #2")
	fe.SendSync(&pgproto3.Sync{})

	if err := fe.Flush(); err != nil {
		t.Fatalf("flush #2: %v", err)
	}

	t.Log("\n--- Responses for Sync #1 ---")
	if err := drainUntilReady(t, fe); err != nil {
		t.Fatalf("sync #1: %v", err)
	}

	t.Log("\n--- Responses for Sync #2 ---")
	if err := drainUntilReady(t, fe); err != nil {
		t.Fatalf("sync #2: %v", err)
	}

	buf, _ := (&pgproto3.Terminate{}).Encode(nil)
	hijacked.Conn.Write(buf)
}

func drainUntilReady(t *testing.T, fe *pgproto3.Frontend) error {
	t.Helper()
	var hadError bool

	for {
		msg, err := fe.Receive()
		if err != nil {
			return fmt.Errorf("receive: %w", err)
		}

		switch m := msg.(type) {
		case *pgproto3.ParseComplete:
			t.Log("  <- ParseComplete")

		case *pgproto3.BindComplete:
			t.Log("  <- BindComplete")

		case *pgproto3.RowDescription:
			names := make([]string, len(m.Fields))
			for i, f := range m.Fields {
				names[i] = string(f.Name)
			}
			t.Logf("  <- RowDescription: %v", names)

		case *pgproto3.DataRow:
			vals := make([]string, len(m.Values))
			for i, v := range m.Values {
				if v == nil {
					vals[i] = "NULL"
				} else {
					vals[i] = string(v)
				}
			}
			t.Logf("  <- DataRow: %v", vals)

		case *pgproto3.CommandComplete:
			t.Logf("  <- CommandComplete: %s", string(m.CommandTag))

		case *pgproto3.ErrorResponse:
			t.Errorf("  <- ERROR: %s (code=%s)", m.Message, m.Code)
			hadError = true

		case *pgproto3.ReadyForQuery:
			t.Logf("  <- ReadyForQuery (tx_status=%c)", m.TxStatus)
			if hadError {
				return fmt.Errorf("server returned error(s) before ReadyForQuery")
			}
			return nil

		default:
			t.Logf("  <- %T", msg)
		}
	}
}
