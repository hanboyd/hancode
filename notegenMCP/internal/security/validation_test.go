package security

import (
	"os"
	"path/filepath"
	"testing"
)

func TestResolveRejectsEscapes(t *testing.T) {
	r, err := NewResolver([]string{t.TempDir()})
	if err != nil {
		t.Fatal(err)
	}
	bad := []string{"../x.md", `C:\Windows\x`, `\\server\share\x`, "CON.md", "x:stream", "trail. ", "a/../../x"}
	for _, p := range bad {
		if _, _, e := r.Resolve(0, p, true); e == nil {
			t.Errorf("accepted %q", p)
		}
	}
}
func TestResolveSymlinkEscape(t *testing.T) {
	root := t.TempDir()
	outside := t.TempDir()
	link := filepath.Join(root, "link")
	if err := os.Symlink(outside, link); err != nil {
		t.Skipf("symlink unavailable: %v", err)
	}
	r, _ := NewResolver([]string{root})
	if _, _, e := r.Resolve(0, "link/x.md", true); e == nil {
		t.Fatal("accepted symlink escape")
	}
}
