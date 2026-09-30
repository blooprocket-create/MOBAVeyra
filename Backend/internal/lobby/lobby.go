// Package lobby owns invite-only custom lobbies (Custom Matches Bible §1–§4;
// ADR-021): who is in one, where each human and bot sits, the host, the
// session's rules, and invitations. Rules are applied to an in-memory Lobby
// value inside a storage transaction, so every rule here is a pure function
// that is unit-tested without a database.
package lobby

import (
	"errors"
	"math"
	"sort"
	"time"
)

// Sides, as matches name them.
const (
	SideA = "A"
	SideB = "B"
)

// Sides lists both sides in order.
var Sides = []string{SideA, SideB}

// Difficulties a bot may play at (Custom Matches Bible §3).
const (
	Beginner     = "beginner"
	Intermediate = "intermediate"
)

// Difficulties lists them in order, easiest first, for a client's choices.
var Difficulties = []string{Beginner, Intermediate}

// Status is where the lobby is on its way to a match.
type Status string

const (
	// Open lobbies change freely.
	Open Status = "open"
	// Selecting lobbies are in the champion select their launch opened; they
	// are fixed until it ends. A select that ends without a match reopens it.
	Selecting Status = "selecting"
)

// Errors describing rule violations. Callers map them to user-facing codes.
var (
	ErrNotInLobby        = errors.New("not in a lobby")
	ErrAlreadyInLobby    = errors.New("already in a lobby")
	ErrLobbyNotFound     = errors.New("lobby not found")
	ErrNotHost           = errors.New("only the host can do that")
	ErrNotMember         = errors.New("account is not in the lobby")
	ErrLobbyFull         = errors.New("the lobby has no empty slot")
	ErrLobbyLocked       = errors.New("the lobby is fixed while its champion select runs")
	ErrNoSuchSlot        = errors.New("no such slot")
	ErrSlotTaken         = errors.New("that slot is taken")
	ErrNotABot           = errors.New("that slot holds no bot")
	ErrUnknownVanguard   = errors.New("not a released Vanguard")
	ErrUnknownDifficulty = errors.New("unknown bot difficulty")
	ErrDuplicateVanguard = errors.New("that Vanguard is already on the side")
	ErrGoldOutOfRange    = errors.New("starting Gold is out of range")
	ErrVictoryNeedsSides = errors.New("victory needs a Vanguard on each side")
	ErrSelf              = errors.New("cannot target yourself")
	ErrNotFriends        = errors.New("not friends")
	ErrBlocked           = errors.New("blocked")
	ErrInviteNotFound    = errors.New("invitation not found or expired")
	ErrBusy              = errors.New("in a match, champion select or queue")
	ErrNoHuman           = errors.New("a custom match needs a human player")
	ErrLaunchUnavailable = errors.New("custom matches cannot be launched")
)

// Seat is a place on a side: its side and its index there, from 0.
type Seat struct {
	Side  string
	Index int
}

// Member is one human in the lobby, and where they sit.
type Member struct {
	AccountID string
	JoinedAt  time.Time
	Seat      Seat
}

// Bot is one AI Vanguard the host placed (§1–§3).
type Bot struct {
	Seat       Seat
	VanguardID string
	Difficulty string
}

// Settings are the session rules the host sets (§4). They apply to the one
// match this lobby launches, never to any other.
type Settings struct {
	VictoryEnabled bool
	// VictoryChosen records that the host set victory themselves; until they
	// do, it follows whether both sides have a Vanguard (ADR-021 §1).
	VictoryChosen bool
	// StartingGold is the Gold each Vanguard starts with; nil plays the game's
	// own starting Gold.
	StartingGold *float64
}

// Lobby is the authoritative lobby state.
type Lobby struct {
	ID       string
	HostID   string
	Status   Status
	Members  []Member
	Bots     []Bot
	Settings Settings
}

// Limits are the validated lobby configuration the pure rules need.
type Limits struct {
	PlayersPerSide int
	// Released are the Vanguards a bot may play: every released one (§2).
	Released        map[string]bool
	StartingGoldMin float64
	StartingGoldMax float64
}

// BotVanguards lists the Vanguards a bot may play, sorted, for a client's
// choices.
func (l Limits) BotVanguards() []string {
	out := make([]string, 0, len(l.Released))
	for id, ok := range l.Released {
		if ok {
			out = append(out, id)
		}
	}
	sort.Strings(out)
	return out
}

// New is a lobby its host has just created, seated first on side A.
func New(id, host string, now time.Time) Lobby {
	l := Lobby{
		ID:      id,
		HostID:  host,
		Status:  Open,
		Members: []Member{{AccountID: host, JoinedAt: now, Seat: Seat{Side: SideA}}},
	}
	l.followSides()
	return l
}

// IsMember reports whether accountID is one of the lobby's humans.
func (l *Lobby) IsMember(accountID string) bool { return l.memberIndex(accountID) >= 0 }

