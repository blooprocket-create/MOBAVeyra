package matchmaking

import (
	"reflect"
	"testing"
)

// parties builds candidates whose accounts are named p<index>-<member>.
func parties(sizes ...int) []Candidate {
	out := make([]Candidate, len(sizes))
	for i, size := range sizes {
		out[i].PartyID = string(rune('a' + i))
		for m := 0; m < size; m++ {
			out[i].Accounts = append(out[i].Accounts, out[i].PartyID+string(rune('0'+m)))
		}
	}
	return out
}

func noBlocks(string, string) bool { return false }

func TestGroupFillsBothSidesOldestFirst(t *testing.T) {
	got := Group(parties(1, 1, 1, 1, 1), 1, noBlocks)
	want := []Grouping{{SideA: []int{0}, SideB: []int{1}}, {SideA: []int{2}, SideB: []int{3}}}
	if !reflect.DeepEqual(got, want) {
		t.Fatalf("got %+v, want %+v; the fifth waits", got, want)
	}
}

func TestGroupNeverSplitsAParty(t *testing.T) {
	// Five a side: 4 + 1 against 3 + 2, never a split of the 4.
	got := Group(parties(4, 3, 2, 1), 5, noBlocks)
	want := []Grouping{{SideA: []int{0, 3}, SideB: []int{1, 2}}}
	if !reflect.DeepEqual(got, want) {
		t.Fatalf("got %+v, want %+v", got, want)
	}
	if got := Group(parties(3, 3), 2, noBlocks); len(got) != 0 {
		t.Fatalf("parties larger than a side never match: %+v", got)
	}
	if got := Group(parties(2, 2, 1), 2, noBlocks); !reflect.DeepEqual(got, []Grouping{{SideA: []int{0}, SideB: []int{1}}}) {
		t.Fatalf("got %+v", got)
	}
}

func TestGroupKeepsBlockedAccountsApartOnEitherTeam(t *testing.T) {
	candidates := parties(1, 1, 1)
	blocked := func(a, b string) bool {
		pair := map[string]bool{a: true, b: true}
		return pair["a0"] && pair["b0"]
	}
	// The oldest two block each other: the oldest waits for the third.
	got := Group(candidates, 1, blocked)
	if !reflect.DeepEqual(got, []Grouping{{SideA: []int{0}, SideB: []int{2}}}) {
		t.Fatalf("got %+v", got)
	}
	// Nor on one team: with two a side, only three non-conflicting players exist.
	if got := Group(parties(1, 1, 1, 1), 2, blocked); len(got) != 0 {
		t.Fatalf("a match needs all four, two of whom block each other: %+v", got)
	}
}

func TestGroupSkipsAnOversizedOldestPartyWithoutStallingTheRest(t *testing.T) {
	got := Group(parties(3, 1, 1), 1, noBlocks)
	if !reflect.DeepEqual(got, []Grouping{{SideA: []int{1}, SideB: []int{2}}}) {
		t.Fatalf("got %+v", got)
	}
}
