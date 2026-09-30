package postgres

import (
	"context"
	"errors"
	"testing"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/settings"
)

func TestAccountSettingsInPostgres(t *testing.T) {
	store := openTestStore(t)
	ctx := context.Background()
	a, err := store.EnsureDevAccount(ctx, "DevOne")
	if err != nil {
		t.Fatal(err)
	}
	// A test limit, not configuration.
	svc := settings.NewService(store.Settings(), 1024)
	if d, err := svc.Get(ctx, a.ID); err != nil || d.Revision != 0 || len(d.Values) != 0 {
		t.Fatalf("a new account's document: %+v %v", d, err)
	}
	d, err := svc.Put(ctx, a.ID, 0, map[string]string{"camera_move_speed": "70"})
	if err != nil || d.Revision != 1 {
		t.Fatalf("the first write: %+v %v", d, err)
	}
	if _, err := svc.Put(ctx, a.ID, 0, map[string]string{"camera_move_speed": "10"}); !errors.Is(err, settings.ErrConflict) {
		t.Fatalf("a second first write must conflict, got %v", err)
	}
	if _, err := svc.Put(ctx, a.ID, 5, map[string]string{"camera_move_speed": "10"}); !errors.Is(err, settings.ErrConflict) {
		t.Fatalf("a write from a base the store never had must conflict, got %v", err)
	}
	d, err = svc.Put(ctx, a.ID, 1, map[string]string{"interface_hud_scale": "120"})
	if err != nil || d.Revision != 2 {
		t.Fatalf("the second write: %+v %v", d, err)
	}
	got, err := svc.Get(ctx, a.ID)
	if err != nil || got.Revision != 2 || got.Values["interface_hud_scale"] != "120" || len(got.Values) != 1 {
		t.Fatalf("stored document: %+v %v", got, err)
	}
}
