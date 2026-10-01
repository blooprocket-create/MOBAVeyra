package matchmaking

// Candidate is one queued party as the matchmaker sees it.
type Candidate struct {
	PartyID  string
	Accounts []string
}

// Grouping is one match formed from candidates: the index of each party on
// each side.
type Grouping struct {
	SideA []int
	SideB []int
}

// Group forms as many matches as it can from candidates, which are in queue
// order, oldest first (Parties & Social Bible §2, §6):
//   - each side holds exactly its size in players: sides[0] on side A and
//     sides[1] on side B, which is 0 for a co-op mode, whose other side is AI
//     (ADR-038 §2);
//   - a party is never split, and a party larger than a side never matches;
//   - two accounts where either blocks the other are never in one match, on
//     either team, even if that means a longer wait.
//
// Each match is built around the oldest party not yet matched. A depth-first
// search goes through the parties after it in queue order, trying each on
// side A, then on side B, then leaving it out, and backtracks when the sides
// cannot both be filled: parties of 1, 1, 4 and 4 for sides of five make 1+4
// against 1+4. The search prefers the oldest parties, and skips any branch
// whose remaining parties could not fill both sides even without blocks. It
// takes at most searchLimit steps around one party; a party whose search runs
// out waits for the next pass.
//
// PROVISIONAL (ADR-010 §11): canon leaves the algorithm open (§10: skill,
// latency, region, party-size pairing). This is the simplest rule that keeps
// the locked constraints.
func Group(candidates []Candidate, sides [2]int, searchLimit int, blocked func(a, b string) bool) []Grouping {
	used := make([]bool, len(candidates))
	fits := fillable(candidates, used, sides)
	var out []Grouping
	for anchor := range candidates {
		if used[anchor] {
			continue
		}
		s := search{candidates: candidates, used: used, fits: fits, limit: searchLimit, blocked: blocked, room: sides}
		if !s.around(anchor) {
			continue
		}
		grouping := Grouping{SideA: s.sides[0], SideB: s.sides[1]}
		for _, i := range append(append([]int(nil), grouping.SideA...), grouping.SideB...) {
			used[i] = true
		}
		out = append(out, grouping)
		fits = fillable(candidates, used, sides)
	}
	return out
}

// fillable returns fits, where fits[i][a][b] says whether the unused parties
// from index i on can make up exactly a players on side A and b on side B, up
// to each side's size, ignoring blocks.
func fillable(candidates []Candidate, used []bool, sides [2]int) [][][]bool {
	fits := make([][][]bool, len(candidates)+1)
	for i := len(candidates); i >= 0; i-- {
		fits[i] = make([][]bool, sides[0]+1)
		for a := range fits[i] {
			fits[i][a] = make([]bool, sides[1]+1)
			for b := range fits[i][a] {
				if i == len(candidates) {
					fits[i][a][b] = a == 0 && b == 0
					continue
				}
				ok := fits[i+1][a][b]
				if size := len(candidates[i].Accounts); !used[i] && size > 0 {
					ok = ok || (size <= a && fits[i+1][a-size][b]) || (size <= b && fits[i+1][a][b-size])
				}
				fits[i][a][b] = ok
			}
		}
	}
	return fits
}

// search looks for one match around an anchor party.
type search struct {
	candidates []Candidate
	used       []bool
	fits       [][][]bool
	blocked    func(a, b string) bool
	// steps counts the search's steps, up to limit.
	steps, limit int
	// sides holds the parties placed on side A and side B, room what is left
	// of each side, and chosen every placed account.
	sides  [2][]int
	room   [2]int
	chosen []string
}

// around places the anchor on side A and fills both sides from the parties
// after it. False if it cannot within the step limit.
func (s *search) around(anchor int) bool {
	size := len(s.candidates[anchor].Accounts)
	if size == 0 || size > s.room[0] {
		return false
	}
	s.place(anchor, 0)
	return s.fill(anchor + 1)
}

// fill places parties from index i on until both sides are full, and reports
// whether it could.
func (s *search) fill(i int) bool {
	if s.room[0] == 0 && s.room[1] == 0 {
		return true
	}
	if i == len(s.candidates) || !s.fits[i][s.room[0]][s.room[1]] || s.steps >= s.limit {
		return false
	}
	s.steps++
	accounts := s.candidates[i].Accounts
	if !s.used[i] && len(accounts) > 0 && !conflicts(s.chosen, accounts, s.blocked) {
		for side := range s.room {
			if len(accounts) > s.room[side] {
				continue
			}
			s.place(i, side)
			if s.fill(i + 1) {
				return true
			}
			s.unplace(side)
		}
	}
	return s.fill(i + 1)
}

func (s *search) place(i, side int) {
	s.sides[side] = append(s.sides[side], i)
	s.room[side] -= len(s.candidates[i].Accounts)
	s.chosen = append(s.chosen, s.candidates[i].Accounts...)
}

// unplace takes back the last party placed on side.
func (s *search) unplace(side int) {
	last := s.sides[side][len(s.sides[side])-1]
	size := len(s.candidates[last].Accounts)
	s.sides[side] = s.sides[side][:len(s.sides[side])-1]
	s.room[side] += size
	s.chosen = s.chosen[:len(s.chosen)-size]
}

// conflicts reports whether any account in accounts and any already chosen
// block each other.
func conflicts(chosen, accounts []string, blocked func(a, b string) bool) bool {
	for _, a := range accounts {
		for _, b := range chosen {
			if blocked(a, b) {
				return true
			}
		}
	}
	return false
}
