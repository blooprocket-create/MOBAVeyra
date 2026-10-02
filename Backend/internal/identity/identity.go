// Package identity owns accounts, Veyra session tokens and launch codes.
//
// Players register and sign in through an external identity provider
// (Firebase Authentication, ADR-038). The provider proves who someone is; the
// launcher exchanges the provider's short-lived credential here for a Veyra
// launcher session, so the provider never reaches the game and replacing it
// changes neither the game nor the launch handshake (ADR-005 H3).
//
// The launcher logs in and holds a launcher session. It exchanges that session
// for a single-use, short-lived launch code bound to the account and client
// build version, and hands the code to the game over a private channel
// (ADR-005). The game redeems the code for its own game session. Tokens and
// codes are opaque random values; only their SHA-256 hashes are stored.
package identity

import (
	"context"
	"errors"
	"regexp"
	"strings"
	"time"
	"unicode/utf8"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/secret"
)

// SessionKind separates launcher sessions from game sessions; neither is
// accepted where the other is required.
type SessionKind string

const (
	SessionLauncher SessionKind = "launcher"
	SessionGame     SessionKind = "game"
)

// Token prefixes make leaked secrets recognisable to secret scanners.
const (
	prefixLauncherSession = "vls_"
	prefixGameSession     = "vgs_"
	prefixLaunchCode      = "vlc_"
)

// Display names a player chooses at registration (Profiles & Identity Bible
// §4). The bible leaves allowed characters and lengths to a later pass; until
// then names are 3–16 ASCII letters, digits and underscores, unique without
// regard to case. Provisional (ADR-038 §4).
const (
	MinDisplayNameLength = 3
	MaxDisplayNameLength = 16
)

var displayNamePattern = regexp.MustCompile(`^[A-Za-z0-9_]+$`)

// maxBuildVersionLength bounds the client build identifier (protocol limit).
const maxBuildVersionLength = 64

var buildVersionPattern = regexp.MustCompile(`^[A-Za-z0-9._+-]+$`)

// Errors returned to callers. Authentication failures are deliberately
// indistinguishable so callers learn nothing about which check failed.
var (
	ErrInvalidCredentials  = errors.New("invalid credentials")
	ErrInvalidBuildVersion = errors.New("invalid build version")
	ErrDevLoginDisabled    = errors.New("dev login disabled")
	ErrNotFound            = errors.New("not found")
	// ErrBuildMismatch is returned by Store.RedeemLaunchCode when the code
	// was valid but bound to a different build; the code is still consumed.
	ErrBuildMismatch = errors.New("launch code build mismatch")
	// ErrNotDevAccount is returned when seeding a dev account whose name is
	// already held by an account that was not created as a dev account.
	ErrNotDevAccount = errors.New("account exists and is not a dev account")

	// ErrPlayerLoginDisabled: no identity provider is configured.
	ErrPlayerLoginDisabled = errors.New("player login disabled")
	// ErrNotRegistered: the provider vouched for the player, but they have
	// not yet created a Veyra account (chosen a display name).
	ErrNotRegistered = errors.New("no Veyra account for this sign-in")
	// ErrAlreadyRegistered: this provider identity already has an account.
	ErrAlreadyRegistered = errors.New("already registered")
	// ErrDisplayNameTaken: another account holds the name, in any case.
	ErrDisplayNameTaken = errors.New("display name taken")
	// ErrInvalidDisplayName: the name breaks the display-name rules.
	ErrInvalidDisplayName = errors.New("invalid display name")
)

// ProviderIdentity is who an external identity provider says a player is:
// the provider's name and its stable, unique ID for that user.
type ProviderIdentity struct {
	Provider string
	Subject  string
}

// Verifier checks a credential an identity provider issued (a Firebase ID
// token) and says whom it identifies. A credential that is malformed, forged,
// expired or meant for another project fails with ErrInvalidCredentials.
type Verifier interface {
	Verify(ctx context.Context, credential string) (ProviderIdentity, error)
}

// Account is a player account.
type Account struct {
	ID          string
	DisplayName string
}

// Session is a stored session; TokenHash is the SHA-256 of the bearer token.
type Session struct {
	TokenHash    []byte
	AccountID    string
	Kind         SessionKind
	BuildVersion string // game sessions only
	CreatedAt    time.Time
	ExpiresAt    time.Time
}

