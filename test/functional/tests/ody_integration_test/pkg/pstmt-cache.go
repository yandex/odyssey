package main

import (
	"context"
	"fmt"
	"io"
	"net"
	"strconv"
	"strings"
	"sync"
	"sync/atomic"
	"time"

	"github.com/jackc/pgx/v5/pgproto3"
)

// Addresses match pstmt-cache.conf.
const pstmtHost = "127.0.0.1"
const pstmtFaultPort = 15432

type pstmtClient struct {
	conn net.Conn
	wire *pgproto3.Frontend
}

type pstmtResponse struct {
	types  string
	rows   [][]string
	status byte
	codes  []string
}

func pstmtCheck(ok bool, format string, args ...any) {
	if !ok {
		panic(fmt.Sprintf(format, args...))
	}
}

func pstmtConnect(db string) *pstmtClient {
	conn, err := net.DialTimeout("tcp", net.JoinHostPort(pstmtHost, strconv.Itoa(odyPort)), 5*time.Second)
	pstmtCheck(err == nil, "connect %s: %v", db, err)
	c := &pstmtClient{conn: conn, wire: pgproto3.NewFrontend(conn, conn)}
	c.wire.Send(&pgproto3.StartupMessage{ProtocolVersion: 196608, Parameters: map[string]string{
		"user": "postgres", "database": db,
	}})
	r := c.receive(false)
	pstmtCheck(len(r.codes) == 0 && r.status == 'I', "startup %s: %+v", db, r)
	return c
}

func (c *pstmtClient) receive(flush bool) pstmtResponse {
	pstmtCheck(c.conn.SetDeadline(time.Now().Add(10*time.Second)) == nil, "set deadline")
	err := c.wire.Flush()
	pstmtCheck(err == nil, "send: %v", err)
	r := pstmtResponse{}
	for {
		msg, err := c.wire.Receive()
		pstmtCheck(err == nil, "receive after %s: %v", r.types, err)
		switch m := msg.(type) {
		case *pgproto3.ReadyForQuery:
			r.types += "Z"
			r.status = m.TxStatus
			return r
		case *pgproto3.ParseComplete:
			r.types += "1"
		case *pgproto3.BindComplete:
			r.types += "2"
		case *pgproto3.CloseComplete:
			r.types += "3"
		case *pgproto3.ParameterDescription:
			r.types += "t"
		case *pgproto3.RowDescription:
			r.types += "T"
		case *pgproto3.CommandComplete:
			r.types += "C"
		case *pgproto3.NoticeResponse:
			r.types += "N"
		case *pgproto3.ErrorResponse:
			r.types += "E"
			r.codes = append(r.codes, m.Code)
			if flush {
				return r
			}
		case *pgproto3.DataRow:
			r.types += "D"
			row := make([]string, len(m.Values))
			for i, v := range m.Values {
				row[i] = string(v)
			}
			r.rows = append(r.rows, row)
		case *pgproto3.AuthenticationOk, *pgproto3.ParameterStatus, *pgproto3.BackendKeyData:
		default:
			panic(fmt.Sprintf("unexpected backend message %T: %+v", m, m))
		}
	}
}

func (c *pstmtClient) exchange(messages ...pgproto3.FrontendMessage) pstmtResponse {
	for _, m := range messages {
		c.wire.Send(m)
	}
	return c.receive(false)
}

func (c *pstmtClient) query(sql string) pstmtResponse {
	r := c.exchange(&pgproto3.Query{String: sql})
	pstmtCheck(len(r.codes) == 0, "query %s: %+v", sql, r)
	return r
}

func (c *pstmtClient) prepare() {
	for i := range 8 {
		c.wire.Send(&pgproto3.Parse{Name: fmt.Sprintf("p%d", i), Query: fmt.Sprintf("select %d::int", i)})
	}
	r := c.exchange(&pgproto3.Sync{})
	pstmtCheck(r.types == "11111111Z", "prepare: %+v", r)
}

func (c *pstmtClient) closeStatements() {
	for i := range 8 {
		c.wire.Send(&pgproto3.Close{ObjectType: 'S', Name: fmt.Sprintf("p%d", i)})
	}
	r := c.exchange(&pgproto3.Sync{})
	pstmtCheck(r.types == "33333333Z", "close: %+v", r)
}

func (c *pstmtClient) inventory() (string, int) {
	r := c.query("select pg_backend_pid(), count(*) from pg_prepared_statements where name like 'odyssey_pstmt_%'")
	pstmtCheck(len(r.rows) == 1 && len(r.rows[0]) == 2, "inventory: %+v", r)
	n, err := strconv.Atoi(r.rows[0][1])
	pstmtCheck(err == nil, "inventory count: %v", err)
	return r.rows[0][0], n
}

func (c *pstmtClient) bind(i int) {
	r := c.exchange(&pgproto3.Bind{PreparedStatement: fmt.Sprintf("p%d", i)}, &pgproto3.Execute{}, &pgproto3.Sync{})
	pstmtCheck(r.types == "2DCZ" && len(r.rows) == 1 && r.rows[0][0] == strconv.Itoa(i), "bind %d: %+v", i, r)
}

