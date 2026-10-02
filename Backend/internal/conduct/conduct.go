// Package conduct keeps reports and commendations: records about a match's
// participants (ADR-047). Each match's reports form one case, which this
// domain holds until a moderation service exists. A report is an allegation,
// never a sanction; a commendation is recorded and nothing reads it.
package conduct

import (
	"context"
	"errors"
	"regexp"
	"slices"
	"strings"
	"time"
	"unicode"
	"unicode/utf8"
)

// Errors describing rule violations. Callers map them to response codes.
var (
	ErrNotParticipant   = errors.New("the account did not play the match")
	ErrUnknownPlayer    = errors.New("no other human participant has that name")
	ErrInvalidReason    = errors.New("not a report reason")
	ErrDetailsTooLong   = errors.New("the details are too long")
	ErrReportClosed     = errors.New("the match can no longer be reported")
	ErrInvalidRequest   = errors.New("invalid request")
	ErrNotTeammate      = errors.New("the player is not a teammate")
	ErrCommendClosed    = errors.New("the match can no longer be commended")
	ErrAlreadyCommended = errors.New("the account already commended a teammate in the match")
	// ErrReportNotFound and ErrNoCommendation are a store's answers.
	ErrReportNotFound = errors.New("report not found")
	ErrNoCommendation = errors.New("no commendation")
)

// Tuning is the conduct configuration, validated by the config package
// (ADR-047 §6).
type Tuning struct {
	// Reasons a report may give.
	Reasons []string
	// DetailsMaxCharacters is the longest a report's cleaned details may be.
	DetailsMaxCharacters int
	// ReportWindow is how long after a match ends it may be reported;
	// CommendWindow how long a teammate may be commended.
	ReportWindow  time.Duration
	CommendWindow time.Duration
}

// Participant is a human who played a match, as the match recorded them.
type Participant struct {
	AccountID   string
	DisplayName string
	Side        string
}

// PlayedMatch is a match an account played, as conduct needs it.
type PlayedMatch struct {
	// EndedAt is zero unless the match ended with a result.
	EndedAt      time.Time
	Participants []Participant
}

// Matches is what conduct needs of the match domain.
type Matches interface {
	// Played returns a match the account played, or ErrNotParticipant.
	Played(ctx context.Context, accountID, matchID string) (PlayedMatch, error)
}

// Report is one participant's report of another (Moderation Bible §1).
type Report struct {
	MatchID    string
	ReporterID string
	ReportedID string
	// ReportedName is the name the match recorded for the reported player.
	ReportedName string
	Reason       string
	Details      string
	ClientID     string
	CreatedAt    time.Time
}

// Case is a match's reports, grouped (§1); each report stays distinct.
type Case struct {
	MatchID  string
	OpenedAt time.Time
	Reports  []Report
}

// Commendation is one participant's commendation of a teammate (UX-58).
type Commendation struct {
	MatchID       string
	CommenderID   string
	CommendedID   string
	CommendedName string
	CreatedAt     time.Time
}

// Tx is one storage transaction.
type Tx interface {
	// LockCase serializes a match's conduct records until the transaction ends.
	LockCase(matchID string) error
	// Report returns the reporter's report of a player in a match, or
	// ErrReportNotFound.
	Report(matchID, reporterID, reportedID string) (Report, error)
	// ReportByClientID returns the reporter's report with that client ID, or
	// ErrReportNotFound.
	ReportByClientID(reporterID, clientID string) (Report, error)
	// OpenCase opens the match's case unless it is open.
	OpenCase(matchID string, at time.Time) error
	AddReport(r Report) error
	// Commendation returns the commender's commendation in a match, or
	// ErrNoCommendation.
	Commendation(matchID, commenderID string) (Commendation, error)
	AddCommendation(c Commendation) error
}

