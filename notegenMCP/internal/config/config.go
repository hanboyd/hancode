package config

import (
	"encoding/json"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"time"
)

type Config struct {
	NoteGenRoots             []string      `json:"notegen_roots"`
	NoteGenVersion           string        `json:"notegen_version,omitempty"`
	PermissionMode           string        `json:"permission_mode"`
	AllowCreate              bool          `json:"allow_create"`
	AllowUpdate              bool          `json:"allow_update"`
	AllowMove                bool          `json:"allow_move"`
	AllowBatchOperations     bool          `json:"allow_batch_operations"`
	AllowSoftDelete          bool          `json:"allow_soft_delete"`
	AllowGitCheckpoint       bool          `json:"allow_git_checkpoint"`
	AllowPermanentDelete     bool          `json:"allow_permanent_delete"`
	TrashDirectory           string        `json:"trash_directory"`
	IndexDirectory           string        `json:"index_directory"`
	LogDirectory             string        `json:"log_directory"`
	MaxBatchFiles            int           `json:"max_batch_files"`
	MaxRequestBytes          int64         `json:"max_request_bytes"`
	MaxNoteBytes             int64         `json:"max_note_bytes"`
	MaxAttachmentBytes       int64         `json:"max_attachment_bytes"`
	WatchFiles               bool          `json:"watch_files"`
	GitCheckpointBeforeBatch bool          `json:"git_checkpoint_before_batch"`
	AuditRequired            bool          `json:"audit_required"`
	LockTimeout              time.Duration `json:"-"`
}

func Load(path string) (Config, error) {
	b, err := os.ReadFile(path)
	if err != nil {
		return Config{}, fmt.Errorf("read config: %w", err)
	}
	var c Config
	d := json.NewDecoder(bytesReader(b))
	d.DisallowUnknownFields()
	if err := d.Decode(&c); err != nil {
		return c, fmt.Errorf("decode config: %w", err)
	}
	c.LockTimeout = 5 * time.Second
	return c, c.Validate()
}

func (c Config) Validate() error {
	if len(c.NoteGenRoots) == 0 {
		return fmt.Errorf("notegen_roots must contain at least one explicit path")
	}
	if c.PermissionMode != "admin" && c.PermissionMode != "read_only" {
		return fmt.Errorf("permission_mode must be admin or read_only")
	}
	if c.AllowPermanentDelete {
		return fmt.Errorf("allow_permanent_delete must be false in this release")
	}
	if c.MaxBatchFiles < 1 || c.MaxBatchFiles > 1000 {
		return fmt.Errorf("max_batch_files must be 1..1000")
	}
	if c.MaxRequestBytes < 1 || c.MaxNoteBytes < 1 || c.MaxNoteBytes > c.MaxRequestBytes {
		return fmt.Errorf("invalid request/note byte limits")
	}
	for _, root := range c.NoteGenRoots {
		if !filepath.IsAbs(root) {
			return fmt.Errorf("workspace root must be absolute: %q", root)
		}
		st, err := os.Stat(root)
		if err != nil {
			return fmt.Errorf("workspace root unavailable %q: %w", root, err)
		}
		if !st.IsDir() {
			return fmt.Errorf("workspace root is not a directory: %q", root)
		}
	}
	for name, p := range map[string]string{"trash_directory": c.TrashDirectory, "index_directory": c.IndexDirectory, "log_directory": c.LogDirectory} {
		if !filepath.IsAbs(p) {
			return fmt.Errorf("%s must be absolute", name)
		}
	}
	return nil
}

// isolated to keep Load strict while avoiding an exported implementation detail.
func bytesReader(b []byte) *reader { return &reader{b: b} }

type reader struct {
	b []byte
	n int
}

func (r *reader) Read(p []byte) (int, error) {
	if r.n == len(r.b) {
		return 0, io.EOF
	}
	n := copy(p, r.b[r.n:])
	r.n += n
	return n, nil
}
