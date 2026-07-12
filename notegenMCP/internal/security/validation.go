package security

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"

	"github.com/hanboyd/notegen-mcp/internal/domain"
)

var reserved = map[string]bool{"CON": true, "PRN": true, "AUX": true, "NUL": true, "COM1": true, "COM2": true, "COM3": true, "COM4": true, "COM5": true, "COM6": true, "COM7": true, "COM8": true, "COM9": true, "LPT1": true, "LPT2": true, "LPT3": true, "LPT4": true, "LPT5": true, "LPT6": true, "LPT7": true, "LPT8": true, "LPT9": true}

type Resolver struct {
	roots    []string
	excluded []string
}

func NewResolver(roots []string, excluded ...string) (*Resolver, error) {
	r := &Resolver{}
	for _, p := range roots {
		q, e := canonicalExisting(p)
		if e != nil {
			return nil, e
		}
		r.roots = append(r.roots, q)
	}
	for _, p := range excluded {
		if p == "" {
			continue
		}
		q, e := filepath.Abs(p)
		if e != nil {
			return nil, e
		}
		r.excluded = append(r.excluded, filepath.Clean(q))
	}
	return r, nil
}

func (r *Resolver) Resolve(rootIndex int, rel string, allowMissing bool) (string, string, error) {
	if rootIndex < 0 || rootIndex >= len(r.roots) {
		return "", "", appErr(domain.ErrPathOutsideRoot, "invalid workspace root")
	}
	if rel == "" || filepath.IsAbs(rel) || strings.HasPrefix(rel, `\\`) || strings.Contains(rel, ":") {
		return "", "", appErr(domain.ErrPathOutsideRoot, "path must be a relative workspace path")
	}
	rel = strings.ReplaceAll(rel, "/", string(filepath.Separator))
	clean := filepath.Clean(rel)
	if clean == "." || clean == ".." || strings.HasPrefix(clean, ".."+string(filepath.Separator)) {
		return "", "", appErr(domain.ErrPathOutsideRoot, "path traversal is not allowed")
	}
	for _, s := range strings.Split(clean, string(filepath.Separator)) {
		if err := validateSegment(s); err != nil {
			return "", "", err
		}
	}
	target := filepath.Join(r.roots[rootIndex], clean)
	parent := target
	if allowMissing {
		parent = filepath.Dir(target)
	}
	resolved, err := canonicalNearest(parent)
	if err != nil {
		return "", "", err
	}
	if allowMissing {
		target = filepath.Join(resolved, filepath.Base(target))
	} else {
		target = resolved
	}
	if !within(r.roots[rootIndex], target) {
		return "", "", appErr(domain.ErrPathOutsideRoot, "resolved path leaves workspace")
	}
	for _, x := range r.excluded {
		if within(x, target) {
			return "", "", appErr(domain.ErrPermissionDenied, "path is an MCP private directory")
		}
	}
	out, err := filepath.Rel(r.roots[rootIndex], target)
	if err != nil {
		return "", "", err
	}
	return target, filepath.ToSlash(out), nil
}

func validateSegment(s string) error {
	if s == "" || s != strings.TrimSpace(s) || strings.HasSuffix(s, ".") || strings.ContainsAny(s, `<>:"/\|?*`) {
		return appErr(domain.ErrPathOutsideRoot, "invalid Windows path segment")
	}
	base := strings.ToUpper(strings.SplitN(s, ".", 2)[0])
	if reserved[base] {
		return appErr(domain.ErrPathOutsideRoot, "Windows reserved device name")
	}
	return nil
}
func canonicalExisting(p string) (string, error) {
	a, e := filepath.Abs(p)
	if e != nil {
		return "", e
	}
	return filepath.EvalSymlinks(a)
}
func canonicalNearest(p string) (string, error) {
	q := p
	for {
		x, e := filepath.EvalSymlinks(q)
		if e == nil {
			suffix, _ := filepath.Rel(q, p)
			return filepath.Join(x, suffix), nil
		}
		next := filepath.Dir(q)
		if next == q {
			return "", e
		}
		q = next
	}
}
func within(root, target string) bool {
	rel, e := filepath.Rel(root, target)
	return e == nil && rel != ".." && !strings.HasPrefix(rel, ".."+string(filepath.Separator)) && !filepath.IsAbs(rel)
}
func appErr(code domain.ErrorCode, msg string) error {
	return &domain.AppError{Code: code, Message: msg, Suggestion: "use a relative path inside the configured workspace"}
}
func (r *Resolver) Root(i int) string { return r.roots[i] }
func EnsureReadable(p string) error {
	f, e := os.Open(p)
	if e != nil {
		return fmt.Errorf("open: %w", e)
	}
	return f.Close()
}