func pstmtCacheLimit() {
	c := pstmtConnect("pstmt_single")
	defer c.conn.Close()
	c.query("begin")
	c.prepare()
	pid, n := c.inventory()
	pstmtCheck(n == 8, "active transaction count: %d", n)
	c.query("rollback")
	pid2, n := c.inventory()
	pstmtCheck(pid2 == pid && n == 2, "reset: pid %s -> %s, count %d", pid, pid2, n)
	for i := range 8 {
		c.bind(i)
	}
	for i := range 8 {
		r := c.exchange(&pgproto3.Describe{ObjectType: 'S', Name: fmt.Sprintf("p%d", i)}, &pgproto3.Sync{})
		pstmtCheck(r.types == "tTZ", "describe %d: %+v", i, r)
	}
	pid2, n = c.inventory()
	pstmtCheck(pid2 == pid && n == 2, "reuse: pid %s -> %s, count %d", pid, pid2, n)
	c.closeStatements()
	c.query("begin")
	c.prepare()
	pid, n = c.inventory()
	pstmtCheck(n == 8, "disconnect setup count: %d", n)
	c.conn.Close()
	d := pstmtConnect("pstmt_single")
	defer d.conn.Close()
	pid2, n = d.inventory()
	pstmtCheck(pid2 == pid && n == 2, "disconnect reset: pid %s -> %s, count %d", pid, pid2, n)
	pstmtCheck(d.query("select 1").status == 'I', "disconnect left an active transaction")

	a, b := pstmtConnect("pstmt_two"), pstmtConnect("pstmt_two")
	defer a.conn.Close()
	defer b.conn.Close()
	a.query("begin")
	b.query("begin")
	a.prepare()
	b.prepare()
	pa, na := a.inventory()
	pb, nb := b.inventory()
	pstmtCheck(pa != pb && na == 8 && nb == 8, "two backend setup: %s/%d %s/%d", pa, na, pb, nb)
	a.closeStatements()
	b.closeStatements()
	a.query("commit")
	b.query("commit")
	a.query("begin")
	b.query("begin")
	p1, n1 := a.inventory()
	p2, n2 := b.inventory()
	pstmtCheck(p1 != p2 && (p1 == pa || p1 == pb) && (p2 == pa || p2 == pb), "backends replaced: %s %s", p1, p2)
	pstmtCheck(n1 == 2 && n2 == 2, "two backend counts: %d %d", n1, n2)
	a.query("commit")
	b.query("commit")

	u := pstmtConnect("pstmt_unlimited")
	defer u.conn.Close()
	u.prepare()
	_, n = u.inventory()
	pstmtCheck(n == 8, "disabled limit count: %d", n)
}

func pstmtShadowParse() {
	c := pstmtConnect("pstmt_single")
	defer c.conn.Close()
	c.query("create table pstmt_cache_victim(id int)")
	r := c.exchange(&pgproto3.Parse{Name: "victim", Query: "select id from pstmt_cache_victim"}, &pgproto3.Sync{})
	pstmtCheck(r.types == "1Z", "initial parse: %+v", r)
	c.prepare()
	c.query("drop table pstmt_cache_victim")
	for _, request := range []pgproto3.FrontendMessage{
		&pgproto3.Bind{PreparedStatement: "victim"},
		&pgproto3.Describe{ObjectType: 'S', Name: "victim"},
	} {
		r = c.exchange(request, &pgproto3.Parse{Name: "skipped", Query: "select 123"}, &pgproto3.Sync{})
		pstmtCheck(r.types == "EZ" && r.status == 'I' && len(r.codes) == 1 && r.codes[0] == "42P01", "shadow error: %+v", r)
		c.query("select 1")
	}
	c.wire.Send(&pgproto3.Bind{PreparedStatement: "victim"})
	c.wire.Send(&pgproto3.Flush{})
	r = c.receive(true)
	pstmtCheck(r.types == "E" && len(r.codes) == 1 && r.codes[0] == "42P01", "Flush error: %+v", r)
	r = c.exchange(&pgproto3.Bind{PreparedStatement: "p0"}, &pgproto3.Sync{})
	pstmtCheck(r.types == "Z" && r.status == 'I', "Flush recovery: %+v", r)
	c.query("begin")
	r = c.exchange(&pgproto3.Bind{PreparedStatement: "victim"}, &pgproto3.Sync{})
	pstmtCheck(r.types == "EZ" && r.status == 'E', "transaction error: %+v", r)
	c.query("rollback")
	c.query("create table pstmt_cache_victim(id int)")
	// Both the original name and the name skipped after the error must be usable.
	r = c.exchange(&pgproto3.Bind{PreparedStatement: "victim"}, &pgproto3.Execute{},
		&pgproto3.Parse{Name: "skipped", Query: "select 123"}, &pgproto3.Sync{})
	pstmtCheck(r.types == "2C1Z", "mapping recovery: %+v", r)

	r = c.exchange(&pgproto3.Parse{Name: "notice", Query: "select 99 as " + strings.Repeat("a", 70)}, &pgproto3.Sync{})
	pstmtCheck(r.types == "N1Z", "initial notice: %+v", r)
	c.closeStatements()
	c.prepare()
	r = c.exchange(&pgproto3.Bind{PreparedStatement: "notice"}, &pgproto3.Execute{}, &pgproto3.Sync{})
	pstmtCheck(r.types == "N2DCZ" && len(r.rows) == 1 && r.rows[0][0] == "99", "shadow notice: %+v", r)

	// The failing Bind precedes a shadow Parse in the same pipeline.
	r = c.exchange(&pgproto3.Bind{PreparedStatement: "notice", Parameters: [][]byte{[]byte("extra")}},
		&pgproto3.Bind{PreparedStatement: "p0"}, &pgproto3.Execute{}, &pgproto3.Sync{})
	pstmtCheck(r.types == "EZ" && r.status == 'I' && len(r.codes) == 1 && r.codes[0] == "08P01", "skipped shadow Parse: %+v", r)
	c.bind(0)
	c.query("drop table pstmt_cache_victim")
}

