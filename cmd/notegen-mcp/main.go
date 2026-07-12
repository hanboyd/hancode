package main

import (
	"context"
	"flag"
	"fmt"
	"github.com/hanboyd/notegen-mcp/internal/audit"
	"github.com/hanboyd/notegen-mcp/internal/config"
	"github.com/hanboyd/notegen-mcp/internal/index"
	mcpserver "github.com/hanboyd/notegen-mcp/internal/mcp"
	"github.com/hanboyd/notegen-mcp/internal/notegen"
	"github.com/hanboyd/notegen-mcp/internal/security"
	"github.com/hanboyd/notegen-mcp/internal/transport"
	"os"
	"os/signal"
	"path/filepath"
	"runtime"
	"syscall"
)

const version = "0.1.0"

func main() {
	cfgPath := flag.String("config", "config.json", "configuration file")
	stdio := flag.Bool("stdio", false, "run local STDIO MCP server")
	reindexFlag := flag.Bool("reindex", false, "safely rebuild the local index")
	check := flag.Bool("check", false, "validate configuration and workspace without modifying notes")
	ver := flag.Bool("version", false, "print version")
	flag.Parse()
	if *ver {
		fmt.Printf("notegen-mcp %s\nGo %s\nMCP Go SDK v1.6.1\nNoteGen 0.31.x\n", version, runtime.Version())
		return
	}
	c, e := config.Load(*cfgPath)
	if e != nil {
		fatal("CONFIG_INVALID", e)
	}
	r, e := security.NewResolver(c.NoteGenRoots, c.TrashDirectory, c.IndexDirectory, c.LogDirectory)
	if e != nil {
		fatal("CONFIG_INVALID", e)
	}
	if *check {
		fmt.Fprintf(os.Stderr, "configuration valid; workspace readable: %s\n", filepath.Clean(c.NoteGenRoots[0]))
		return
	}
	indexPath := filepath.Join(c.IndexDirectory, "index.db")
	if *reindexFlag {
		st, err := index.Rebuild(context.Background(), c.NoteGenRoots[0], indexPath)
		if err != nil {
			fatal("INDEX_UNAVAILABLE", err)
		}
		fmt.Fprintf(os.Stderr, "reindex complete: scanned=%d created=%d failed=%d duration_ms=%d\n", st.Scanned, st.Created, st.Failed, st.DurationMS)
		return
	}
	if !*stdio {
		fatal("CONFIG_INVALID", fmt.Errorf("specify --stdio, --check, or --version"))
	}
	a, e := audit.New(c.LogDirectory, 10<<20)
	if e != nil {
		fatal("WORKSPACE_UNAVAILABLE", e)
	}
	w := notegen.NewWorkspace(r, a, c.MaxNoteBytes, c.TrashDirectory)
	idx, e := index.Open(indexPath)
	if e != nil {
		fatal("INDEX_UNAVAILABLE", e)
	}
	defer idx.Close()
	_, _ = idx.Incremental(context.Background(), c.NoteGenRoots[0])
	ctx, cancel := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer cancel()
	var watcher *index.Watcher
	if c.WatchFiles {
		watcher, e = index.NewWatcher(c.NoteGenRoots[0], idx)
		if e != nil {
			fmt.Fprintf(os.Stderr, "watcher unavailable: %v\n", e)
		} else {
			watcher.Start(ctx)
			defer watcher.Close()
		}
	}
	srv := mcpserver.New(mcpserver.Service{Workspace: w, Index: idx, Watcher: watcher, WorkspaceRoot: c.NoteGenRoots[0], Version: version, NoteGenVersion: c.NoteGenVersion, Permission: c.PermissionMode})
	if e = transport.NewStdio(srv).Start(ctx); e != nil && ctx.Err() == nil {
		fatal("WORKSPACE_UNAVAILABLE", e)
	}
}
func fatal(code string, e error) { fmt.Fprintf(os.Stderr, "%s: %v\n", code, e); os.Exit(1) }
