package notegen

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"io/fs"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"time"

	"github.com/hanboyd/notegen-mcp/internal/audit"
	"github.com/hanboyd/notegen-mcp/internal/domain"
	nfs "github.com/hanboyd/notegen-mcp/internal/filesystem"
	md "github.com/hanboyd/notegen-mcp/internal/markdown"
	"github.com/hanboyd/notegen-mcp/internal/security"
)

type Workspace struct {
	resolver *security.Resolver
	locks    *nfs.LockSet
	audit    *audit.Logger
	maxNote  int64
	trash    string
}

func NewWorkspace(r *security.Resolver, a *audit.Logger, maxNote int64, trash string) *Workspace {
	return &Workspace{resolver: r, locks: nfs.NewLockSet(), audit: a, maxNote: maxNote, trash: trash}
}
func Hash(b []byte) string { x := sha256.Sum256(b); return hex.EncodeToString(x[:]) }
func (w *Workspace) Read(ctx context.Context, rel, section string, start, end, maxChars int) (domain.Note, error) {
	p, r, e := w.resolver.Resolve(0, rel, false)
	if e != nil {
		return domain.Note{}, e
	}
	b, e := os.ReadFile(p)
	if os.IsNotExist(e) {
		return domain.Note{}, notFound(r)
	}
	if e != nil {
		return domain.Note{}, e
	}
	if int64(len(b)) > w.maxNote {
		return domain.Note{}, tooLarge(r)
	}
	st, _ := os.Stat(p)
	text := string(b)
	if section != "" {
		var ok bool
		text, ok = md.Section(text, section)
		if !ok {
			return domain.Note{}, notFound(r + "#" + section)
		}
	}
	if start > 0 || end > 0 {
		text = md.Lines(text, start, end)
	}
	tr := false
	if maxChars > 0 && len([]rune(text)) > maxChars {
		text = string([]rune(text)[:maxChars])
		tr = true
	}
	return makeNote(r, text, b, st, tr), nil
}
func (w *Workspace) List(ctx context.Context, folder string, limit int, cursor string) ([]domain.Note, string, error) {
	if limit < 1 || limit > 200 {
		limit = 50
	}
	base := w.resolver.Root(0)
	var e error
	if folder != "" {
		base, _, e = w.resolver.Resolve(0, folder, false)
		if e != nil {
			return nil, "", e
		}
	}
	var paths []string
	e = filepath.WalkDir(base, func(p string, d fs.DirEntry, e error) error {
		if e != nil {
			return e
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
	if e != nil {
		return nil, "", e
	}
	sort.Strings(paths)
	var out []domain.Note
	next := ""
	for _, p := range paths {
		rel, _ := filepath.Rel(w.resolver.Root(0), p)
		rel = filepath.ToSlash(rel)
		if cursor != "" && rel <= cursor {
			continue
		}
		b, e := os.ReadFile(p)
		if e != nil {
			continue
		}
		st, _ := os.Stat(p)
		n := makeNote(rel, "", b, st, false)
		content := string(b)
		n.Title = md.Title(content, strings.TrimSuffix(filepath.Base(p), filepath.Ext(p)))
		rs := []rune(strings.TrimSpace(content))
		if len(rs) > 240 {
			rs = rs[:240]
		}
		n.Content = string(rs)
		out = append(out, n)
		if len(out) == limit {
			next = rel
			break
		}
	}
	return out, next, nil
}
func (w *Workspace) Create(ctx context.Context, rel, content, expected string, overwrite, mkdir bool) (domain.Note, error) {
	p, r, e := w.resolver.Resolve(0, rel, true)
	if e != nil {
		return domain.Note{}, e
	}
	if !strings.EqualFold(filepath.Ext(p), ".md") {
		return domain.Note{}, &domain.AppError{Code: domain.ErrInvalidMarkdown, Message: "note path must end in .md"}
	}
	if mkdir {
		if e = os.MkdirAll(filepath.Dir(p), 0755); e != nil {
			return domain.Note{}, e
		}
	}
	if _, e = os.Stat(p); e == nil && !overwrite {
		return domain.Note{}, conflict(r, "note already exists", "")
	}
	return w.write(ctx, p, r, []byte(content), expected, "notegen_create_note")
}
func (w *Workspace) Update(ctx context.Context, rel, op, content, section, find, repl, expected string) (domain.Note, error) {
	p, r, e := w.resolver.Resolve(0, rel, false)
	if e != nil {
		return domain.Note{}, e
	}
	old, e := os.ReadFile(p)
	if e != nil {
		return domain.Note{}, e
	}
	s := string(old)
	switch op {
	case "replace_all":
		s = content
	case "append":
		s += content
	case "prepend":
		s = content + s
	case "replace_section":
		var ok bool
		s, ok = md.ReplaceSection(s, section, content)
		if !ok {
			return domain.Note{}, notFound(r + "#" + section)
		}
	case "replace_text":
		if !strings.Contains(s, find) {
			return domain.Note{}, notFound(r + ":text")
		}
		s = strings.ReplaceAll(s, find, repl)
	default:
		return domain.Note{}, &domain.AppError{Code: domain.ErrInvalidMarkdown, Message: "unsupported update operation"}
	}
	return w.write(ctx, p, r, []byte(s), expected, "notegen_update_note")
}
func (w *Workspace) write(ctx context.Context, p, rel string, data []byte, expected, tool string) (domain.Note, error) {
	if int64(len(data)) > w.maxNote {
		return domain.Note{}, tooLarge(rel)
	}
	unlock, e := w.locks.Acquire(ctx, p)
	if e != nil {
		return domain.Note{}, &domain.AppError{Code: domain.ErrFileLocked, Message: "file lock timeout", Retryable: true}
	}
	defer unlock()
	old, _ := os.ReadFile(p)
	before := Hash(old)
	if expected != "" && before != expected {
		return domain.Note{}, conflict(rel, "file changed since it was read", before)
	}
	start := time.Now()
	if w.audit == nil {
		return domain.Note{}, fmt.Errorf("audit unavailable")
	}
	if e = nfs.AtomicWrite(p, data, 0644); e != nil {
		return domain.Note{}, e
	}
	after := Hash(data)
	if e = w.audit.Write(audit.Entry{Tool: tool, Operation: "write", RelativePaths: []string{rel}, BeforeHash: before, AfterHash: after, DurationMS: time.Since(start).Milliseconds(), Result: "success"}); e != nil {
		_ = nfs.AtomicWrite(p, old, 0644)
		return domain.Note{}, fmt.Errorf("audit failed; write rolled back: %w", e)
	}
	st, _ := os.Stat(p)
	return makeNote(rel, string(data), data, st, false), nil
}
func makeNote(rel, text string, raw []byte, st os.FileInfo, tr bool) domain.Note {
	title := md.Title(string(raw), strings.TrimSuffix(filepath.Base(rel), filepath.Ext(rel)))
	return domain.Note{ID: Hash([]byte(strings.ToLower(rel))), Path: filepath.ToSlash(rel), Title: title, Content: text, Hash: Hash(raw), ModifiedAt: st.ModTime(), CreatedAt: st.ModTime(), Size: st.Size(), Truncated: tr}
}
func notFound(p string) error {
	return &domain.AppError{Code: domain.ErrNoteNotFound, Message: "note or section not found", RelativePath: p}
}
func tooLarge(p string) error {
	return &domain.AppError{Code: domain.ErrOperationTooLarge, Message: "note exceeds configured size limit", RelativePath: p}
}
func conflict(p, msg, h string) error {
	return &domain.AppError{Code: domain.ErrVersionConflict, Message: msg, RelativePath: p, CurrentHash: h, Suggestion: "read the latest note and retry with its content hash"}
}
