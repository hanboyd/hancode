package filesystem

import (
	"fmt"
	"os"
	"path/filepath"
)

func AtomicWrite(path string, data []byte, mode os.FileMode) error {
	dir := filepath.Dir(path)
	f, err := os.CreateTemp(dir, ".notegen-mcp-*.tmp")
	if err != nil {
		return err
	}
	tmp := f.Name()
	ok := false
	defer func() {
		f.Close()
		if !ok {
			_ = os.Remove(tmp)
		}
	}()
	if err = f.Chmod(mode); err != nil {
		return err
	}
	if _, err = f.Write(data); err != nil {
		return err
	}
	if err = f.Sync(); err != nil {
		return err
	}
	if err = f.Close(); err != nil {
		return err
	}
	// Windows cannot replace an existing file with os.Rename. Move the old name aside,
	// then restore it if installation of the new file fails.
	backup := tmp + ".old"
	existed := false
	if _, err = os.Stat(path); err == nil {
		existed = true
		if err = os.Rename(path, backup); err != nil {
			return fmt.Errorf("stage existing file: %w", err)
		}
	} else if !os.IsNotExist(err) {
		return err
	}
	if err = os.Rename(tmp, path); err != nil {
		if existed {
			_ = os.Rename(backup, path)
		}
		return fmt.Errorf("install temporary file: %w", err)
	}
	if existed {
		_ = os.Remove(backup)
	}
	ok = true
	return nil
}
