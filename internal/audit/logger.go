package audit

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"sync"
	"time"
)

type Entry struct {
	Timestamp     time.Time `json:"timestamp"`
	RequestID     string    `json:"request_id,omitempty"`
	Client        string    `json:"client,omitempty"`
	Tool          string    `json:"tool"`
	Operation     string    `json:"operation"`
	RelativePaths []string  `json:"relative_paths"`
	BeforeHash    string    `json:"before_hash,omitempty"`
	AfterHash     string    `json:"after_hash,omitempty"`
	DurationMS    int64     `json:"duration_ms"`
	Result        string    `json:"result"`
	ErrorCode     string    `json:"error_code,omitempty"`
}
type Logger struct {
	dir      string
	maxBytes int64
	mu       sync.Mutex
}

func New(dir string, maxBytes int64) (*Logger, error) {
	if err := os.MkdirAll(dir, 0700); err != nil {
		return nil, err
	}
	return &Logger{dir: dir, maxBytes: maxBytes}, nil
}
func (l *Logger) Write(e Entry) error {
	l.mu.Lock()
	defer l.mu.Unlock()
	p := filepath.Join(l.dir, "audit.jsonl")
	if st, err := os.Stat(p); err == nil && st.Size() > l.maxBytes {
		_ = os.Rename(p, filepath.Join(l.dir, "audit-"+time.Now().UTC().Format("20060102T150405Z")+".jsonl"))
	}
	f, err := os.OpenFile(p, os.O_CREATE|os.O_APPEND|os.O_WRONLY, 0600)
	if err != nil {
		return err
	}
	defer f.Close()
	e.Timestamp = time.Now().UTC()
	b, err := json.Marshal(e)
	if err != nil {
		return err
	}
	if _, err = f.Write(append(b, '\n')); err != nil {
		return err
	}
	if err = f.Sync(); err != nil {
		return fmt.Errorf("sync audit: %w", err)
	}
	return nil
}
