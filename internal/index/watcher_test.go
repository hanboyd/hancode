package index

import (
	"context"
	"os"
	"path/filepath"
	"testing"
	"time"
)

func TestWatcherExternalChanges(t *testing.T) {
	root := t.TempDir()
	db := filepath.Join(t.TempDir(), "i.db")
	if _, e := Rebuild(context.Background(), root, db); e != nil {
		t.Fatal(e)
	}
	s, e := Open(db)
	if e != nil {
		t.Fatal(e)
	}
	defer s.Close()
	w, e := NewWatcher(root, s)
	if e != nil {
		t.Fatal(e)
	}
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	w.Start(ctx)
	defer w.Close()
	p := filepath.Join(root, "external.md")
	if e = os.WriteFile(p, []byte("# 外部\n中文"), 0644); e != nil {
		t.Fatal(e)
	}
	waitCount(t, s, 1)
	if e = os.WriteFile(p, []byte("# 修改\nEnglish"), 0644); e != nil {
		t.Fatal(e)
	}
	time.Sleep(600 * time.Millisecond)
	if e = os.Remove(p); e != nil {
		t.Fatal(e)
	}
	waitCount(t, s, 0)
	if !w.State().Running {
		t.Fatal("watcher stopped")
	}
}
func waitCount(t *testing.T, s *Store, want int) {
	t.Helper()
	until := time.Now().Add(5 * time.Second)
	for time.Now().Before(until) {
		st, e := s.State(context.Background())
		if e == nil && st.IndexedNotes == want {
			return
		}
		time.Sleep(100 * time.Millisecond)
	}
	t.Fatalf("index count did not reach %d", want)
}
