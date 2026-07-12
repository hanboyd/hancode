package index

import (
	"context"
	"database/sql"
	"encoding/json"
	"fmt"
	"io/fs"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"sync"
	"time"

	"github.com/hanboyd/notegen-mcp/internal/markdown"
	"github.com/hanboyd/notegen-mcp/internal/notegen"
	"gopkg.in/yaml.v3"
	_ "modernc.org/sqlite"
)

const SchemaVersion = 1
const TokenizerVersion = "unicode-ngram-2-3-v1"

type Store struct {
	db   *sql.DB
	path string
	mu   sync.RWMutex
}
type Stats struct {
	Scanned    int       `json:"scanned"`
	Created    int       `json:"created"`
	Updated    int       `json:"updated"`
	Removed    int       `json:"removed"`
	Unchanged  int       `json:"unchanged"`
	Failed     int       `json:"failed"`
	DurationMS int64     `json:"duration_ms"`
	Failures   []Failure `json:"failures,omitempty"`
}
type Failure struct {
	Path  string `json:"path"`
	Error string `json:"error"`
}
type State struct {
	IndexedNotes           int    `json:"indexed_notes"`
	SchemaVersion          int    `json:"schema_version"`
	SizeBytes              int64  `json:"size_bytes"`
	LastFullScanAt         string `json:"last_full_scan_at,omitempty"`
	LastConsistencyCheckAt string `json:"last_consistency_check_at,omitempty"`
}

func Open(path string) (*Store, error) {
	if err := os.MkdirAll(filepath.Dir(path), 0700); err != nil {
		return nil, err
	}
	db, err := sql.Open("sqlite", path)
	if err != nil {
		return nil, err
	}
	s := &Store{db: db, path: path}
	if err = s.init(context.Background()); err != nil {
		db.Close()
		return nil, err
	}
	return s, nil
}
func (s *Store) Close() error { s.mu.Lock(); defer s.mu.Unlock(); return s.db.Close() }
func (s *Store) init(ctx context.Context) error {
	if _, err := s.db.ExecContext(ctx, `PRAGMA journal_mode=WAL; PRAGMA synchronous=FULL; PRAGMA foreign_keys=ON;`); err != nil {
		return err
	}
	_, err := s.db.ExecContext(ctx, `
CREATE TABLE IF NOT EXISTS index_metadata (schema_version INTEGER NOT NULL, created_at TEXT NOT NULL, last_full_scan_at TEXT, last_consistency_check_at TEXT, tokenizer_version TEXT NOT NULL);
CREATE TABLE IF NOT EXISTS notes (
 note_id TEXT PRIMARY KEY, relative_path TEXT NOT NULL UNIQUE, normalized_path TEXT NOT NULL UNIQUE,
 title TEXT NOT NULL, content TEXT NOT NULL, tags_json TEXT NOT NULL, created_at TEXT NOT NULL,
 updated_at TEXT NOT NULL, indexed_at TEXT NOT NULL, content_hash TEXT NOT NULL, file_size INTEGER NOT NULL,
 frontmatter_json TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_notes_updated ON notes(updated_at DESC, normalized_path);
CREATE TABLE IF NOT EXISTS note_tokens (note_id TEXT NOT NULL, token TEXT NOT NULL, field TEXT NOT NULL, frequency INTEGER NOT NULL, PRIMARY KEY(note_id,token,field), FOREIGN KEY(note_id) REFERENCES notes(note_id) ON DELETE CASCADE);
CREATE INDEX IF NOT EXISTS idx_note_tokens_token ON note_tokens(token,note_id);
`)
	if err != nil {
		return err
	}
	var n int
	if err = s.db.QueryRowContext(ctx, "SELECT count(*) FROM index_metadata").Scan(&n); err != nil {
		return err
	}
	if n == 0 {
		_, err = s.db.ExecContext(ctx, "INSERT INTO index_metadata(schema_version,created_at,tokenizer_version) VALUES(?,?,?)", SchemaVersion, time.Now().UTC().Format(time.RFC3339Nano), TokenizerVersion)
		return err
	}
	var v int
	if err = s.db.QueryRowContext(ctx, "SELECT schema_version FROM index_metadata LIMIT 1").Scan(&v); err != nil {
		return err
	}
	if v != SchemaVersion {
		return fmt.Errorf("unsupported index schema %d (expected %d)", v, SchemaVersion)
	}
	return nil
}

