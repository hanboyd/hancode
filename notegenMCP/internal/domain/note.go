package domain

import "time"

type Note struct {
	ID          string         `json:"note_id"`
	Path        string         `json:"relative_path"`
	Title       string         `json:"title"`
	Content     string         `json:"content,omitempty"`
	Tags        []string       `json:"tags,omitempty"`
	Frontmatter map[string]any `json:"frontmatter,omitempty"`
	Hash        string         `json:"content_hash"`
	CreatedAt   time.Time      `json:"created_at"`
	ModifiedAt  time.Time      `json:"modified_at"`
	Size        int64          `json:"file_size"`
	Truncated   bool           `json:"truncated,omitempty"`
}