// LaunchCode is a stored launch code; CodeHash is the SHA-256 of the code.
type LaunchCode struct {
	CodeHash     []byte
	AccountID    string
	BuildVersion string
	CreatedAt    time.Time
	ExpiresAt    time.Time
}

// Store persists identity state.
type Store interface {
	// EnsureDevAccount creates the named dev account if absent and returns
	// it. It fails with ErrNotDevAccount if a non-dev account holds the name.
	EnsureDevAccount(ctx context.Context, displayName string) (Account, error)
	// DevAccountByDisplayName finds an account created by dev seeding only.
	DevAccountByDisplayName(ctx context.Context, displayName string) (Account, error)
	AccountByID(ctx context.Context, id string) (Account, error)
	// AccountByDisplayName finds any account by its unique display name.
	AccountByDisplayName(ctx context.Context, displayName string) (Account, error)
	// AccountsByIDs returns the accounts that exist among ids.
	AccountsByIDs(ctx context.Context, ids []string) ([]Account, error)
	CreateSession(ctx context.Context, s Session) error
	// ActiveSession returns the unexpired, unrevoked session with this hash.
	ActiveSession(ctx context.Context, tokenHash []byte, now time.Time) (Session, error)
	CreateLaunchCode(ctx context.Context, c LaunchCode) error
	// RedeemLaunchCode atomically consumes an unexpired, unconsumed code and
	// creates newSession for the code's account, returning that account.
	// newSession.AccountID is filled from the code. If the code's build
	// differs from buildVersion, the code is consumed, no session is created
	// and ErrBuildMismatch is returned. If creating the session fails,
	// nothing changes and the code stays redeemable. An unknown, expired or
	// already-consumed code returns ErrNotFound.
	RedeemLaunchCode(ctx context.Context, codeHash []byte, buildVersion string, now time.Time, newSession Session) (Account, error)
	// AccountByProvider finds the account linked to a provider identity, or
	// returns ErrNotFound.
	AccountByProvider(ctx context.Context, who ProviderIdentity) (Account, error)
	// CreateProviderAccount atomically creates an account with displayName
	// and links it to who. It fails with ErrDisplayNameTaken if any account
	// holds the name in any letter case, and with ErrAlreadyRegistered if who
	// is already linked; either way nothing is created.
	CreateProviderAccount(ctx context.Context, who ProviderIdentity, displayName string) (Account, error)

	// Name changes (ADR-049). Each call joins the caller's unit of work.
	// NameState returns what identity keeps about the account's name.
	NameState(ctx context.Context, accountID string) (NameState, error)
	// LockNameState returns it locked until the unit of work ends, so one
	// account's changes run one at a time.
	LockNameState(ctx context.Context, accountID string) (NameState, error)
	// HolderOfName returns the account holding name in any letter case,
	// locked until the unit of work ends, or ErrNotFound.
	HolderOfName(ctx context.Context, name string) (Account, error)
	// SetDisplayName gives the account name, or fails with
	// ErrDisplayNameTaken if another account holds it in any case. A
	// voluntary change uses the free change and starts the cooldown at at; a
	// required one clears RenameRequired and touches neither.
	SetDisplayName(ctx context.Context, accountID, name string, voluntary bool, at time.Time) error
	// RequireRename gives a claimed account placeholder and marks it to
	// choose a new name.
	RequireRename(ctx context.Context, accountID, placeholder string) error
	// TouchLauncherLogin records a successful launcher login at at.
	TouchLauncherLogin(ctx context.Context, accountID string, at time.Time) error
}

// Settings are the validated lifetimes the service needs.
type Settings struct {
	LauncherSessionLifetime time.Duration
	GameSessionLifetime     time.Duration
	LaunchCodeLifetime      time.Duration
	DevLoginEnabled         bool
	// PlayerLogin verifies identity-provider credentials; nil disables
	// player registration and sign-in (ErrPlayerLoginDisabled).
	PlayerLogin Verifier
}

// Service implements identity operations.
type Service struct {
	store    Store
	settings Settings
	now      func() time.Time
	// Name changes, once SetNames enables them (ADR-049).
	names  *NameSettings
	payer  Payer
	atomic func(ctx context.Context, fn func(context.Context) error) error
}

// NewService builds a Service. now is injectable for tests.
func NewService(store Store, settings Settings, now func() time.Time) *Service {
	return &Service{store: store, settings: settings, now: now}
}

// IssuedToken is a freshly issued bearer secret. The plaintext is returned
// exactly once and never stored.
type IssuedToken struct {
	Token     string
	ExpiresAt time.Time
}