func Rebuild(ctx context.Context, root, indexPath string) (Stats, error) {
	start := time.Now()
	if err := os.MkdirAll(filepath.Dir(indexPath), 0700); err != nil {
		return Stats{}, err
	}
	tmp := indexPath + fmt.Sprintf(".rebuild-%d", time.Now().UnixNano())
	_ = os.Remove(tmp)
	s, err := Open(tmp)
	if err != nil {
		return Stats{}, err
	}
	stats, err := s.scan(ctx, root, true)
	if err == nil {
		var check string
		err = s.db.QueryRowContext(ctx, "PRAGMA integrity_check").Scan(&check)
		if err == nil && check != "ok" {
			err = fmt.Errorf("integrity_check: %s", check)
		}
	}
	closeErr := s.Close()
	if err == nil {
		err = closeErr
	}
	if err != nil {
		_ = os.Remove(tmp)
		return stats, err
	}
	if err = replaceDatabase(indexPath, tmp); err != nil {
		_ = os.Remove(tmp)
		return stats, err
	}
	stats.DurationMS = time.Since(start).Milliseconds()
	return stats, nil
}
func (s *Store) Incremental(ctx context.Context, root string) (Stats, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	start := time.Now()
	st, err := s.scan(ctx, root, false)
	st.DurationMS = time.Since(start).Milliseconds()
	return st, err
}

func (s *Store) FullRebuild(ctx context.Context, root string) (Stats, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	_ = s.db.Close()
	stats, rebuildErr := Rebuild(ctx, root, s.path)
	reopened, openErr := Open(s.path)
	if openErr != nil {
		return stats, openErr
	}
	s.db = reopened.db
	if rebuildErr != nil {
		return stats, rebuildErr
	}
	return stats, nil
}

func (s *Store) scan(ctx context.Context, root string, full bool) (Stats, error) {
	var stats Stats
	paths := []string{}
	err := filepath.WalkDir(root, func(p string, d fs.DirEntry, e error) error {
		if e != nil {
			return e
		}
		if ctx.Err() != nil {
			return ctx.Err()
		}
		if d.IsDir() {
			if strings.HasPrefix(d.Name(), ".notegen-mcp-") {
				return filepath.SkipDir
			}
			return nil
		}
		if strings.EqualFold(filepath.Ext(p), ".md") {
			paths = append(paths, p)
		}
		return nil
	})
	if err != nil {
		return stats, err
	}
	sort.Strings(paths)
	seen := map[string]bool{}
	tx, err := s.db.BeginTx(ctx, nil)
	if err != nil {
		return stats, err
	}
	defer tx.Rollback()
	for _, p := range paths {
		stats.Scanned++
		rel, _ := filepath.Rel(root, p)
		rel = filepath.ToSlash(rel)
		norm := strings.ToLower(rel)
		seen[norm] = true
		changed, e := upsertFile(ctx, tx, p, rel, norm)
		if e != nil {
			stats.Failed++
			stats.Failures = append(stats.Failures, Failure{rel, e.Error()})
			continue
		}
		if changed == 1 {
			stats.Created++
		} else if changed == 2 {
			stats.Updated++
		} else {
			stats.Unchanged++
		}
	}
	rows, err := tx.QueryContext(ctx, "SELECT normalized_path FROM notes")
	if err != nil {
		return stats, err
	}
	var remove []string
	for rows.Next() {
		var p string
		if err = rows.Scan(&p); err != nil {
			rows.Close()
			return stats, err
		}
		if !seen[p] {
			remove = append(remove, p)
		}
	}
	rows.Close()
	for _, p := range remove {
		if _, err = tx.ExecContext(ctx, "DELETE FROM notes WHERE normalized_path=?", p); err != nil {
			return stats, err
		}
		stats.Removed++
	}
	now := time.Now().UTC().Format(time.RFC3339Nano)
	field := "last_consistency_check_at"
	if full {
		field = "last_full_scan_at"
	}
	if _, err = tx.ExecContext(ctx, "UPDATE index_metadata SET "+field+"=?", now); err != nil {
		return stats, err
	}
	if err = tx.Commit(); err != nil {
		return stats, err
	}
	return stats, nil
}

