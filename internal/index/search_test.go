package index

import (
	"context"
	"os"
	"path/filepath"
	"testing"
)

func TestTokenizeChineseMixed(t *testing.T) {
	x := Tokenize("知识管理 GoLang 123")
	for _, k := range []string{"知识", "知识管", "管理", "golang", "123"} {
		if x[k] == 0 {
			t.Errorf("missing %q in %#v", k, x)
		}
	}
}
func TestSearchChineseExactWeightsTagsPagination(t *testing.T) {
	root := t.TempDir()
	files := map[string]string{"a.md": "---\ntags: [项目, Go]\n---\n# 知识管理\n中文精确短语 Alpha 123", "b.md": "# 其他\n知识管理在正文 alpha", "c.md": "# 单字\n龙"}
	for p, c := range files {
		if err := os.WriteFile(filepath.Join(root, p), []byte(c), 0644); err != nil {
			t.Fatal(err)
		}
	}
	db := filepath.Join(t.TempDir(), "i.db")
	if _, err := Rebuild(context.Background(), root, db); err != nil {
		t.Fatal(err)
	}
	s, err := Open(db)
	if err != nil {
		t.Fatal(err)
	}
	defer s.Close()
	p, err := s.Search(context.Background(), SearchQuery{Query: "知识管理", SearchIn: "all", ExactPhrase: true, Limit: 1})
	if err != nil || len(p.Results) != 1 || p.Results[0].RelativePath != "a.md" || p.NextCursor == "" {
		t.Fatalf("page %#v %v", p, err)
	}
	p2, err := s.Search(context.Background(), SearchQuery{Query: "知识管理", Limit: 1, Cursor: p.NextCursor})
	if err != nil || len(p2.Results) != 1 {
		t.Fatalf("page2 %#v %v", p2, err)
	}
	tag, err := s.Search(context.Background(), SearchQuery{Query: "项目", SearchIn: "tags", Tags: []string{"项目"}})
	if err != nil || len(tag.Results) != 1 {
		t.Fatalf("tag %#v %v", tag, err)
	}
	single, err := s.Search(context.Background(), SearchQuery{Query: "龙", ExactPhrase: true})
	if err != nil || len(single.Results) != 1 {
		t.Fatalf("single %#v %v", single, err)
	}
	eng, err := s.Search(context.Background(), SearchQuery{Query: "ALPHA"})
	if err != nil || len(eng.Results) != 2 {
		t.Fatalf("english %#v %v", eng, err)
	}
}
