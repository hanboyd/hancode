package transport

import (
	"context"
	"github.com/modelcontextprotocol/go-sdk/mcp"
)

type Stdio struct {
	server *mcp.Server
	cancel context.CancelFunc
}

func NewStdio(s *mcp.Server) *Stdio { return &Stdio{server: s} }
func (s *Stdio) Start(ctx context.Context) error {
	ctx, s.cancel = context.WithCancel(ctx)
	return s.server.Run(ctx, &mcp.StdioTransport{})
}
func (s *Stdio) Stop(context.Context) error {
	if s.cancel != nil {
		s.cancel()
	}
	return nil
}
