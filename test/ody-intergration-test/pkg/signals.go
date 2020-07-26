package main

import (
	"context"
	"fmt"
	"syscall"
	"time"
)

func SigintAfterSigusr2Test(ctx context.Context) error {

	if err := ensureOdysseyRunning(ctx); err != nil {
		return err
	}

	if err := waitOnOdysseyAlive(ctx, time.Second*2); err != nil {
		return err
	}

	go selectSleepNoWait(ctx, 10000)

	if _, err := signalToProc(syscall.SIGUSR2, "odyssey"); err != nil {
		return err
	}
	if _, err := pidNyName("odyssey"); err != nil {
		return err
	}
	if p, err := signalToProc(syscall.SIGINT, "odyssey"); err != nil {
		return err
	} else {
		_, cancel := context.WithTimeout(ctx, 2*time.Second)
		defer cancel()

		if pst, err := p.Wait(); err != nil {
			return err
		} else {
			if pst.Exited() {
				return nil
			} else {
				return fmt.Errorf("odyssey ignores sigint")
			}
		}
	}
}

func odySignalsTestSet(ctx context.Context) error {
	if err := SigintAfterSigusr2Test(ctx); err != nil {
		err = fmt.Errorf("signals test failed: %w", err)
		fmt.Println(err)
		return err
	}

	fmt.Println("odySignalsTestSet: Ok")

	return nil
}
