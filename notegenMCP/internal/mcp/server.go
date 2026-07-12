package mcpserver

import (
	"context"
	"encoding/json"
	"errors"
	"runtime"
	"time"

	"github.com/hanboyd/notegen-mcp/internal/domain"
	"github.com/hanboyd/notegen-mcp/internal/index"
	"github.com/hanboyd/notegen-mcp/internal/notegen"
	"github.com/modelcontextprotocol/go-sdk/mcp"
)

const Instructions = "所有路径均相对于配置的 NoteGen 根目录。修改前应读取最新内容。修改时优先提供 expected_hash。删除默认进入回收站。批量修改优先 dry_run。不得访问工作区之外的文件。"

type Service struct {
	Workspace                           *notegen.Workspace
	Index                               *index.Store
	Watcher                             *index.Watcher
	WorkspaceRoot                       string
	Version, NoteGenVersion, Permission string
}

func New(s Service) *mcp.Server {
	srv := mcp.NewServer(&mcp.Implementation{Name: "notegen-mcp", Version: s.Version}, &mcp.ServerOptions{Instructions: Instructions})
	register(srv, s)
	return srv
}
func ro(title string) *mcp.ToolAnnotations {
	f := false
	return &mcp.ToolAnnotations{Title: title, ReadOnlyHint: true, DestructiveHint: &f, IdempotentHint: true, OpenWorldHint: &f}
}
func rw(title string, destructive, idempotent bool) *mcp.ToolAnnotations {
	f := false
	return &mcp.ToolAnnotations{Title: title, DestructiveHint: &destructive, IdempotentHint: idempotent, OpenWorldHint: &f}
}

type statusIn struct{}
type statusOut struct {
	ServerVersion       string `json:"server_version"`
	GoVersion           string `json:"go_version"`
	NoteGenVersion      string `json:"notegen_version"`
	PermissionMode      string `json:"permission_mode"`
	NetworkListening    bool   `json:"network_listening"`
	IndexedNotes        int    `json:"indexed_notes"`
	IndexSchemaVersion  int    `json:"index_schema_version"`
	IndexSizeBytes      int64  `json:"index_size_bytes"`
	WatcherRunning      bool   `json:"watcher_running"`
	LastWatcherEventAt  string `json:"last_watcher_event_at,omitempty"`
	PendingIndexUpdates int64  `json:"pending_index_updates"`
}
type listIn struct {
	Folder string `json:"folder,omitempty" jsonschema:"folder relative to workspace"`
	Limit  int    `json:"limit,omitempty" jsonschema:"maximum 1..200"`
	Cursor string `json:"cursor,omitempty"`
}
type listOut struct {
	Notes      []domain.Note `json:"notes"`
	NextCursor string        `json:"next_cursor,omitempty"`
}
type readIn struct {
	Path          string `json:"path"`
	Section       string `json:"section,omitempty"`
	StartLine     int    `json:"start_line,omitempty"`
	EndLine       int    `json:"end_line,omitempty"`
	MaxCharacters int    `json:"max_characters,omitempty"`
}
type noteOut struct {
	Note domain.Note `json:"note"`
}
type createIn struct {
	Path                    string `json:"path"`
	Content                 string `json:"content"`
	Overwrite               bool   `json:"overwrite,omitempty"`
	CreateParentDirectories bool   `json:"create_parent_directories,omitempty"`
	ExpectedHash            string `json:"expected_hash,omitempty"`
}
type updateIn struct {
	Path         string `json:"path"`
	Operation    string `json:"operation" jsonschema:"replace_all,append,prepend,replace_section,replace_text"`
	Content      string `json:"content,omitempty"`
	Section      string `json:"section,omitempty"`
	FindText     string `json:"find_text,omitempty"`
	Replacement  string `json:"replacement,omitempty"`
	ExpectedHash string `json:"expected_hash"`
}
type searchIn struct {
	Query          string   `json:"query"`
	SearchIn       string   `json:"search_in,omitempty"`
	Folder         string   `json:"folder,omitempty"`
	Tags           []string `json:"tags,omitempty"`
	ExactPhrase    bool     `json:"exact_phrase,omitempty"`
	ExcludeFolders []string `json:"exclude_folders,omitempty"`
	Limit          int      `json:"limit,omitempty"`
	Cursor         string   `json:"cursor,omitempty"`
}
type searchOut struct {
	Results       []index.SearchResult `json:"results"`
	NextCursor    string               `json:"next_cursor,omitempty"`
	TotalEstimate int                  `json:"total_estimate"`
	IndexState    string               `json:"index_state"`
}
type reindexIn struct {
	Mode         string `json:"mode" jsonschema:"full,incremental,note,folder"`
	Path         string `json:"path,omitempty"`
	VerifyHashes bool   `json:"verify_hashes,omitempty"`
}
type reindexOut struct {
	Scanned    int             `json:"scanned"`
	Created    int             `json:"created"`
	Updated    int             `json:"updated"`
	Removed    int             `json:"removed"`
	Unchanged  int             `json:"unchanged"`
	Failed     int             `json:"failed"`
	DurationMS int64           `json:"duration_ms"`
	Failures   []index.Failure `json:"failures,omitempty"`
}