// Store persists conduct records.
type Store interface {
	InTx(ctx context.Context, fn func(context.Context, Tx) error) error
	// ReportsBy returns a reporter's reports in a match.
	ReportsBy(ctx context.Context, matchID, reporterID string) ([]Report, error)
	// CommendationBy returns the commender's commendation in a match, or
	// ErrNoCommendation.
	CommendationBy(ctx context.Context, matchID, commenderID string) (Commendation, error)
	// Case returns a match's case; ok is false while it has none.
	Case(ctx context.Context, matchID string) (c Case, ok bool, err error)
	Commendations(ctx context.Context, matchID string) ([]Commendation, error)
}

// clientIDPattern is the shape of the IDs clients generate for reports.
var clientIDPattern = regexp.MustCompile(`^[A-Za-z0-9-]{8,64}$`)

// CleanDetails returns a report's details as they are stored: invalid UTF-8
// replaced, control characters turned into spaces and the ends trimmed.
func CleanDetails(details string) string {
	details = strings.ToValidUTF8(details, "�")
	details = strings.Map(func(r rune) rune {
		if unicode.IsControl(r) {
			return ' '
		}
		return r
	}, details)
	return strings.TrimSpace(details)
}

// Service applies the conduct rules.
type Service struct {
	store   Store
	matches Matches
	tuning  Tuning
	now     func() time.Time
}

// NewService builds a Service. now is injectable for tests.
func NewService(store Store, matches Matches, tuning Tuning, now func() time.Time) *Service {
	return &Service{store: store, matches: matches, tuning: tuning, now: now}
}

// ReportRequest is a report as a participant files it.
type ReportRequest struct {
	// ReportedName is the player's name as the results show it.
	ReportedName string
	Reason       string
	Details      string
	ClientID     string
}

// Report files a participant's report of another human participant
// (ADR-047 §2). A second report of the same player in the same match, or a
// resend with the same client ID, returns the first. It changes nothing but
// the match's case.
func (s *Service) Report(ctx context.Context, actor, matchID string, req ReportRequest) (Report, error) {
	if !clientIDPattern.MatchString(req.ClientID) {
		return Report{}, ErrInvalidRequest
	}
	if !slices.Contains(s.tuning.Reasons, req.Reason) {
		return Report{}, ErrInvalidReason
	}
	details := CleanDetails(req.Details)
	if utf8.RuneCountInString(details) > s.tuning.DetailsMaxCharacters {
		return Report{}, ErrDetailsTooLong
	}
	played, err := s.matches.Played(ctx, actor, matchID)
	if err != nil {
		return Report{}, err
	}
	now := s.now()
	if played.EndedAt.IsZero() || !now.Before(played.EndedAt.Add(s.tuning.ReportWindow)) {
		return Report{}, ErrReportClosed
	}
	reported, ok := other(played, actor, req.ReportedName)
	if !ok {
		return Report{}, ErrUnknownPlayer
	}
	var out Report
	err = s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		if err := tx.LockCase(matchID); err != nil {
			return err
		}
		// A repeat never floods the case (§1): the first report stands for it.
		for _, find := range []func() (Report, error){
			func() (Report, error) { return tx.ReportByClientID(actor, req.ClientID) },
			func() (Report, error) { return tx.Report(matchID, actor, reported.AccountID) },
		} {
			prior, err := find()
			if err == nil {
				out = prior
				return nil
			}
			if !errors.Is(err, ErrReportNotFound) {
				return err
			}
		}
		if err := tx.OpenCase(matchID, now); err != nil {
			return err
		}
		out = Report{MatchID: matchID, ReporterID: actor, ReportedID: reported.AccountID, ReportedName: reported.DisplayName, Reason: req.Reason,
			Details: details, ClientID: req.ClientID, CreatedAt: now}
		return tx.AddReport(out)
	})
	if err != nil {
		return Report{}, err
	}
	return out, nil
}