// MemberIDs lists the lobby's humans, in join order.
func (l *Lobby) MemberIDs() []string {
	ids := make([]string, len(l.Members))
	for i, m := range l.Members {
		ids[i] = m.AccountID
	}
	return ids
}

// Occupant describes what sits in a seat: a member, a bot, or neither.
func (l *Lobby) Occupant(seat Seat) (member *Member, bot *Bot) {
	for i := range l.Members {
		if l.Members[i].Seat == seat {
			return &l.Members[i], nil
		}
	}
	for i := range l.Bots {
		if l.Bots[i].Seat == seat {
			return nil, &l.Bots[i]
		}
	}
	return nil, nil
}

// SideCount is how many Vanguards, humans and bots, sit on side.
func (l *Lobby) SideCount(side string) int {
	n := 0
	for _, m := range l.Members {
		if m.Seat.Side == side {
			n++
		}
	}
	for _, b := range l.Bots {
		if b.Seat.Side == side {
			n++
		}
	}
	return n
}

func (l *Lobby) humansOn(side string) int {
	n := 0
	for _, m := range l.Members {
		if m.Seat.Side == side {
			n++
		}
	}
	return n
}

func (l *Lobby) memberIndex(accountID string) int {
	for i, m := range l.Members {
		if m.AccountID == accountID {
			return i
		}
	}
	return -1
}

func (l *Lobby) botIndex(seat Seat) int {
	for i, b := range l.Bots {
		if b.Seat == seat {
			return i
		}
	}
	return -1
}

func (l *Lobby) requireHost(accountID string) error {
	if l.HostID != accountID {
		return ErrNotHost
	}
	return nil
}

func (l *Lobby) requireOpen() error {
	if l.Status != Open {
		return ErrLobbyLocked
	}
	return nil
}

func validSeat(seat Seat, limits Limits) bool {
	return (seat.Side == SideA || seat.Side == SideB) && seat.Index >= 0 && seat.Index < limits.PlayersPerSide
}

// firstEmpty is side's lowest empty seat, or false when it has none.
func (l *Lobby) firstEmpty(side string, limits Limits) (Seat, bool) {
	for i := 0; i < limits.PlayersPerSide; i++ {
		seat := Seat{Side: side, Index: i}
		if m, b := l.Occupant(seat); m == nil && b == nil {
			return seat, true
		}
	}
	return Seat{}, false
}

// Join seats a new human who accepted an invitation: on the side with fewer
// humans, side A on a tie, at its first empty seat; the other side's when that
// one is full (ADR-021 §8).
func (l *Lobby) Join(accountID string, limits Limits, now time.Time) error {
	if err := l.requireOpen(); err != nil {
		return err
	}
	if l.IsMember(accountID) {
		return ErrAlreadyInLobby
	}
	first, second := SideA, SideB
	if l.humansOn(SideB) < l.humansOn(SideA) {
		first, second = SideB, SideA
	}
	seat, ok := l.firstEmpty(first, limits)
	if !ok {
		if seat, ok = l.firstEmpty(second, limits); !ok {
			return ErrLobbyFull
		}
	}
	l.Members = append(l.Members, Member{AccountID: accountID, JoinedAt: now, Seat: seat})
	l.followSides()
	return nil
}

// Remove takes a human out for any reason: leaving, a kick, or a block. A
// departing host hands the lobby to the human who has been in it longest
// (ADR-021 §8). It reports whether no human is left, which closes the lobby.
func (l *Lobby) Remove(accountID string) (empty bool, err error) {
	i := l.memberIndex(accountID)
	if i < 0 {
		return false, ErrNotMember
	}
	l.Members = append(l.Members[:i], l.Members[i+1:]...)
	if len(l.Members) == 0 {
		return true, nil
	}
	if l.HostID == accountID {
		sorted := append([]Member(nil), l.Members...)
		sort.SliceStable(sorted, func(a, b int) bool { return sorted[a].JoinedAt.Before(sorted[b].JoinedAt) })
		l.HostID = sorted[0].AccountID
	}
	l.followSides()
	return false, nil
}

// Kick lets the host remove another human.
func (l *Lobby) Kick(hostID, targetID string) error {
	if err := l.requireHost(hostID); err != nil {
		return err
	}
	if err := l.requireOpen(); err != nil {
		return err
	}
	if hostID == targetID {
		return ErrSelf
	}
	_, err := l.Remove(targetID)
	return err
}

// Move puts a human in an empty seat; the host decides where every human
// plays (§1).
func (l *Lobby) Move(hostID, targetID string, seat Seat, limits Limits) error {
	if err := l.requireHost(hostID); err != nil {
		return err
	}
	if err := l.requireOpen(); err != nil {
		return err
	}
	i := l.memberIndex(targetID)
	if i < 0 {
		return ErrNotMember
	}
	if !validSeat(seat, limits) {
		return ErrNoSuchSlot
	}
	if l.Members[i].Seat == seat {
		return nil
	}
	if m, b := l.Occupant(seat); m != nil || b != nil {
		return ErrSlotTaken
	}
	l.Members[i].Seat = seat
	l.followSides()
	return nil
}

