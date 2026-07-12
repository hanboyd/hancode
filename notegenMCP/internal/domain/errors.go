package domain

import "fmt"

type ErrorCode string

const (
	ErrNoteNotFound         ErrorCode = "NOTE_NOT_FOUND"
	ErrPathOutsideRoot      ErrorCode = "PATH_OUTSIDE_ROOT"
	ErrPathConflict         ErrorCode = "PATH_CONFLICT"
	ErrVersionConflict      ErrorCode = "VERSION_CONFLICT"
	ErrFileLocked           ErrorCode = "FILE_LOCKED"
	ErrInvalidMarkdown      ErrorCode = "INVALID_MARKDOWN"
	ErrInvalidFrontmatter   ErrorCode = "INVALID_FRONTMATTER"
	ErrIndexUnavailable     ErrorCode = "INDEX_UNAVAILABLE"
	ErrIndexCorrupted       ErrorCode = "INDEX_CORRUPTED"
	ErrBatchValidation      ErrorCode = "BATCH_VALIDATION_FAILED"
	ErrBatchExecution       ErrorCode = "BATCH_EXECUTION_FAILED"
	ErrBatchRollback        ErrorCode = "BATCH_ROLLBACK_FAILED"
	ErrGitConflict          ErrorCode = "GIT_CONFLICT"
	ErrPermissionDenied     ErrorCode = "PERMISSION_DENIED"
	ErrOperationTooLarge    ErrorCode = "OPERATION_TOO_LARGE"
	ErrAttachmentTooLarge   ErrorCode = "ATTACHMENT_TOO_LARGE"
	ErrConfigInvalid        ErrorCode = "CONFIG_INVALID"
	ErrWorkspaceUnavailable ErrorCode = "WORKSPACE_UNAVAILABLE"
)

type AppError struct {
	Code                ErrorCode `json:"code"`
	Message             string    `json:"message"`
	Retryable           bool      `json:"retryable"`
	Suggestion          string    `json:"suggestion,omitempty"`
	RelativePath        string    `json:"relative_path,omitempty"`
	CurrentHash         string    `json:"current_hash,omitempty"`
	CurrentModifiedTime string    `json:"current_modified_time,omitempty"`
	Cause               error     `json:"-"`
}

func (e *AppError) Error() string { return fmt.Sprintf("%s: %s", e.Code, e.Message) }
func (e *AppError) Unwrap() error { return e.Cause }