// Commend records the actor's commendation of one teammate from the
// immediate results (ADR-047 §3): once per match. A resend for the same
// teammate returns the first.
func (s *Service) Commend(ctx context.Context, actor, matchID, name string) (Commendation, error) {
	played, err := s.matches.Played(ctx, actor, matchID)
	if err != nil {
		return Commendation{}, err
	}
	now := s.now()
	if played.EndedAt.IsZero() || !now.Before(played.EndedAt.Add(s.tuning.CommendWindow)) {
		return Commendation{}, ErrCommendClosed
	}
	teammate, ok := other(played, actor, name)
	if !ok {
		return Commendation{}, ErrUnknownPlayer
	}
	me, _ := participant(played, actor)
	if teammate.Side != me.Side {
		return Commendation{}, ErrNotTeammate
	}
	var out Commendation
	err = s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		if err := tx.LockCase(matchID); err != nil {
			return err
		}
		prior, err := tx.Commendation(matchID, actor)
		if err == nil {
			if prior.CommendedID != teammate.AccountID {
				return ErrAlreadyCommended
			}
			out = prior
			return nil
		}
		if !errors.Is(err, ErrNoCommendation) {
			return err
		}
		out = Commendation{MatchID: matchID, CommenderID: actor, CommendedID: teammate.AccountID, CommendedName: teammate.DisplayName, CreatedAt: now}
		return tx.AddCommendation(out)
	})
	if err != nil {
		return Commendation{}, err
	}
	return out, nil
}

// Record is what the player did in a match: whom they reported and whom they
// commended, by the names the match recorded (ADR-047 §4). It names the other
// human participants a report or commendation may be about, and carries the
// reasons a report may give and how long its details may be, so a client
// offers what the backend accepts.
type Record struct {
	Reported             []string
	Commended            string
	Players              []RecordPlayer
	Reasons              []string
	DetailsMaxCharacters int
}

// RecordPlayer is another human participant: whom a player menu opens for.
type RecordPlayer struct {
	Name string
	// Teammate is whether they played on the actor's side, so may be commended.
	Teammate bool
}

// Record returns the actor's own conduct records for a match it played.
func (s *Service) Record(ctx context.Context, actor, matchID string) (Record, error) {
	played, err := s.matches.Played(ctx, actor, matchID)
	if err != nil {
		return Record{}, err
	}
	me, _ := participant(played, actor)
	reports, err := s.store.ReportsBy(ctx, matchID, actor)
	if err != nil {
		return Record{}, err
	}
	out := Record{Reported: []string{}, Players: []RecordPlayer{}, Reasons: slices.Clone(s.tuning.Reasons), DetailsMaxCharacters: s.tuning.DetailsMaxCharacters}
	for _, p := range played.Participants {
		if p.AccountID != actor {
			out.Players = append(out.Players, RecordPlayer{Name: p.DisplayName, Teammate: p.Side == me.Side})
		}
	}
	for _, r := range reports {
		out.Reported = append(out.Reported, r.ReportedName)
	}
	slices.Sort(out.Reported)
	c, err := s.store.CommendationBy(ctx, matchID, actor)
	if err == nil {
		out.Commended = c.CommendedName
	} else if !errors.Is(err, ErrNoCommendation) {
		return Record{}, err
	}
	return out, nil
}

// DevMatch returns a match's case and commendations, for the development
// route (local only).
func (s *Service) DevMatch(ctx context.Context, matchID string) (Case, []Commendation, error) {
	c, _, err := s.store.Case(ctx, matchID)
	if err != nil {
		return Case{}, nil, err
	}
	commendations, err := s.store.Commendations(ctx, matchID)
	return c, commendations, err
}

// other returns the human participant called name, other than the actor.
// Display names are unique (Profiles & Identity Bible §4); case is ignored.
func other(m PlayedMatch, actor, name string) (Participant, bool) {
	name = strings.TrimSpace(name)
	for _, p := range m.Participants {
		if p.AccountID != actor && strings.EqualFold(p.DisplayName, name) {
			return p, true
		}
	}
	return Participant{}, false
}

func participant(m PlayedMatch, accountID string) (Participant, bool) {
	i := slices.IndexFunc(m.Participants, func(p Participant) bool { return p.AccountID == accountID })
	if i < 0 {
		return Participant{}, false
	}
	return m.Participants[i], true
}
