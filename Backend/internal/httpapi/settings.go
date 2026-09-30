package httpapi

import (
	"errors"
	"net/http"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/settings"
)

// settingsDocumentVersion is the settings document's format, which the client
// writes too (ADR-024 §1; VeyraSettingsDocument::SchemaVersion).
const settingsDocumentVersion = 1

// routeSettings registers the player's account settings routes (ADR-024 §1).
func (s *Server) routeSettings(mux *http.ServeMux) {
	if s.Settings == nil {
		return
	}
	mux.HandleFunc("GET /v1/account/settings", s.authed(s.getSettings))
	mux.HandleFunc("PUT /v1/account/settings", s.authed(s.putSettings))
}

type settingsJSON struct {
	SchemaVersion int               `json:"schemaVersion"`
	Revision      int64             `json:"revision"`
	Values        map[string]string `json:"values"`
}

func toSettingsJSON(d settings.Document) settingsJSON {
	values := d.Values
	if values == nil {
		values = map[string]string{}
	}
	return settingsJSON{SchemaVersion: settingsDocumentVersion, Revision: d.Revision, Values: values}
}

// getSettings answers the player's account settings: revision 0 and no values
// before the first write.
func (s *Server) getSettings(w http.ResponseWriter, r *http.Request, actor string) {
	d, err := s.Settings.Get(r.Context(), actor)
	if err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, toSettingsJSON(d))
}

// putSettings replaces the player's account settings with a document based on
// the revision it names. A stale base is refused with the current document, so
// the client can ask the player which to keep (Settings & Accessibility §7).
func (s *Server) putSettings(w http.ResponseWriter, r *http.Request, actor string) {
	var req settingsJSON
	if !s.decode(w, r, &req) {
		return
	}
	if req.SchemaVersion != settingsDocumentVersion {
		writeError(w, http.StatusBadRequest, "bad_settings")
		return
	}
	d, err := s.Settings.Put(r.Context(), actor, req.Revision, req.Values)
	if errors.Is(err, settings.ErrConflict) {
		current, getErr := s.Settings.Get(r.Context(), actor)
		if getErr != nil {
			s.fail(w, getErr)
			return
		}
		writeJSON(w, http.StatusConflict, map[string]any{"error": "settings_conflict", "current": toSettingsJSON(current)})
		return
	}
	if err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, toSettingsJSON(d))
}