// SetBot places a bot in an empty seat, or changes the bot already there: any
// released Vanguard (§2) at a difficulty (§3). A side holds each Vanguard
// once, bots and humans alike (ADR-021 §8); humans are held to it when they
// pick.
func (l *Lobby) SetBot(hostID string, bot Bot, limits Limits) error {
	if err := l.requireHost(hostID); err != nil {
		return err
	}
	if err := l.requireOpen(); err != nil {
		return err
	}
	if !validSeat(bot.Seat, limits) {
		return ErrNoSuchSlot
	}
	if !limits.Released[bot.VanguardID] {
		return ErrUnknownVanguard
	}
	if bot.Difficulty != Beginner && bot.Difficulty != Intermediate {
		return ErrUnknownDifficulty
	}
	if m, _ := l.Occupant(bot.Seat); m != nil {
		return ErrSlotTaken
	}
	for _, other := range l.Bots {
		if other.Seat != bot.Seat && other.Seat.Side == bot.Seat.Side && other.VanguardID == bot.VanguardID {
			return ErrDuplicateVanguard
		}
	}
	if i := l.botIndex(bot.Seat); i >= 0 {
		l.Bots[i] = bot
	} else {
		l.Bots = append(l.Bots, bot)
	}
	l.followSides()
	return nil
}

// RemoveBot empties a bot's seat.
func (l *Lobby) RemoveBot(hostID string, seat Seat) error {
	if err := l.requireHost(hostID); err != nil {
		return err
	}
	if err := l.requireOpen(); err != nil {
		return err
	}
	i := l.botIndex(seat)
	if i < 0 {
		return ErrNotABot
	}
	l.Bots = append(l.Bots[:i], l.Bots[i+1:]...)
	l.followSides()
	return nil
}

// SetSettings lets the host set the session's rules (§4). Starting Gold, when
// set, lies within the configured range. Victory needs a Vanguard on each
// side, since a side with none could never lose (§1: 1v0 is open-ended).
func (l *Lobby) SetSettings(hostID string, victory bool, startingGold *float64, limits Limits) error {
	if err := l.requireHost(hostID); err != nil {
		return err
	}
	if err := l.requireOpen(); err != nil {
		return err
	}
	if startingGold != nil {
		g := *startingGold
		if math.IsNaN(g) || math.IsInf(g, 0) || g < limits.StartingGoldMin || g > limits.StartingGoldMax {
			return ErrGoldOutOfRange
		}
		v := g
		startingGold = &v
	}
	if victory && !l.bothSides() {
		return ErrVictoryNeedsSides
	}
	l.Settings = Settings{VictoryEnabled: victory, VictoryChosen: true, StartingGold: startingGold}
	return nil
}

func (l *Lobby) bothSides() bool { return l.SideCount(SideA) > 0 && l.SideCount(SideB) > 0 }

// followSides keeps victory true to the sides: never with an empty side, and,
// until the host has chosen, on exactly when both sides have a Vanguard.
func (l *Lobby) followSides() {
	switch {
	case !l.bothSides():
		l.Settings.VictoryEnabled = false
	case !l.Settings.VictoryChosen:
		l.Settings.VictoryEnabled = true
	}
}

// CheckLaunch is whether the host may launch now: an open lobby with at least
// one human (§1). Whether each human is free is the service's to ask.
func (l *Lobby) CheckLaunch(hostID string) error {
	if err := l.requireHost(hostID); err != nil {
		return err
	}
	if err := l.requireOpen(); err != nil {
		return err
	}
	if len(l.Members) == 0 {
		return ErrNoHuman
	}
	return nil
}

// launch is what the lobby hands champion select: its humans and bots each
// in side, then seat, order, and its rules.
func (l *Lobby) launch() Launch {
	out := Launch{LobbyID: l.ID, HostID: l.HostID, Members: append([]Member(nil), l.Members...), Bots: append([]Bot(nil), l.Bots...), Settings: l.Settings}
	bySeat := func(a, b Seat) bool {
		if a.Side != b.Side {
			return a.Side < b.Side
		}
		return a.Index < b.Index
	}
	sort.SliceStable(out.Members, func(i, j int) bool { return bySeat(out.Members[i].Seat, out.Members[j].Seat) })
	sort.SliceStable(out.Bots, func(i, j int) bool { return bySeat(out.Bots[i].Seat, out.Bots[j].Seat) })
	return out
}

// BeginSelecting fixes the lobby while the champion select it launched runs.
func (l *Lobby) BeginSelecting() { l.Status = Selecting }

// Reopen returns the lobby to its members after a select that made no match.
func (l *Lobby) Reopen() { l.Status = Open }
