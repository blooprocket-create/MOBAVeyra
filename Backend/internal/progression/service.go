package progression

import (
	"context"
	"errors"
	"regexp"
	"slices"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/account"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
)

// Errors describing rule violations. Callers map them to response codes.
var (
	ErrNotForSale       = errors.New("the Vanguard is not released")
	ErrAlreadyOwned     = errors.New("the account already owns the Vanguard")
	ErrInsufficient     = errors.New("the balance does not cover the price")
	ErrInvalidPurchase  = errors.New("invalid purchase")
	ErrPurchaseConflict = errors.New("the purchase ID was already used for another purchase")
	ErrNoGrant          = errors.New("no grant for the match and account")
	// ErrAlreadyGranted is a store's answer to a second grant for one match
	// and account; the service treats it as done.
	ErrAlreadyGranted = errors.New("the match already granted the account")
	// ErrPurchaseNotFound is a store's answer for an unknown purchase ID.
	ErrPurchaseNotFound = errors.New("purchase not found")
)

// Currency is an account currency a purchase spends (Bible §3).
type Currency string

const (
	// CurrencyFlux is the earned account currency.
	CurrencyFlux Currency = "flux"
	// CurrencyRefinedFlux is the premium account currency.
	CurrencyRefinedFlux Currency = "refinedFlux"
)

// purchaseIDPattern is the shape of the IDs clients generate for purchases.
var purchaseIDPattern = regexp.MustCompile(`^[A-Za-z0-9-]{8,64}$`)

// Account is an account's progression. A new account is at Level 1 with
// nothing earned.
type Account struct {
	AccountID   string
	Level       int
	LevelXP     int64
	LifetimeXP  int64
	Flux        int64
	RefinedFlux int64
}

// Mastery is an account's Mastery of one Vanguard.
type Mastery struct {
	VanguardID     string
	Level          int
	LevelPoints    int64
	LifetimePoints int64
}

// Grant is what one match gave one account: the reward ledger's line.
type Grant struct {
	MatchID   string
	AccountID string
	// Reason says why account XP was withheld; empty when it was earned.
	Reason        Reason
	AccountXP     int64
	LevelBefore   int
	LevelAfter    int
	Flux          int64
	RefinedFlux   int64
	VanguardID    string
	MasteryPoints int64
	MasteryBefore int
	MasteryAfter  int
	GrantedAt     time.Time
}

// Purchase is one Vanguard bought: the spending ledger's line.
type Purchase struct {
	PurchaseID  string
	AccountID   string
	VanguardID  string
	Currency    Currency
	Price       int64
	PurchasedAt time.Time
}

// DevAdjustment is currency the development route granted; it keeps the
// balances reconcilable with the ledgers.
type DevAdjustment struct {
	AccountID   string
	Flux        int64
	RefinedFlux int64
	GrantedAt   time.Time
}

// Tx is one storage transaction.
type Tx interface {
	// LockAccount returns the account's progression, locked until the
	// transaction ends; an account with none starts at Level 1.
	LockAccount(accountID string) (Account, error)
	SaveAccount(a Account) error
	// LockMastery returns the account's Mastery of a Vanguard, locked; one
	// never played starts at Level 1.
	LockMastery(accountID, vanguardID string) (Mastery, error)
	SaveMastery(accountID string, m Mastery) error
	// AddGrant records a grant, or returns ErrAlreadyGranted when the match
	// already granted the account.
	AddGrant(g Grant) error
	// Purchase returns a recorded purchase, or ErrPurchaseNotFound.
	Purchase(purchaseID string) (Purchase, error)
	AddPurchase(p Purchase) error
	AddDevAdjustment(a DevAdjustment) error
}

// Store persists progression. InTx passes its callback a ctx carrying the
// transaction, so other domains' stores called with it join the transaction.
type Store interface {
	InTx(ctx context.Context, fn func(context.Context, Tx) error) error
	Account(ctx context.Context, accountID string) (Account, error)
	Masteries(ctx context.Context, accountID string) ([]Mastery, error)
	// Grant returns what a match gave an account, or ErrNoGrant.
	Grant(ctx context.Context, matchID, accountID string) (Grant, error)
}

// Ownership is what progression needs of the account domain: which Vanguards
// an account owns and may pick, and granting one bought.
type Ownership interface {
	Vanguards(ctx context.Context, accountID string) (account.Availability, error)
	Entitlements(ctx context.Context, accountID string) ([]account.Entitlement, error)
	// GrantPurchase gives the account a Vanguard it bought, in ctx's
	// transaction.
	GrantPurchase(ctx context.Context, accountID, vanguardID string, at time.Time) error
}

// Service applies the progression rules.
type Service struct {
	store     Store
	ownership Ownership
	tuning    Tuning
	// categories maps each matchmade mode to its category (ADR-039 §6).
	categories map[string]string
	now        func() time.Time
}

