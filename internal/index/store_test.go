package index

import (
	"context"
	"os"
	"path/filepath"
	"testing"
)

func TestRebuildAndIncremental(t *testing.T) {
	root := t.TempDir()
	db := filepath.Join(t.TempDir(), "index.db")
	if err := os.WriteFile(filepath.Join(root, "中文.md"), []byte("# 标题\n你好世界"), 0644); err != nil {
		t.Fatal(err)
	}
	st, err := Rebuild(context.Background(), root, db)
	if err != nil || st.Created != 1 {
		t.Fatalf("rebuild %#v %v", st, err)
	}
	s, err := Open(db)
	if err != nil {
		t.Fatal(err)
	}
	defer s.Close()
	state, err := s.State(context.Background())
	if err != nil || state.IndexedNotes != 1 || state.SchemaVersion != SchemaVersion {
		t.Fatalf("state %#v %v", state, err)
	}
	if err = os.WriteFile(filepath.Join(root, "中文.md"), []byte("# 新标题\n混合 Go 123"), 0644); err != nil {
		t.Fatal(err)
	}
	inc, err := s.Incremental(context.Background(), root)
	if err != nil || inc.Updated != 1 {
		t.Fatalf("incremental %#v %v", inc, err)
	}
	if err = os.Remove(filepath.Join(root, "中文.md")); err != nil {
		t.Fatal(err)
	}
	inc, err = s.Incremental(context.Background(), root)
	if err != nil || inc.Removed != 1 {
		t.Fatalf("remove %#v %v", inc, err)
	}
}
func TestUnknownSchemaRejected(t *testing.T) {
	p := filepath.Join(t.TempDir(), "x.db")
	s, err := Open(p)
	if err != nil {
		t.Fatal(err)
	}
	if _, err = s.db.Exec("UPDATE index_metadata SET schema_version=999"); err != nil {
		t.Fatal(err)
	}
	s.Close()
	if _, err = Open(p); err == nil {
		t.Fatal("expected schema rejection")
	}
}
