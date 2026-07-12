package transport

import "context"

type MCPTransport interface {
	Start(context.Context) error
	Stop(context.Context) error
}
