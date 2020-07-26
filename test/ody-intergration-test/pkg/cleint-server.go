
package main

import (
	"context"
	"fmt"
	"sync"
	"time"
)


func selectSleepNoWait(ctx context.Context, i int) error {
	db, err := getConn(ctx, databaseName)
	if err != nil {
		return err
	}

	qry := fmt.Sprintf("SELECT pg_sleep(%d)", i)

	fmt.Printf("selectSleep: doing query %s\n", qry)

	_, err = db.QueryContext(ctx, qry)
	if err != nil {
		return err
	}

	fmt.Print("select sleep OK\n")
	return err
}

func selectSleep(ctx context.Context, i int, ch chan error, wg *sync.WaitGroup, before_restart bool) {
	defer wg.Done()
	db, err := getConn(ctx, databaseName)
	if err != nil {

		if before_restart {
			err = fmt.Errorf("before restart coroutine failed %w", err)
		} else {
			err = fmt.Errorf("after restart coroutine failed %w", err)
		}

		ch <- err
		fmt.Println(err)
		return
	}

	qry := fmt.Sprintf("SELECT pg_sleep(%d)", i)

	fmt.Printf("selectSleep: doing query %s\n", qry)

	_, err = db.QueryContext(ctx, qry)
	if err != nil {
		if before_restart {
			err = fmt.Errorf("before restart coroutine failed %w", err)
		} else {
			err = fmt.Errorf("after restart coroutine failed %w", err)
		}
		fmt.Println("sleep coroutine fail time: %s", time.Now().Format(time.RFC3339Nano))
	}
	ch <- err
	if err == nil {
		fmt.Print("select sleep OK\n")
	}
}

const (
	sleepInterval = 30
	maxCoroutineFailOk = 4
)


func waitOnChan (ch chan error, maxOK int) error {
	failedCnt := 0
	for {
		select {
		case err, ok := <-ch:
			if !ok {
				if failedCnt > maxOK {
					return fmt.Errorf("too many coroutines failed")
				}
				return nil
			}
			if err != nil {
				fmt.Println(err)
				failedCnt++
			}
		}
	}
}

func onlineRestartTest(ctx context.Context) error {

	err := ensurePostgresqlRunning(ctx)
	if err != nil {
		return err
	}

	err = ensureOdysseyRunning(ctx)
	if err != nil {
		return err
	}

	coroutineSleepCnt := 10

	ch := make(chan error, coroutineSleepCnt*2+1)
	wg := sync.WaitGroup{}
	{
		for i := 0; i < coroutineSleepCnt; i++ {
			wg.Add(1)
			go selectSleep(ctx, sleepInterval, ch, &wg, true)
		}

		restartOdyssey(ctx)

		for i := 0; i < coroutineSleepCnt; i++ {
			wg.Add(1)
			go selectSleep(ctx, sleepInterval, ch, &wg, false)
		}
	}
	wg.Wait()
	fmt.Println("onlineRestartTest: wait done, closing channel")
	close(ch)
	if err := waitOnChan(ch, 4); err != nil {
		fmt.Println(fmt.Errorf("online restart failed %w", err))
		return err
	}

	return nil
}

func sigusr2Test(
	ctx context.Context,
) error {

	err := ensurePostgresqlRunning(ctx)
	if err != nil {
		return err
	}

	err = ensureOdysseyRunning(ctx)
	if err != nil {
		return err
	}

	coroutineSleepCnt := 10

	ch := make(chan error, coroutineSleepCnt+1)
	wg := sync.WaitGroup{}
	{
		for i := 0; i < coroutineSleepCnt; i++ {
			wg.Add(1)
			go selectSleep(ctx, sleepInterval, ch, &wg, true)
		}

		time.Sleep(1 * time.Second)
		{
			wg.Add(1)
			go sigusr2Odyssey(ctx, ch, &wg)
		}
		time.Sleep(1 * time.Second)

	}
	wg.Wait()
	fmt.Println("sigusr2Test: wait done, closing channel")
	close(ch)
	if err := waitOnChan(ch, 0); err != nil {
		fmt.Println(fmt.Errorf("sigusr2 failed %w", err))
		return err
	}

	return nil
}

func odyClientServerInteractionsTestSet(ctx context.Context) error {

	err := sigusr2Test(ctx)
	if err != nil {
		err = fmt.Errorf("client server interaction error %w", err)
		fmt.Println(err)
		return err
	}

	err = onlineRestartTest(ctx)
	if err != nil {
		err = fmt.Errorf("client server interaction error %w", err)
		fmt.Println(err)
		return err
	}

	fmt.Println("odyClientServerInteractionsTestSet: Ok")

	return nil
}