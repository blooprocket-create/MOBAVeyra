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
//   - each side holds exactly teamSize players;
//   - a party is never split, and a party larger than a side never matches;
//   - two accounts where either blocks the other are never in one match, on
//     either team, even if that means a longer wait.
//
// Each match is built around the oldest party not yet matched, taking the
// parties after it in order, first onto side A and then onto side B. That
// keeps the oldest parties first; a party that cannot be placed now waits.
//
// PROVISIONAL (ADR-010 §11): canon leaves the algorithm open (§10: skill,
// latency, region, party-size pairing). This is the simplest rule that keeps
// the locked constraints.
func Group(candidates []Candidate, teamSize int, blocked func(a, b string) bool) []Grouping {
	used := make([]bool, len(candidates))
	var out []Grouping
	for anchor := range candidates {
		if used[anchor] {
			continue
		}
		grouping, ok := formAround(candidates, used, anchor, teamSize, blocked)
		if !ok {
			continue
		}
		for _, i := range append(append([]int(nil), grouping.SideA...), grouping.SideB...) {
			used[i] = true
		}
		out = append(out, grouping)
	}
	return out
}

// formAround tries to build one match starting with the anchor party.
func formAround(candidates []Candidate, used []bool, anchor, teamSize int, blocked func(a, b string) bool) (Grouping, bool) {
	var grouping Grouping
	var sizes [2]int
	var chosen []string
	for i := anchor; i < len(candidates); i++ {
		candidate := candidates[i]
		size := len(candidate.Accounts)
		if i == anchor && (size == 0 || size > teamSize) {
			return Grouping{}, false
		}
		if used[i] || size == 0 || conflicts(chosen, candidate.Accounts, blocked) {
			continue
		}
		switch {
		case sizes[0]+size <= teamSize:
			grouping.SideA = append(grouping.SideA, i)
			sizes[0] += size
		case sizes[1]+size <= teamSize:
			grouping.SideB = append(grouping.SideB, i)
			sizes[1] += size
		default:
			continue
		}
		chosen = append(chosen, candidate.Accounts...)
		if sizes[0] == teamSize && sizes[1] == teamSize {
			return grouping, true
		}
	}
	return Grouping{}, false
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
