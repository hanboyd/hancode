package filesystem

import (
	"context"
	"sync"
)

type LockSet struct{ m sync.Map }
type keyedLock struct{ ch chan struct{} }

func NewLockSet() *LockSet { return &LockSet{} }
func (s *LockSet) Acquire(ctx context.Context, path string) (func(), error) {
	v, _ := s.m.LoadOrStore(path, &keyedLock{ch: make(chan struct{}, 1)})
	l := v.(*keyedLock)
	select {
	case l.ch <- struct{}{}:
		return func() { <-l.ch }, nil
	case <-ctx.Done():
		return nil, ctx.Err()
	}
}
