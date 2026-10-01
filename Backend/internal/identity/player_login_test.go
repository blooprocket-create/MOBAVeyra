package identity

import (
	"context"
	"errors"
	"strings"
	"testing"
)

// fakeVerifier accepts "valid:<subject>" and nothing else.
type fakeVerifier struct{}

func (fakeVerifier) Verify(_ context.Context, credential string) (ProviderIdentity, error) {
	subject, ok := strings.CutPrefix(credential, "valid:")
	if !ok || subject == "" {
		return ProviderIdentity{}, ErrInvalidCredentials
	}
	return ProviderIdentity{Provider: "test", Subject: subject}, nil
}

func playerSettings() Settings {
	s := testSettings
	s.PlayerLogin = fakeVerifier{}
	return s
}

func TestRegisterThenSignIn(t *testing.T) {
	svc, _ := newTestService(t, playerSettings())
	ctx := context.Background()

	if _, _, err := svc.PlayerLogin(ctx, "valid:uid-1"); !errors.Is(err, ErrNotRegistered) {
		t.Fatalf("before registering: got %v, want ErrNotRegistered", err)
	}
	reg, acct, err := svc.Register(ctx, "valid:uid-1", "Ember_Wing")
	if err != nil {
		t.Fatalf("Register: %v", err)
	}
	if acct.DisplayName != "Ember_Wing" || !strings.HasPrefix(reg.Token, prefixLauncherSession) {
		t.Fatalf("got %+v %q", acct, reg.Token)
	}
	login, again, err := svc.PlayerLogin(ctx, "valid:uid-1")
	if err != nil {
		t.Fatalf("PlayerLogin: %v", err)
	}
	if again.ID != acct.ID || login.Token == reg.Token {
		t.Fatalf("sign-in gave account %s token reused=%v", again.ID, login.Token == reg.Token)
	}
	// The session works for the existing handoff.
	if _, err := svc.IssueLaunchCode(ctx, login.Token, "dev-1"); err != nil {
		t.Fatalf("IssueLaunchCode: %v", err)
	}
}

func TestRegisterRules(t *testing.T) {
	svc, _ := newTestService(t, playerSettings())
	ctx := context.Background()
	if _, _, err := svc.Register(ctx, "valid:uid-1", "Ember"); err != nil {
		t.Fatal(err)
	}
	cases := []struct {
		name, credential, displayName string
		want                          error
	}{
		{"same identity twice", "valid:uid-1", "Other", ErrAlreadyRegistered},
		{"name taken", "valid:uid-2", "Ember", ErrDisplayNameTaken},
		{"name taken in another case", "valid:uid-2", "eMBER", ErrDisplayNameTaken},
		{"dev account's name", "valid:uid-2", "devone", ErrDisplayNameTaken},
		{"bad credential", "forged", "Fresh", ErrInvalidCredentials},
		{"empty credential", "", "Fresh", ErrInvalidCredentials},
		{"too short", "valid:uid-2", "ab", ErrInvalidDisplayName},
		{"too long", "valid:uid-2", strings.Repeat("a", MaxDisplayNameLength+1), ErrInvalidDisplayName},
		{"space", "valid:uid-2", "Ember Wing", ErrInvalidDisplayName},
		{"padded", "valid:uid-2", " Ember2", ErrInvalidDisplayName},
		{"non-ascii", "valid:uid-2", "Émber", ErrInvalidDisplayName},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			if _, _, err := svc.Register(ctx, c.credential, c.displayName); !errors.Is(err, c.want) {
				t.Fatalf("got %v, want %v", err, c.want)
			}
		})
	}
	// A refused registration created nothing: uid-2 can still register.
	if _, _, err := svc.Register(ctx, "valid:uid-2", "Fresh"); err != nil {
		t.Fatalf("after refusals: %v", err)
	}
}

func TestPlayerLoginDisabledWithoutAProvider(t *testing.T) {
	svc, _ := newTestService(t, testSettings)
	ctx := context.Background()
	if _, _, err := svc.PlayerLogin(ctx, "valid:uid-1"); !errors.Is(err, ErrPlayerLoginDisabled) {
		t.Fatalf("PlayerLogin: got %v", err)
	}
	if _, _, err := svc.Register(ctx, "valid:uid-1", "Ember"); !errors.Is(err, ErrPlayerLoginDisabled) {
		t.Fatalf("Register: got %v", err)
	}
}

func TestPlayerLoginRejectsBadCredentials(t *testing.T) {
	svc, _ := newTestService(t, playerSettings())
	if _, _, err := svc.PlayerLogin(context.Background(), "forged"); !errors.Is(err, ErrInvalidCredentials) {
		t.Fatalf("got %v", err)
	}
}