// NewService builds a Service. now is injectable for tests.
func NewService(store Store, ownership Ownership, tuning Tuning, categories map[string]string, now func() time.Time) *Service {
	return &Service{store: store, ownership: ownership, tuning: tuning, categories: categories, now: now}
}

// Grant gives each rostered participant of an ended match what it earned
// (ADR-045 §3). It implements match.Rewards: the match service calls it in
// the transaction that stores the result, through ctx, so a grant commits
// with its result. A participant the match already granted is skipped, which
// makes a replayed result grant nothing twice.
func (s *Service) Grant(ctx context.Context, m *match.Match) error {
	r := m.Result
	if r == nil {
		return nil
	}
	category := s.categories[m.Mode]
	now := s.now()
	return s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		for _, p := range r.Participants {
			rostered, ok := m.Participant(p.AccountID)
			if !ok {
				continue
			}
			acc, err := tx.LockAccount(p.AccountID)
			if err != nil {
				return err
			}
			earnsXP, earnsMastery, reason := Eligibility(s.tuning.AccountXP, m, category, p, acc.Level)
			won := r.Winner != "" && rostered.Side == r.Winner
			g := Grant{MatchID: m.ID, AccountID: p.AccountID, Reason: reason, LevelBefore: acc.Level, LevelAfter: acc.Level,
				VanguardID: rostered.VanguardID, GrantedAt: now}
			if earnsXP {
				g.AccountXP = MatchXP(s.tuning.AccountXP, r, won)
				standing, reached := s.tuning.AccountLevels.Add(Standing{Level: acc.Level, Into: acc.LevelXP}, g.AccountXP)
				g.LevelAfter = standing.Level
				g.Flux, g.RefinedFlux = LevelUpRewards(s.tuning, reached)
				acc.Level, acc.LevelXP = standing.Level, standing.Into
				acc.LifetimeXP += g.AccountXP
				acc.Flux += g.Flux
				acc.RefinedFlux += g.RefinedFlux
			}
			var mastery Mastery
			if earnsMastery && rostered.VanguardID != "" {
				if mastery, err = tx.LockMastery(p.AccountID, rostered.VanguardID); err != nil {
					return err
				}
				g.MasteryPoints = MatchMastery(s.tuning.Mastery, r, won, statisticsOf(r, p.AccountID))
				g.MasteryBefore = mastery.Level
				standing, _ := s.tuning.Mastery.Levels.Add(Standing{Level: mastery.Level, Into: mastery.LevelPoints}, g.MasteryPoints)
				mastery.Level, mastery.LevelPoints = standing.Level, standing.Into
				mastery.LifetimePoints += g.MasteryPoints
				g.MasteryAfter = mastery.Level
			}
			if err := tx.AddGrant(g); errors.Is(err, ErrAlreadyGranted) {
				continue
			} else if err != nil {
				return err
			}
			if err := tx.SaveAccount(acc); err != nil {
				return err
			}
			if g.MasteryPoints > 0 {
				if err := tx.SaveMastery(p.AccountID, mastery); err != nil {
					return err
				}
			}
		}
		return nil
	})
}

// statisticsOf finds the account's line on the result's scoreboard; zero
// statistics when the server sent none.
func statisticsOf(r *match.Result, accountID string) match.PlayerStatistics {
	for _, p := range r.Players {
		if p.AccountID == accountID {
			return p.Statistics
		}
	}
	return match.PlayerStatistics{}
}

// Summary is an account's progression as the client reads it.
type Summary struct {
	Account
	// LevelNeed is what the current level takes to the next.
	LevelNeed int64
}

// Progression returns the account's level, XP and balances.
func (s *Service) Progression(ctx context.Context, accountID string) (Summary, error) {
	a, err := s.store.Account(ctx, accountID)
	if err != nil {
		return Summary{}, err
	}
	return Summary{Account: a, LevelNeed: s.tuning.AccountLevels.Need(a.Level)}, nil
}

// CollectionEntry is one released Vanguard in an account's Collection (Bible
// §4): whatever the account owns, with its own Mastery.
type CollectionEntry struct {
	VanguardID string
	Owned      bool
	// Source says how an owned Vanguard was gained.
	Source   account.Source
	Rotation bool
	Price    Price
	// Purchasable is whether it can be bought now: released and not owned.
	Purchasable bool
	Mastery     Mastery
	// MasteryNeed is what its Mastery Level takes to the next.
	MasteryNeed int64
	EmoteTier   int
}