// DevLogin issues a launcher session for a seeded account without a password.
// It exists only in the local environment (enforced by config validation).
func (s *Service) DevLogin(ctx context.Context, displayName string) (IssuedToken, Account, error) {
	if !s.settings.DevLoginEnabled {
		return IssuedToken{}, Account{}, ErrDevLoginDisabled
	}
	acct, err := s.store.DevAccountByDisplayName(ctx, displayName)
	if errors.Is(err, ErrNotFound) {
		return IssuedToken{}, Account{}, ErrInvalidCredentials
	}
	if err != nil {
		return IssuedToken{}, Account{}, err
	}
	if err := s.touchLauncherLogin(ctx, acct.ID); err != nil {
		return IssuedToken{}, Account{}, err
	}
	tok, err := s.createSession(ctx, acct.ID, SessionLauncher, "", s.settings.LauncherSessionLifetime, prefixLauncherSession)
	return tok, acct, err
}

// PlayerLogin exchanges an identity-provider credential for a launcher
// session. A valid credential for someone who has no Veyra account yet fails
// with ErrNotRegistered, so the launcher can ask them for a display name.
func (s *Service) PlayerLogin(ctx context.Context, credential string) (IssuedToken, Account, error) {
	who, err := s.verify(ctx, credential)
	if err != nil {
		return IssuedToken{}, Account{}, err
	}
	acct, err := s.store.AccountByProvider(ctx, who)
	if errors.Is(err, ErrNotFound) {
		return IssuedToken{}, Account{}, ErrNotRegistered
	}
	if err != nil {
		return IssuedToken{}, Account{}, err
	}
	// A launcher login keeps the account's name from being claimed (ADR-049 §1).
	if err := s.touchLauncherLogin(ctx, acct.ID); err != nil {
		return IssuedToken{}, Account{}, err
	}
	tok, err := s.createSession(ctx, acct.ID, SessionLauncher, "", s.settings.LauncherSessionLifetime, prefixLauncherSession)
	return tok, acct, err
}

// Register creates the Veyra account for an identity-provider credential,
// with the display name the player chose, and signs it in. The credential
// proves who is registering; the name is checked and reserved atomically.
func (s *Service) Register(ctx context.Context, credential, displayName string) (IssuedToken, Account, error) {
	// The name is checked first: it costs nothing and leaks nothing.
	if err := ValidateDisplayName(displayName); err != nil {
		return IssuedToken{}, Account{}, err
	}
	who, err := s.verify(ctx, credential)
	if err != nil {
		return IssuedToken{}, Account{}, err
	}
	acct, err := s.store.CreateProviderAccount(ctx, who, displayName)
	if err != nil {
		return IssuedToken{}, Account{}, err
	}
	if err := s.touchLauncherLogin(ctx, acct.ID); err != nil {
		return IssuedToken{}, Account{}, err
	}
	tok, err := s.createSession(ctx, acct.ID, SessionLauncher, "", s.settings.LauncherSessionLifetime, prefixLauncherSession)
	return tok, acct, err
}

func (s *Service) verify(ctx context.Context, credential string) (ProviderIdentity, error) {
	if s.settings.PlayerLogin == nil {
		return ProviderIdentity{}, ErrPlayerLoginDisabled
	}
	if credential == "" {
		return ProviderIdentity{}, ErrInvalidCredentials
	}
	return s.settings.PlayerLogin.Verify(ctx, credential)
}

// ValidateDisplayName reports whether a chosen display name keeps the rules.
func ValidateDisplayName(name string) error {
	n := utf8.RuneCountInString(name)
	if n < MinDisplayNameLength || n > MaxDisplayNameLength || !displayNamePattern.MatchString(name) {
		return ErrInvalidDisplayName
	}
	return nil
}

// IssueLaunchCode exchanges a launcher session for a single-use launch code.
func (s *Service) IssueLaunchCode(ctx context.Context, launcherToken, buildVersion string) (IssuedToken, error) {
	if err := validateBuildVersion(buildVersion); err != nil {
		return IssuedToken{}, err
	}
	sess, err := s.authenticate(ctx, launcherToken, SessionLauncher)
	if err != nil {
		return IssuedToken{}, err
	}
	code, hash, err := secret.New(prefixLaunchCode)
	if err != nil {
		return IssuedToken{}, err
	}
	now := s.now()
	lc := LaunchCode{
		CodeHash:     hash,
		AccountID:    sess.AccountID,
		BuildVersion: buildVersion,
		CreatedAt:    now,
		ExpiresAt:    now.Add(s.settings.LaunchCodeLifetime),
	}
	if err := s.store.CreateLaunchCode(ctx, lc); err != nil {
		return IssuedToken{}, err
	}
	return IssuedToken{Token: code, ExpiresAt: lc.ExpiresAt}, nil
}