// Replace one eviction Close with invalid SQL. PostgreSQL then rejects the
// exchange and ignores the remaining Close messages until Sync.
func pstmtCloseFaultProxy() (func(), *atomic.Bool) {
	pg := net.JoinHostPort(pstmtHost, strconv.Itoa(hostPort))
	listener, err := net.Listen("tcp", net.JoinHostPort(pstmtHost, strconv.Itoa(pstmtFaultPort)))
	pstmtCheck(err == nil, "fault listener: %v", err)
	var injected atomic.Bool
	var wg sync.WaitGroup
	var connections []net.Conn
	accepted := make(chan struct{})
	wg.Add(1)
	go func() {
		defer wg.Done()
		defer close(accepted)
		for {
			client, err := listener.Accept()
			if err != nil {
				return
			}
			server, err := net.DialTimeout("tcp", pg, 5*time.Second)
			if err != nil {
				client.Close()
				continue
			}
			connections = append(connections, client, server)
			wg.Add(1)
			go func() {
				defer wg.Done()
				defer client.Close()
				defer server.Close()
				client.SetDeadline(time.Now().Add(30 * time.Second))
				server.SetDeadline(time.Now().Add(30 * time.Second))
				backend := pgproto3.NewBackend(client, client)
				frontend := pgproto3.NewFrontend(server, server)
				startup, err := backend.ReceiveStartupMessage()
				if err != nil {
					return
				}
				frontend.Send(startup)
				if frontend.Flush() != nil {
					return
				}
				wg.Add(1)
				go func() {
					defer wg.Done()
					io.Copy(client, server)
					client.Close()
				}()
				for {
					msg, err := backend.Receive()
					if err != nil {
						return
					}
					if close, ok := msg.(*pgproto3.Close); ok && close.ObjectType == 'S' && injected.CompareAndSwap(false, true) {
						msg = &pgproto3.Parse{Query: "select from"}
					}
					frontend.Send(msg)
					if frontend.Flush() != nil {
						return
					}
				}
			}()
		}
	}()
	return func() {
		listener.Close()
		<-accepted
		for _, conn := range connections {
			conn.Close()
		}
		wg.Wait()
	}, &injected
}

func pstmtResetFailure() {
	stop, injected := pstmtCloseFaultProxy()
	defer stop()
	c := pstmtConnect("pstmt_fault")
	defer c.conn.Close()
	c.query("begin")
	c.prepare()
	pid, n := c.inventory()
	pstmtCheck(n == 8, "fault setup count: %d", n)
	c.closeStatements()
	c.query("commit")
	pid2, n := c.inventory()
	pstmtCheck(injected.Load() && pid2 != pid && n == 0, "failed Close reused backend: %s -> %s, count %d", pid, pid2, n)

	u := pstmtConnect("pstmt_bad_reset")
	defer u.conn.Close()
	pid, _ = u.inventory()
	pid2, _ = u.inventory()
	pstmtCheck(pid != pid2, "custom discard left a transaction on reused backend %s", pid)
}

func pstmtRun(name string, run func()) (err error) {
	defer func() {
		if r := recover(); r != nil {
			err = fmt.Errorf("%s: %v", name, r)
			fmt.Println(err)
		}
	}()
	run()
	logTestDone(name)
	return nil
}

func odyPstmtCacheTestSet(_ context.Context) error {
	for _, tc := range []struct {
		name string
		run  func()
	}{
		{"pstmt cache limit", pstmtCacheLimit},
		{"shadow Parse recovery", pstmtShadowParse},
		{"pstmt reset failure", pstmtResetFailure},
	} {
		if err := pstmtRun(tc.name, tc.run); err != nil {
			return err
		}
	}
	return nil
}
