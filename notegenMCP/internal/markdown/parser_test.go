package markdown

import "testing"

func TestSection(t *testing.T) {
	in := "# A\nintro\n## 子节\n中文\n# B\nend"
	got, ok := Section(in, "A")
	if !ok || got != "# A\nintro\n## 子节\n中文" {
		t.Fatalf("%q %v", got, ok)
	}
	out, ok := ReplaceSection(in, "A", "# A\nnew")
	if !ok || out != "# A\nnew\n# B\nend" {
		t.Fatal(out)
	}
}
