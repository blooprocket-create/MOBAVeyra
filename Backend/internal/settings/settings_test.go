package settings

import (
	"context"
	"errors"
	"strings"
	"testing"
)

// A test limit, not configuration.
const testLimit = 256

const account = "11111111-aaaa-4bbb-8ccc-dddddddddddd"

func TestAFirstWriteIsRevisionOneAndEachWriteNamesItsBase(t *testing.T) {
	svc := NewService(NewMemStore(), testLimit)
	ctx := context.Background()
	if d, err := svc.Get(ctx, account); err != nil || d.Revision != 0 || len(d.Values) != 0 {
		t.Fatalf("a new account's document: %+v %v", d, err)
	}
	d, err := svc.Put(ctx, account, 0, map[string]string{"camera_move_speed": "70"})
	if err != nil || d.Revision != 1 {
		t.Fatalf("the first write: %+v %v", d, err)
	}
	if _, err := svc.Put(ctx, account, 0, map[string]string{"camera_move_speed": "40"}); !errors.Is(err, ErrConflict) {
		t.Fatalf("a write from a stale base must conflict, got %v", err)
	}
	d, err = svc.Put(ctx, account, 1, map[string]string{"interface_hud_scale": "120"})
	if err != nil || d.Revision != 2 {
		t.Fatalf("the second write: %+v %v", d, err)
	}
	if got, _ := svc.Get(ctx, account); got.Revision != 2 || got.Values["interface_hud_scale"] != "120" || len(got.Values) != 1 {
		t.Fatalf("a write replaces the whole document: %+v", got)
	}
}

func TestItRefusesMalformedAndOversizedDocuments(t *testing.T) {
	svc := NewService(NewMemStore(), testLimit)
	ctx := context.Background()
	if _, err := svc.Put(ctx, account, 0, map[string]string{"Camera Speed": "70"}); !errors.Is(err, ErrInvalid) {
		t.Fatalf("a malformed setting ID: %v", err)
	}
	if _, err := svc.Put(ctx, account, -1, nil); !errors.Is(err, ErrInvalid) {
		t.Fatalf("a negative revision: %v", err)
	}
	if _, err := svc.Put(ctx, account, 0, map[string]string{"camera_move_speed": strings.Repeat("9", testLimit)}); !errors.Is(err, ErrTooLarge) {
		t.Fatalf("a document over the limit: %v", err)
	}
	if d, _ := svc.Get(ctx, account); d.Revision != 0 {
		t.Fatalf("a refused write stores nothing: %+v", d)
	}
}
