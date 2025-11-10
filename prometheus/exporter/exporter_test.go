package main

import (
	"context"
	"errors"
	"io"
	"log/slog"
	"regexp"
	"strings"
	"testing"

	sqlmock "github.com/DATA-DOG/go-sqlmock"
	"github.com/prometheus/client_golang/prometheus"
)

func TestExporterDescribeDoesNotTouchDatabase(t *testing.T) {
	logger := slog.New(slog.NewTextHandler(io.Discard, nil))

	exporter := &Exporter{
		logger: logger,
		// connector intentionally left nil — Describe must not dereference it.
	}

	expected := len(describeMetricDescs) + len(listMetricNameToDescription)

	descCh := make(chan *prometheus.Desc, expected)

	defer func() {
		if r := recover(); r != nil {
			t.Fatalf("Describe should not interact with the database or panic, got %v", r)
		}
	}()

	exporter.Describe(descCh)
	close(descCh)

	var got []*prometheus.Desc
	for desc := range descCh {
		if desc == nil {
			t.Fatal("Describe emitted nil descriptor")
		}
		got = append(got, desc)
	}

	if len(got) != expected {
		t.Fatalf("unexpected descriptor count, got %d, want %d", len(got), expected)
	}
}

func TestSendPoolsExtendedMetricsHandlesByteColumns(t *testing.T) {
	logger := slog.New(slog.NewTextHandler(io.Discard, nil))
	exporter := &Exporter{
		logger: logger,
	}

	db, mock, err := sqlmock.New()
	if err != nil {
		t.Fatalf("failed to create sqlmock: %v", err)
	}
	defer db.Close()

	rows := sqlmock.
		NewRows([]string{"database", "user", "cl_active", "sv_active", "sv_idle"}).
		AddRow([]byte("db1"), []byte("user1"), 1, 2, 3)

	mock.ExpectQuery(regexp.QuoteMeta(showPoolsExtendedCommand)).WillReturnRows(rows)

	ch := make(chan prometheus.Metric, 32)
	err = exporter.sendPoolsExtendedMetrics(context.Background(), ch, db, map[routeKey]float64{})
	if err != nil {
		t.Fatalf("sendPoolsExtendedMetrics returned error: %v", err)
	}

	close(ch)
	count := 0
	for range ch {
		count++
	}

	if count == 0 {
		t.Fatalf("expected at least one metric, got %d", count)
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Fatalf("unmet expectations: %v", err)
	}
}

func TestCollectWithDBContinuesAfterStepFailure(t *testing.T) {
	logger := slog.New(slog.NewTextHandler(io.Discard, nil))
	exporter := &Exporter{
		logger: logger,
	}

	db, mock, err := sqlmock.New()
	if err != nil {
		t.Fatalf("failed to create sqlmock: %v", err)
	}
	defer db.Close()

	mock.ExpectQuery(regexp.QuoteMeta(showVersionCommand)).
		WillReturnRows(sqlmock.NewRows([]string{"version"}).AddRow("v1.0"))

	mock.ExpectQuery(regexp.QuoteMeta(showListsCommand)).
		WillReturnError(errors.New("boom lists"))

	mock.ExpectQuery(regexp.QuoteMeta(showIsPausedCommand)).
		WillReturnRows(sqlmock.NewRows([]string{"is_paused"}).AddRow(false))

	mock.ExpectQuery(regexp.QuoteMeta(showErrorsCommand)).
		WillReturnRows(sqlmock.NewRows([]string{"error_type", "count"}))

	mock.ExpectQuery(regexp.QuoteMeta(showStatsCommand)).
		WillReturnRows(sqlmock.NewRows([]string{"database", "avg_xact_count", "avg_query_count"}).
			AddRow("db1", 1, 2))

	mock.ExpectQuery(regexp.QuoteMeta(showDatabasesCommand)).
		WillReturnRows(sqlmock.NewRows([]string{"name", "pool_size"}).AddRow("db1", "4"))

	mock.ExpectQuery(regexp.QuoteMeta(showPoolsExtendedCommand)).
		WillReturnRows(sqlmock.NewRows([]string{"database", "user", "cl_active"}).AddRow("db1", "user1", 1))

	ch := make(chan prometheus.Metric, 64)
	err = exporter.collectWithDB(context.Background(), ch, db)
	if err == nil {
		t.Fatalf("expected collectWithDB to return error when a step fails")
	}

	close(ch)

	foundIsPaused := false
	for metric := range ch {
		if metric.Desc().String() == isPausedDescription.String() {
			foundIsPaused = true
		}
	}

	if !foundIsPaused {
		t.Fatalf("expected is_paused metric even when earlier step failed")
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Fatalf("unmet expectations: %v", err)
	}
}

func TestSendListsMetricsReturnsRowsErr(t *testing.T) {
	logger := slog.New(slog.NewTextHandler(io.Discard, nil))
	exporter := &Exporter{
		logger: logger,
	}

	db, mock, err := sqlmock.New()
	if err != nil {
		t.Fatalf("failed to create sqlmock: %v", err)
	}
	defer db.Close()

	rows := sqlmock.NewRows([]string{"list", "items"}).
		AddRow("databases", "1").
		AddRow("users", "2").
		RowError(1, errors.New("cursor error"))

	mock.ExpectQuery(regexp.QuoteMeta(showListsCommand)).WillReturnRows(rows)

	metricCh := make(chan prometheus.Metric, len(listMetricNameToDescription))
	err = exporter.sendListsMetrics(context.Background(), metricCh, db)
	if err == nil || !strings.Contains(err.Error(), "cursor error") {
		t.Fatalf("expected cursor error, got %v", err)
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Fatalf("unmet expectations: %v", err)
	}
}
func TestCollectWithDBRespectsContextCancellation(t *testing.T) {
	logger := slog.New(slog.NewTextHandler(io.Discard, nil))
	exporter := &Exporter{
		logger: logger,
	}

	db, mock, err := sqlmock.New()
	if err != nil {
		t.Fatalf("failed to create sqlmock: %v", err)
	}
	defer db.Close()

	for _, query := range []string{
		showVersionCommand,
		showListsCommand,
		showIsPausedCommand,
		showErrorsCommand,
		showStatsCommand,
		showDatabasesCommand,
		showPoolsExtendedCommand,
	} {
		mock.ExpectQuery(regexp.QuoteMeta(query)).
			WillReturnError(context.DeadlineExceeded)
	}

	err = exporter.collectWithDB(context.Background(), make(chan prometheus.Metric, 1), db)
	if err == nil {
		t.Fatalf("expected error when database returns deadline exceeded")
	}

	if !errors.Is(err, context.DeadlineExceeded) {
		t.Fatalf("expected context.DeadlineExceeded error, got %v", err)
	}

	if err := mock.ExpectationsWereMet(); err != nil {
		t.Fatalf("unmet expectations: %v", err)
	}
}
