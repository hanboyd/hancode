package index

import (
	"context"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"sort"
	"strconv"
	"strings"
	"time"
	"unicode"
)

func Tokenize(s string) map[string]int {
	out := map[string]int{}
	var han, word []rune
	flushHan := func() {
		if len(han) == 1 {
			out[string(han)]++
		}
		for n := 2; n <= 3; n++ {
			for i := 0; i+n <= len(han); i++ {
				out[string(han[i:i+n])]++
			}
		}
		han = nil
	}
	flushWord := func() {
		if len(word) > 0 {
			out[strings.ToLower(string(word))]++
			word = nil
		}
	}
	for _, r := range []rune(s) {
		if unicode.Is(unicode.Han, r) {
			flushWord()
			han = append(han, r)
		} else {
			flushHan()
			if unicode.IsLetter(r) || unicode.IsDigit(r) {
				word = append(word, unicode.ToLower(r))
			} else {
				flushWord()
			}
		}
	}
	flushHan()
	flushWord()
	return out
}

type SearchQuery struct {
	Query                                                    string
	SearchIn                                                 string
	Folder                                                   string
	Tags                                                     []string
	ExactPhrase                                              bool
	ExcludeFolders                                           []string
	Limit                                                    int
	Cursor                                                   string
	CreatedAfter, CreatedBefore, UpdatedAfter, UpdatedBefore time.Time
}
type SearchResult struct {
	NoteID       string   `json:"note_id"`
	RelativePath string   `json:"relative_path"`
	Title        string   `json:"title"`
	Snippet      string   `json:"snippet"`
	MatchType    string   `json:"match_type"`
	MatchedTerms []string `json:"matched_terms"`
	Score        float64  `json:"score"`
	UpdatedAt    string   `json:"updated_at"`
	ContentHash  string   `json:"content_hash"`
}
type SearchPage struct {
	Results       []SearchResult `json:"results"`
	NextCursor    string         `json:"next_cursor,omitempty"`
	TotalEstimate int            `json:"total_estimate"`
	IndexState    string         `json:"index_state"`
}

func (s *Store) Search(ctx context.Context, q SearchQuery) (SearchPage, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	if strings.TrimSpace(q.Query) == "" {
		return SearchPage{}, fmt.Errorf("query is required")
	}
	if q.Limit < 1 {
		q.Limit = 20
	}
	if q.Limit > 100 {
		q.Limit = 100
	}
	offset := decodeCursor(q.Cursor)
	tokens := Tokenize(q.Query)
	terms := make([]string, 0, len(tokens))
	for x := range tokens {
		terms = append(terms, x)
	}
	sort.Strings(terms)
	rows, err := s.db.QueryContext(ctx, "SELECT note_id,relative_path,title,content,tags_json,created_at,updated_at,content_hash FROM notes ORDER BY normalized_path")
	if err != nil {
		return SearchPage{}, err
	}
	defer rows.Close()
	needle := strings.ToLower(q.Query)
	weights := map[string]float64{"title": 5, "tags": 4, "path": 3, "content": 1}
	var all []SearchResult
	for rows.Next() {
		var id, path, title, content, tagsJSON, created, updated, hash string
		if err = rows.Scan(&id, &path, &title, &content, &tagsJSON, &created, &updated, &hash); err != nil {
			return SearchPage{}, err
		}
		if !pathAllowed(path, q.Folder, q.ExcludeFolders) {
			continue
		}
		var tags []string
		_ = json.Unmarshal([]byte(tagsJSON), &tags)
		if !containsAllFold(tags, q.Tags) {
			continue
		}
		ct, _ := time.Parse(time.RFC3339Nano, created)
		ut, _ := time.Parse(time.RFC3339Nano, updated)
		if !inRange(ct, q.CreatedAfter, q.CreatedBefore) || !inRange(ut, q.UpdatedAfter, q.UpdatedBefore) {
			continue
		}
		fields := map[string]string{"title": title, "path": path, "tags": strings.Join(tags, " "), "content": content}
		score := 0.0
		match := ""
		for field, text := range fields {
			if q.SearchIn != "" && q.SearchIn != "all" && q.SearchIn != field {
				continue
			}
			lower := strings.ToLower(text)
			phrase := strings.Contains(lower, needle)
			hits := 0
			ft := Tokenize(text)
			for _, term := range terms {
				if ft[term] > 0 {
					hits++
				}
			}
			matched := phrase
			if !q.ExactPhrase && !matched {
				matched = len(terms) > 0 && hits == len(terms)
			}
			if matched {
				score += float64(hits)*weights[field] + 10*weights[field]
				if match == "" || weights[field] > weights[match] {
					match = field
				}
			}
		}
		if match != "" {
			all = append(all, SearchResult{id, path, title, makeSnippet(content, q.Query), match, terms, score, updated, hash})
		}
	}
	sort.SliceStable(all, func(i, j int) bool {
		if all[i].Score == all[j].Score {
			return all[i].RelativePath < all[j].RelativePath
		}
		return all[i].Score > all[j].Score
	})
	total := len(all)
	if offset > total {
		offset = total
	}
	end := offset + q.Limit
	if end > total {
		end = total
	}
	p := SearchPage{Results: all[offset:end], TotalEstimate: total, IndexState: "ready"}
	if end < total {
		p.NextCursor = base64.RawURLEncoding.EncodeToString([]byte(strconv.Itoa(end)))
	}
	return p, nil
}
func decodeCursor(c string) int {
	if c == "" {
		return 0
	}
	b, e := base64.RawURLEncoding.DecodeString(c)
	if e != nil {
		return 0
	}
	n, _ := strconv.Atoi(string(b))
	return n
}
func pathAllowed(path, folder string, excluded []string) bool {
	p := strings.ToLower(filepathSlash(path))
	if folder != "" && !strings.HasPrefix(p, strings.ToLower(strings.Trim(filepathSlash(folder), "/")+"/")) {
		return false
	}
	for _, x := range excluded {
		if strings.HasPrefix(p, strings.ToLower(strings.Trim(filepathSlash(x), "/")+"/")) {
			return false
		}
	}
	return true
}
func filepathSlash(s string) string { return strings.ReplaceAll(s, "\\", "/") }
func containsAllFold(have, want []string) bool {
	if len(want) == 0 {
		return true
	}
	m := map[string]bool{}
	for _, x := range have {
		m[strings.ToLower(x)] = true
	}
	for _, x := range want {
		if !m[strings.ToLower(x)] {
			return false
		}
	}
	return true
}
func inRange(v, a, b time.Time) bool {
	return (a.IsZero() || !v.Before(a)) && (b.IsZero() || !v.After(b))
}
func makeSnippet(content, query string) string {
	rs := []rune(content)
	i := strings.Index(strings.ToLower(content), strings.ToLower(query))
	if i < 0 {
		if len(rs) > 240 {
			rs = rs[:240]
		}
		return string(rs)
	}
	before := len([]rune(content[:i]))
	start := before - 80
	if start < 0 {
		start = 0
	}
	end := before + len([]rune(query)) + 120
	if end > len(rs) {
		end = len(rs)
	}
	return string(rs[start:end])
}
