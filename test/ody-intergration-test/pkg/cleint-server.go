package main

import (
	"context"
	"fmt"
	"sync"
	"syscall"
	"time"
)

func selectSleepNoWait(ctx context.Context, i int) error {
	db, err := getConn(ctx, databaseName, 1)
	if err != nil {
		return err
	}
	defer db.Close()

	qry := fmt.Sprintf("SELECT pg_sleep(%d)", i)

	fmt.Printf("selectSleep: doing query %s\n", qry)

	_ = db.QueryRowContext(ctx, qry)
	fmt.Print("select sleep OK\n")
	return err
}

func selectSleep(ctx context.Context, i int, ch chan error, wg *sync.WaitGroup, before_restart bool) {
	defer wg.Done()
	db, err := getConn(ctx, databaseName, 1)
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

	defer db.Close()

	qry := fmt.Sprintf("SELECT pg_sleep(%d)", i)

	fmt.Printf("selectSleep: doing query %s\n", qry)

	r, err := db.QueryContext(ctx, qry)
	if err != nil {
		if before_restart {
			err = fmt.Errorf("before restart coroutine failed %w", err)
		} else {
			err = fmt.Errorf("after restart coroutine failed %w", err)
		}
		fmt.Printf("sleep coroutine fail time: %s\n", time.Now().Format(time.RFC3339Nano))
		ch <- err
		return
	} else {
		for r.Next() {
			r.Scan(struct {

			}{})
		}
		r.Close()
	}
	ch <- err
	fmt.Print("select sleep OK\n")
}

const (
	sleepInterval      = 10
	maxCoroutineFailOk = 4
)

func waitOnChan(ch chan error, maxOK int) error {
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

	if err := ensureOdysseyRunning(ctx); err != nil {
		return err
	}

	coroutineSleepCnt := 5
	repeatCnt := 4

	for j := 0; j < repeatCnt; j++ {
		ch := make(chan error, coroutineSleepCnt*3)
		wg := sync.WaitGroup{}
		{
			for i := 0; i < coroutineSleepCnt; i++ {
				wg.Add(1)
				go selectSleep(ctx, sleepInterval, ch, &wg, true)
			}

			// to make sure previous select was in old ody
			time.Sleep(1 * time.Second)

			restartOdyssey(ctx)

			for i := 0; i < coroutineSleepCnt*2; i++ {
				wg.Add(1)
				go selectSleep(ctx, sleepInterval, ch, &wg, false)
			}
		}
		wg.Wait()
		fmt.Println("onlineRestartTest: wait done, closing channel")
		close(ch)
		// no single coroutine should fail!
		if err := waitOnChan(ch, 0); err != nil {
			fmt.Println(fmt.Errorf("online restart failed %w", err))
			return err
		}
		time.Sleep(1 * time.Second)
		fmt.Println("Iter complete")
	}
	if _, err := signalToProc(syscall.SIGINT, "odyssey"); err != nil {
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

	if err := ensureOdysseyRunning(ctx); err != nil {
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
		wg.Add(1)
		go sigusr2Odyssey(ctx, ch, &wg)

	}
	wg.Wait()
	fmt.Println("sigusr2Test: wait done, closing channel")
	close(ch)
	if err := waitOnChan(ch, 0); err != nil {
		fmt.Println(fmt.Errorf("sigusr2 failed %w", err))
		return err
	}

	if _, err := signalToProc(syscall.SIGINT, "odyssey"); err != nil {
		return err
	}
	return nil
}



func usrNoReadResultWhilesigusr2Test(
	ctx context.Context,
) error {

	err := ensurePostgresqlRunning(ctx)
	if err != nil {
		return err
	}

	if err := ensureOdysseyRunning(ctx); err != nil {
		return err
	}

	db, err := getConn(ctx, databaseName, 1)
	
	if _, err := db.Query("Select 42"); err != nil {
		return err
	}

	if _, err := signalToProc(syscall.SIGUSR2, "odyssey"); err != nil {
		return err
	}

	time.Sleep(30 * time.Second)
	if err := db.Ping(); err != nil {
		return err
	}
	time.Sleep(70 * time.Second)
	if err := db.Ping(); err != nil {
		fmt.Println(err)
		return nil
	}

	return fmt.Errorf("connection not closed!!!\n")
}

func odyClientServerInteractionsTestSet(ctx context.Context) error {

	if err := usrNoReadResultWhilesigusr2Test(ctx); err != nil {
		err = fmt.Errorf("usrNoReadResultWhilesigusr2 error %w", err)
		fmt.Println(err)
		return err
	}

	if err := onlineRestartTest(ctx); err != nil {
		err = fmt.Errorf("online restart error %w", err)
		fmt.Println(err)
		return err
	}

	if err := sigusr2Test(ctx); err != nil {
		err = fmt.Errorf("sigusr2 error %w", err)
		fmt.Println(err)
		return err
	}

	fmt.Println("odyClientServerInteractionsTestSet: Ok")

	return nil
}