// Collection returns every released Vanguard, in the catalog's order, with
// the account's ownership and Mastery of each.
func (s *Service) Collection(ctx context.Context, accountID string) ([]CollectionEntry, error) {
	available, err := s.ownership.Vanguards(ctx, accountID)
	if err != nil {
		return nil, err
	}
	entitlements, err := s.ownership.Entitlements(ctx, accountID)
	if err != nil {
		return nil, err
	}
	masteries, err := s.store.Masteries(ctx, accountID)
	if err != nil {
		return nil, err
	}
	sources := map[string]account.Source{}
	for _, e := range entitlements {
		sources[e.VanguardID] = e.Source
	}
	out := make([]CollectionEntry, 0, len(available.Released))
	for _, id := range available.Released {
		source, owned := sources[id]
		m := Mastery{VanguardID: id, Level: 1}
		if i := slices.IndexFunc(masteries, func(x Mastery) bool { return x.VanguardID == id }); i >= 0 {
			m = masteries[i]
		}
		out = append(out, CollectionEntry{VanguardID: id, Owned: owned, Source: source, Rotation: slices.Contains(available.Rotation, id),
			Price: s.tuning.Prices[id], Purchasable: !owned, Mastery: m, MasteryNeed: s.tuning.Mastery.Levels.Need(m.Level),
			EmoteTier: EmoteTier(s.tuning.Mastery, m.Level)})
	}
	return out, nil
}

// Buy spends one currency on a released Vanguard the account does not own,
// and grants it (Bible §3). purchaseID makes it idempotent: repeating a
// purchase returns its first outcome, and the ID with a different Vanguard
// or currency is ErrPurchaseConflict.
func (s *Service) Buy(ctx context.Context, accountID, purchaseID, vanguardID string, currency Currency) (Purchase, Summary, error) {
	if !purchaseIDPattern.MatchString(purchaseID) || (currency != CurrencyFlux && currency != CurrencyRefinedFlux) {
		return Purchase{}, Summary{}, ErrInvalidPurchase
	}
	price, released := s.tuning.Prices[vanguardID]
	if !released {
		return Purchase{}, Summary{}, ErrNotForSale
	}
	var bought Purchase
	err := s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		// The account's lock orders this purchase against its others.
		acc, err := tx.LockAccount(accountID)
		if err != nil {
			return err
		}
		earlier, err := tx.Purchase(purchaseID)
		switch {
		case err == nil:
			if earlier.AccountID != accountID || earlier.VanguardID != vanguardID || earlier.Currency != currency {
				return ErrPurchaseConflict
			}
			bought = earlier
			return nil
		case !errors.Is(err, ErrPurchaseNotFound):
			return err
		}
		available, err := s.ownership.Vanguards(ctx, accountID)
		if err != nil {
			return err
		}
		if slices.Contains(available.Owned, vanguardID) {
			return ErrAlreadyOwned
		}
		bought = Purchase{PurchaseID: purchaseID, AccountID: accountID, VanguardID: vanguardID, Currency: currency, PurchasedAt: s.now()}
		switch currency {
		case CurrencyFlux:
			bought.Price = price.Flux
			if acc.Flux < bought.Price {
				return ErrInsufficient
			}
			acc.Flux -= bought.Price
		case CurrencyRefinedFlux:
			bought.Price = price.RefinedFlux
			if acc.RefinedFlux < bought.Price {
				return ErrInsufficient
			}
			acc.RefinedFlux -= bought.Price
		}
		if err := tx.AddPurchase(bought); err != nil {
			return err
		}
		if err := tx.SaveAccount(acc); err != nil {
			return err
		}
		return s.ownership.GrantPurchase(ctx, accountID, vanguardID, bought.PurchasedAt)
	})
	if err != nil {
		return Purchase{}, Summary{}, err
	}
	summary, err := s.Progression(ctx, accountID)
	return bought, summary, err
}

// DevGrant adds currency to an account, recorded as a development
// adjustment. Callers expose it only in development (ADR-045 §6).
func (s *Service) DevGrant(ctx context.Context, accountID string, flux, refinedFlux int64) (Summary, error) {
	if flux < 0 || refinedFlux < 0 {
		return Summary{}, ErrInvalidPurchase
	}
	err := s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		acc, err := tx.LockAccount(accountID)
		if err != nil {
			return err
		}
		acc.Flux += flux
		acc.RefinedFlux += refinedFlux
		if err := tx.AddDevAdjustment(DevAdjustment{AccountID: accountID, Flux: flux, RefinedFlux: refinedFlux, GrantedAt: s.now()}); err != nil {
			return err
		}
		return tx.SaveAccount(acc)
	})
	if err != nil {
		return Summary{}, err
	}
	return s.Progression(ctx, accountID)
}

// MatchRewards returns what a match gave the account, or ErrNoGrant while
// its result is not adjudicated.
func (s *Service) MatchRewards(ctx context.Context, matchID, accountID string) (Grant, error) {
	return s.store.Grant(ctx, matchID, accountID)
}

// DevGrantEnabled reports whether the development grant route may be
// mounted.
func (s *Service) DevGrantEnabled() bool { return s.tuning.DevGrant }