func register(srv *mcp.Server, s Service) {
	mcp.AddTool(srv, &mcp.Tool{Name: "notegen_get_status", Description: "Read local server and workspace mode status. Read-only and idempotent.", Annotations: ro("NoteGen status")}, func(ctx context.Context, r *mcp.CallToolRequest, in statusIn) (*mcp.CallToolResult, statusOut, error) {
		out := statusOut{ServerVersion: s.Version, GoVersion: runtime.Version(), NoteGenVersion: s.NoteGenVersion, PermissionMode: s.Permission, NetworkListening: false}
		if s.Index != nil {
			if st, e := s.Index.State(ctx); e == nil {
				out.IndexedNotes = st.IndexedNotes
				out.IndexSchemaVersion = st.SchemaVersion
				out.IndexSizeBytes = st.SizeBytes
			}
		}
		if s.Watcher != nil {
			ws := s.Watcher.State()
			out.WatcherRunning = ws.Running
			out.PendingIndexUpdates = ws.Pending
			if !ws.LastEventAt.IsZero() {
				out.LastWatcherEventAt = ws.LastEventAt.UTC().Format(time.RFC3339Nano)
			}
		}
		return nil, out, nil
	})
	mcp.AddTool(srv, &mcp.Tool{Name: "notegen_list_notes", Description: "List a bounded page of Markdown notes; paths are relative and bodies are summaries.", Annotations: ro("List notes")}, func(ctx context.Context, r *mcp.CallToolRequest, in listIn) (*mcp.CallToolResult, listOut, error) {
		n, c, e := s.Workspace.List(ctx, in.Folder, in.Limit, in.Cursor)
		if e != nil {
			return toolErr(e), listOut{}, nil
		}
		return nil, listOut{n, c}, nil
	})
	mcp.AddTool(srv, &mcp.Tool{Name: "notegen_read_note", Description: "Read a note, line range, or Markdown heading section. Read latest before writes.", Annotations: ro("Read note")}, func(ctx context.Context, r *mcp.CallToolRequest, in readIn) (*mcp.CallToolResult, *noteOut, error) {
		n, e := s.Workspace.Read(ctx, in.Path, in.Section, in.StartLine, in.EndLine, in.MaxCharacters)
		if e != nil {
			return toolErr(e), nil, nil
		}
		return nil, &noteOut{n}, nil
	})
	mcp.AddTool(srv, &mcp.Tool{Name: "notegen_create_note", Description: "Create a Markdown note with validated path, atomic write and mandatory audit. May modify data.", Annotations: rw("Create note", false, false)}, func(ctx context.Context, r *mcp.CallToolRequest, in createIn) (*mcp.CallToolResult, *noteOut, error) {
		n, e := s.Workspace.Create(ctx, in.Path, in.Content, in.ExpectedHash, in.Overwrite, in.CreateParentDirectories)
		if e != nil {
			return toolErr(e), nil, nil
		}
		if s.Index != nil {
			if _, e = s.Index.Incremental(ctx, s.WorkspaceRoot); e != nil {
				return toolErr(&domain.AppError{Code: domain.ErrIndexUnavailable, Message: "note created but index update failed", Cause: e}), nil, nil
			}
		}
		return nil, &noteOut{n}, nil
	})
	mcp.AddTool(srv, &mcp.Tool{Name: "notegen_update_note", Description: "Safely update a note using optimistic expected_hash, atomic replacement and audit. May modify data.", Annotations: rw("Update note", true, false)}, func(ctx context.Context, r *mcp.CallToolRequest, in updateIn) (*mcp.CallToolResult, *noteOut, error) {
		n, e := s.Workspace.Update(ctx, in.Path, in.Operation, in.Content, in.Section, in.FindText, in.Replacement, in.ExpectedHash)
		if e != nil {
			return toolErr(e), nil, nil
		}
		if s.Index != nil {
			if _, e = s.Index.Incremental(ctx, s.WorkspaceRoot); e != nil {
				return toolErr(&domain.AppError{Code: domain.ErrIndexUnavailable, Message: "note updated but index update failed", Cause: e}), nil, nil
			}
		}
		return nil, &noteOut{n}, nil
	})
	mcp.AddTool(srv, &mcp.Tool{Name: "notegen_search", Description: "Search the persistent local Unicode 2/3-gram index with bounded snippets.", Annotations: ro("Search notes")}, func(ctx context.Context, r *mcp.CallToolRequest, in searchIn) (*mcp.CallToolResult, *searchOut, error) {
		if s.Index == nil {
			return toolErr(&domain.AppError{Code: domain.ErrIndexUnavailable, Message: "index unavailable"}), nil, nil
		}
		p, e := s.Index.Search(ctx, index.SearchQuery{Query: in.Query, SearchIn: in.SearchIn, Folder: in.Folder, Tags: in.Tags, ExactPhrase: in.ExactPhrase, ExcludeFolders: in.ExcludeFolders, Limit: in.Limit, Cursor: in.Cursor})
		if e != nil {
			return toolErr(&domain.AppError{Code: domain.ErrIndexUnavailable, Message: "search failed", Cause: e}), nil, nil
		}
		return nil, &searchOut{p.Results, p.NextCursor, p.TotalEstimate, p.IndexState}, nil
	})
	mcp.AddTool(srv, &mcp.Tool{Name: "notegen_reindex", Description: "Safely rebuild or incrementally reconcile the local cache index; Markdown is never changed.", Annotations: rw("Reindex", false, true)}, func(ctx context.Context, r *mcp.CallToolRequest, in reindexIn) (*mcp.CallToolResult, *reindexOut, error) {
		if s.Index == nil {
			return toolErr(&domain.AppError{Code: domain.ErrIndexUnavailable, Message: "index unavailable"}), nil, nil
		}
		var st index.Stats
		var e error
		if in.Mode == "full" {
			st, e = s.Index.FullRebuild(ctx, s.WorkspaceRoot)
		} else {
			st, e = s.Index.Incremental(ctx, s.WorkspaceRoot)
		}
		if e != nil {
			return toolErr(&domain.AppError{Code: domain.ErrIndexUnavailable, Message: "reindex failed", Cause: e}), nil, nil
		}
		return nil, &reindexOut{st.Scanned, st.Created, st.Updated, st.Removed, st.Unchanged, st.Failed, st.DurationMS, st.Failures}, nil
	})
}
func toolErr(err error) *mcp.CallToolResult {
	var ae *domain.AppError
	if !errors.As(err, &ae) {
		ae = &domain.AppError{Code: "INTERNAL_ERROR", Message: "operation failed"}
	}
	b, _ := json.Marshal(ae)
	return &mcp.CallToolResult{Content: []mcp.Content{&mcp.TextContent{Text: string(b)}}, IsError: true}
}
