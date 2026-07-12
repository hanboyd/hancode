package notegen

import (
	"context"
	"errors"
	"path/filepath"
	"testing"

	"github.com/hanboyd/notegen-mcp/internal/audit"
	"github.com/hanboyd/notegen-mcp/internal/domain"
	"github.com/hanboyd/notegen-mcp/internal/security"
)

func testWorkspace(t *testing.T) (*Workspace, string) {
	t.Helper()
	root := t.TempDir()
	trash := filepath.Join(root, ".notegen-mcp-trash")
	logs := filepath.Join(root, ".notegen-mcp-logs")
	r, e := security.NewResolver([]string{root}, trash, logs)
	if e != nil {
		t.Fatal(e)
	}
	a, e := audit.New(logs, 1<<20)
	if e != nil {
		t.Fatal(e)
	}
	return NewWorkspace(r, a, 1<<20, trash), root
}
func TestCreateReadUpdateConflict(t *testing.T) {
	w, _ := testWorkspace(t)
	ctx := context.Background()
	n, e := w.Create(ctx, "folder/中文.md", "# 标题\n\n你好世界", "", false, true)
	if e != nil {
		t.Fatal(e)
	}
	got, e := w.Read(ctx, n.Path, "标题", 0, 0, 100)
	if e != nil || got.Content == "" {
		t.Fatalf("read: %#v %v", got, e)
	}
	u, e := w.Update(ctx, n.Path, "append", "\n追加", "", "", "", n.Hash)
	if e != nil {
		t.Fatal(e)
	}
	if _, e = w.Update(ctx, n.Path, "append", "bad", "", "", "", n.Hash); e == nil {
		t.Fatal("expected conflict")
	} else {
		var ae *domain.AppError
		if !errors.As(e, &ae) || ae.Code != domain.ErrVersionConflict {
			t.Fatalf("wrong error: %v", e)
		}
	}
	if u.Hash == n.Hash {
		t.Fatal("hash did not change")
	}
}
func TestListPagination(t *testing.T) {
	w, _ := testWorkspace(t)
	ctx := context.Background()
	for _, p := range []string{"a.md", "b.md", "c.md"} {
		if _, e := w.Create(ctx, p, "# "+p, "", false, true); e != nil {
			t.Fatal(e)
		}
	}
	a, c, e := w.List(ctx, "", 2, "")
	if e != nil || len(a) != 2 || c == "" {
		t.Fatalf("first page %d %q %v", len(a), c, e)
	}
	b, _, e := w.List(ctx, "", 2, c)
	if e != nil || len(b) != 1 {
		t.Fatalf("second page %d %v", len(b), e)
	}
}