func upsertFile(ctx context.Context, tx *sql.Tx, path, rel, norm string) (int, error) {
	st, err := os.Stat(path)
	if err != nil {
		return 0, err
	}
	updatedText := st.ModTime().UTC().Format(time.RFC3339Nano)
	var old string
	var oldSize int64
	var oldUpdated string
	err = tx.QueryRowContext(ctx, "SELECT content_hash,file_size,updated_at FROM notes WHERE normalized_path=?", norm).Scan(&old, &oldSize, &oldUpdated)
	created := err == sql.ErrNoRows
	if err != nil && err != sql.ErrNoRows {
		return 0, err
	}
	if !created && oldSize == st.Size() && oldUpdated == updatedText {
		return 0, nil
	}
	b, err := os.ReadFile(path)
	if err != nil {
		return 0, err
	}
	hash := notegen.Hash(b)
	if old == hash {
		_, err = tx.ExecContext(ctx, "UPDATE notes SET file_size=?,updated_at=?,indexed_at=? WHERE normalized_path=?", st.Size(), updatedText, time.Now().UTC().Format(time.RFC3339Nano), norm)
		return 0, nil
	}
	content := string(b)
	title := markdown.Title(content, strings.TrimSuffix(filepath.Base(rel), filepath.Ext(rel)))
	front, tags := parseFrontmatter(content)
	fj, _ := json.Marshal(front)
	tj, _ := json.Marshal(tags)
	now := time.Now().UTC().Format(time.RFC3339Nano)
	noteID := notegen.Hash([]byte(norm))
	_, err = tx.ExecContext(ctx, `INSERT INTO notes(note_id,relative_path,normalized_path,title,content,tags_json,created_at,updated_at,indexed_at,content_hash,file_size,frontmatter_json) VALUES(?,?,?,?,?,?,?,?,?,?,?,?) ON CONFLICT(normalized_path) DO UPDATE SET relative_path=excluded.relative_path,title=excluded.title,content=excluded.content,tags_json=excluded.tags_json,updated_at=excluded.updated_at,indexed_at=excluded.indexed_at,content_hash=excluded.content_hash,file_size=excluded.file_size,frontmatter_json=excluded.frontmatter_json`, noteID, rel, norm, title, content, string(tj), updatedText, updatedText, now, hash, st.Size(), string(fj))
	if err != nil {
		return 0, err
	}
	if _, err = tx.ExecContext(ctx, "DELETE FROM note_tokens WHERE note_id=?", noteID); err != nil {
		return 0, err
	}
	fields := map[string]string{"title": title, "path": rel, "tags": strings.Join(tags, " "), "content": content}
	for field, text := range fields {
		for token, frequency := range Tokenize(text) {
			if _, err = tx.ExecContext(ctx, "INSERT INTO note_tokens(note_id,token,field,frequency) VALUES(?,?,?,?)", noteID, token, field, frequency); err != nil {
				return 0, err
			}
		}
	}
	if created {
		return 1, nil
	}
	return 2, nil
}
func parseFrontmatter(content string) (map[string]any, []string) {
	fm := map[string]any{}
	normalized := strings.ReplaceAll(strings.TrimPrefix(content, "\ufeff"), "\r\n", "\n")
	if !strings.HasPrefix(normalized, "---\n") {
		return fm, []string{}
	}
	end := strings.Index(normalized[4:], "\n---")
	if end < 0 || yaml.Unmarshal([]byte(normalized[4:4+end]), &fm) != nil {
		return map[string]any{}, []string{}
	}
	var tags []string
	switch v := fm["tags"].(type) {
	case []any:
		for _, x := range v {
			tags = append(tags, fmt.Sprint(x))
		}
	case []string:
		tags = v
	case string:
		for _, x := range strings.Split(v, ",") {
			if x = strings.TrimSpace(x); x != "" {
				tags = append(tags, x)
			}
		}
	}
	return fm, tags
}
func replaceDatabase(dst, tmp string) error {
	backup := dst + ".previous"
	_ = os.Remove(backup)
	if _, err := os.Stat(dst); err == nil {
		if err = os.Rename(dst, backup); err != nil {
			return err
		}
	}
	if err := os.Rename(tmp, dst); err != nil {
		_ = os.Rename(backup, dst)
		return err
	}
	_ = os.Remove(backup)
	return nil
}
func (s *Store) State(ctx context.Context) (State, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	var st State
	var full, check sql.NullString
	if err := s.db.QueryRowContext(ctx, "SELECT count(*) FROM notes").Scan(&st.IndexedNotes); err != nil {
		return st, err
	}
	if err := s.db.QueryRowContext(ctx, "SELECT schema_version,last_full_scan_at,last_consistency_check_at FROM index_metadata LIMIT 1").Scan(&st.SchemaVersion, &full, &check); err != nil {
		return st, err
	}
	st.LastFullScanAt = full.String
	st.LastConsistencyCheckAt = check.String
	if x, err := os.Stat(s.path); err == nil {
		st.SizeBytes = x.Size()
	}
	return st, nil
}