// RedeemLaunchCode consumes a launch code and issues a game session. The code
// is consumed even when the build version does not match, so a code can
// never be tried twice.
func (s *Service) RedeemLaunchCode(ctx context.Context, code, buildVersion string) (IssuedToken, Account, error) {
	if err := validateBuildVersion(buildVersion); err != nil {
		return IssuedToken{}, Account{}, err
	}
	if !strings.HasPrefix(code, prefixLaunchCode) {
		return IssuedToken{}, Account{}, ErrInvalidCredentials
	}
	tok, hash, err := secret.New(prefixGameSession)
	if err != nil {
		return IssuedToken{}, Account{}, err
	}
	now := s.now()
	sess := Session{
		TokenHash:    hash,
		Kind:         SessionGame,
		BuildVersion: buildVersion,
		CreatedAt:    now,
		ExpiresAt:    now.Add(s.settings.GameSessionLifetime),
	}
	acct, err := s.store.RedeemLaunchCode(ctx, secret.Hash(code), buildVersion, now, sess)
	if errors.Is(err, ErrNotFound) || errors.Is(err, ErrBuildMismatch) {
		return IssuedToken{}, Account{}, ErrInvalidCredentials
	}
	if err != nil {
		return IssuedToken{}, Account{}, err
	}
	return IssuedToken{Token: tok, ExpiresAt: sess.ExpiresAt}, acct, nil
}

// LookupAccount finds an account by display name.
func (s *Service) LookupAccount(ctx context.Context, displayName string) (Account, error) {
	return s.store.AccountByDisplayName(ctx, displayName)
}

// Accounts returns the accounts that exist among ids, keyed by ID.
func (s *Service) Accounts(ctx context.Context, ids []string) (map[string]Account, error) {
	list, err := s.store.AccountsByIDs(ctx, ids)
	if err != nil {
		return nil, err
	}
	out := make(map[string]Account, len(list))
	for _, a := range list {
		out[a.ID] = a
	}
	return out, nil
}

// AuthenticateGame resolves a game session token to its account.
func (s *Service) AuthenticateGame(ctx context.Context, token string) (Account, error) {
	sess, err := s.authenticate(ctx, token, SessionGame)
	if err != nil {
		return Account{}, err
	}
	return s.store.AccountByID(ctx, sess.AccountID)
}

func (s *Service) authenticate(ctx context.Context, token string, kind SessionKind) (Session, error) {
	prefix := prefixLauncherSession
	if kind == SessionGame {
		prefix = prefixGameSession
	}
	if !strings.HasPrefix(token, prefix) {
		return Session{}, ErrInvalidCredentials
	}
	sess, err := s.store.ActiveSession(ctx, secret.Hash(token), s.now())
	if errors.Is(err, ErrNotFound) {
		return Session{}, ErrInvalidCredentials
	}
	if err != nil {
		return Session{}, err
	}
	if sess.Kind != kind {
		return Session{}, ErrInvalidCredentials
	}
	return sess, nil
}

func (s *Service) createSession(ctx context.Context, accountID string, kind SessionKind, buildVersion string, lifetime time.Duration, prefix string) (IssuedToken, error) {
	tok, hash, err := secret.New(prefix)
	if err != nil {
		return IssuedToken{}, err
	}
	now := s.now()
	sess := Session{
		TokenHash:    hash,
		AccountID:    accountID,
		Kind:         kind,
		BuildVersion: buildVersion,
		CreatedAt:    now,
		ExpiresAt:    now.Add(lifetime),
	}
	if err := s.store.CreateSession(ctx, sess); err != nil {
		return IssuedToken{}, err
	}
	return IssuedToken{Token: tok, ExpiresAt: sess.ExpiresAt}, nil
}

func validateBuildVersion(v string) error {
	if len(v) == 0 || len(v) > maxBuildVersionLength || !buildVersionPattern.MatchString(v) {
		return ErrInvalidBuildVersion
	}
	return nil
}
