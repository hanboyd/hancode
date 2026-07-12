package markdown

import (
	"bufio"
	"strings"
)

func Title(content, fallback string) string {
	s := bufio.NewScanner(strings.NewReader(content))
	for s.Scan() {
		line := strings.TrimSpace(s.Text())
		if strings.HasPrefix(line, "# ") {
			return strings.TrimSpace(line[2:])
		}
	}
	return fallback
}
func Section(content, heading string) (string, bool) {
	lines := strings.Split(content, "\n")
	want := strings.TrimSpace(strings.TrimLeft(heading, "#"))
	start, end, level := -1, len(lines), 0
	for i, line := range lines {
		t := strings.TrimSpace(line)
		if !strings.HasPrefix(t, "#") {
			continue
		}
		n := 0
		for n < len(t) && t[n] == '#' {
			n++
		}
		if n == len(t) || t[n] != ' ' {
			continue
		}
		name := strings.TrimSpace(t[n:])
		if start < 0 && strings.EqualFold(name, want) {
			start = i
			level = n
			continue
		}
		if start >= 0 && n <= level {
			end = i
			break
		}
	}
	if start < 0 {
		return "", false
	}
	return strings.Join(lines[start:end], "\n"), true
}
func ReplaceSection(content, heading, replacement string) (string, bool) {
	old, ok := Section(content, heading)
	if !ok {
		return content, false
	}
	return strings.Replace(content, old, replacement, 1), true
}
func Lines(content string, start, end int) string {
	a := strings.Split(content, "\n")
	if start < 1 {
		start = 1
	}
	if end <= 0 || end > len(a) {
		end = len(a)
	}
	if start > end {
		return ""
	}
	return strings.Join(a[start-1:end], "\n")
}
