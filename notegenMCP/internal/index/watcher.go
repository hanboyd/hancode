package index

import (
	"context"
	"io/fs"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"sync/atomic"
	"time"

	"github.com/fsnotify/fsnotify"
)

type WatcherState struct {
	Running                bool      `json:"watcher_running"`
	LastEventAt            time.Time `json:"last_watcher_event_at,omitempty"`
	LastConsistencyCheckAt time.Time `json:"last_consistency_check_at,omitempty"`
	Pending                int64     `json:"pending_index_updates"`
}
type Watcher struct {
	root    string
	store   *Store
	watcher *fsnotify.Watcher
	mu      sync.RWMutex
	state   WatcherState
	pending atomic.Int64
	cancel  context.CancelFunc
}

func NewWatcher(root string, store *Store) (*Watcher, error) {
	w, e := fsnotify.NewWatcher()
	if e != nil {
		return nil, e
	}
	x := &Watcher{root: root, store: store, watcher: w}
	e = filepath.WalkDir(root, func(p string, d fs.DirEntry, e error) error {
		if e != nil {
			return e
		}
		if d.IsDir() {
			if strings.HasPrefix(d.Name(), ".notegen-mcp-") {
				return filepath.SkipDir
			}
			return w.Add(p)
		}
		return nil
	})
	if e != nil {
		w.Close()
		return nil, e
	}
	return x, nil
}
func (x *Watcher) Start(ctx context.Context) {
	ctx, x.cancel = context.WithCancel(ctx)
	x.mu.Lock()
	x.state.Running = true
	x.mu.Unlock()
	go x.run(ctx)
}
func (x *Watcher) Close() error {
	if x.cancel != nil {
		x.cancel()
	}
	return x.watcher.Close()
}
func (x *Watcher) State() WatcherState {
	x.mu.RLock()
	defer x.mu.RUnlock()
	s := x.state
	s.Pending = x.pending.Load()
	return s
}
func (x *Watcher) run(ctx context.Context) {
	defer func() { x.mu.Lock(); x.state.Running = false; x.mu.Unlock() }()
	events := make(chan struct{}, 1)
	go func() {
		for {
			select {
			case <-ctx.Done():
				return
			case <-events:
				time.Sleep(250 * time.Millisecond)
				x.pending.Store(1)
				_, _ = x.store.Incremental(ctx, x.root)
				x.pending.Store(0)
			}
		}
	}()
	ticker := time.NewTicker(5 * time.Minute)
	defer ticker.Stop()
	for {
		select {
		case <-ctx.Done():
			return
		case e, ok := <-x.watcher.Events:
			if !ok {
				return
			}
			if ignoredEvent(e.Name) {
				continue
			}
			if e.Op&fsnotify.Create != 0 {
				if st, err := os.Stat(e.Name); err == nil && st.IsDir() {
					_ = x.watcher.Add(e.Name)
				}
			}
			x.mu.Lock()
			x.state.LastEventAt = time.Now()
			x.mu.Unlock()
			select {
			case events <- struct{}{}:
			default:
			}
		case <-x.watcher.Errors:
			select {
			case events <- struct{}{}:
			default:
			}
		case t := <-ticker.C:
			x.pending.Store(1)
			_, _ = x.store.Incremental(ctx, x.root)
			x.pending.Store(0)
			x.mu.Lock()
			x.state.LastConsistencyCheckAt = t
			x.mu.Unlock()
		}
	}
}
func ignoredEvent(p string) bool {
	n := strings.ToLower(filepath.Base(p))
	ext := strings.ToLower(filepath.Ext(n))
	return strings.HasPrefix(n, ".notegen-mcp-") || strings.HasSuffix(n, ".tmp") || strings.HasSuffix(n, "~") || (ext != "" && ext != ".md")
}
