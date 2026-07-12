package mcpserver

import (
	"context"
	"testing"

	"github.com/hanboyd/notegen-mcp/internal/audit"
	"github.com/hanboyd/notegen-mcp/internal/notegen"
	"github.com/hanboyd/notegen-mcp/internal/security"
	"github.com/modelcontextprotocol/go-sdk/mcp"
)

func TestToolDiscoveryAndAnnotations(t *testing.T) {
	root := t.TempDir()
	r, _ := security.NewResolver([]string{root})
	a, _ := audit.New(t.TempDir(), 1<<20)
	srv := New(Service{Workspace: notegen.NewWorkspace(r, a, 1<<20, t.TempDir()), Version: "test"})
	ct, st := mcp.NewInMemoryTransports()
	ss, err := srv.Connect(context.Background(), st, nil)
	if err != nil {
		t.Fatal(err)
	}
	defer ss.Close()
	client := mcp.NewClient(&mcp.Implementation{Name: "test"}, nil)
	cs, err := client.Connect(context.Background(), ct, nil)
	if err != nil {
		t.Fatal(err)
	}
	defer cs.Close()
	res, err := cs.ListTools(context.Background(), nil)
	if err != nil {
		t.Fatal(err)
	}
	if len(res.Tools) != 5 {
		t.Fatalf("got %d tools", len(res.Tools))
	}
	for _, tool := range res.Tools {
		if tool.Annotations == nil {
			t.Fatalf("missing annotations: %s", tool.Name)
		}
		if tool.Name == "notegen_read_note" && !tool.Annotations.ReadOnlyHint {
			t.Fatal("read tool not marked read-only")
		}
	}
}
