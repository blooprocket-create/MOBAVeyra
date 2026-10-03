package config

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"runtime"
	"slices"
	"strings"
	"testing"
	"time"
)

const validJSON = `{
  "environment": "local",
  "listenAddress": ":8080",
  "requestBodyLimitBytes": 1024,
  "http": {"readTimeout": "5s", "writeTimeout": "5s", "idleTimeout": "30s", "shutdownTimeout": "5s"},
  "sessions": {"launcherLifetime": "720h", "gameLifetime": "24h"},
  "launchCodes": {"lifetime": "20s"},
  "devLogin": {"enabled": true, "accounts": ["DevOne", "DevTwo"]},
  "playerLogin": {"provider": "firebase", "firebase": {"projectId": "veyra-test1", "keysUrl": "https://keys.example.com/certs", "keysFetchTimeout": "10s", "clockSkew": "30s"}},
  "party": {"maxSize": 5, "inviteLifetime": "2m", "defaultPrivacy": "private"},
  "modes": [
    {"id": "casual_select", "category": "casual", "enabled": true, "humanPlayersPerTeam": 5, "matchmaking": "casualSelect"},
    {"id": "ranked", "category": "ranked", "enabled": false, "humanPlayersPerTeam": 5, "matchmaking": "notImplemented"}
  ],
  "vanguards": {"released": ["cairn", "qazharr", "oriel", "bryn"], "starters": ["cairn", "qazharr", "oriel"], "rotation": {"slots": 12, "epoch": "2026-09-28T00:00:00Z", "weekSeconds": 604800, "seed": "test", "releases": {}}},
  "fluxSpells": {"roster": ["blink", "mend"]},
  "customPractice": {"enabled": true, "mode": "custom_practice", "hostSide": "A", "pickDuration": "30s", "playersPerSide": 5,
    "bots": [{"side": "B", "vanguardId": "cairn", "difficulty": "beginner"}, {"side": "B", "vanguardId": "bryn", "difficulty": "intermediate"}]},
  "settings": {"maxDocumentBytes": 512},
  "customLobby": {"enabled": true, "mode": "custom_game", "playersPerSide": 5, "pickDuration": "60s", "inviteLifetime": "2m", "startingGold": {"min": 0, "max": 20000}},
  "matchmaking": {"interval": "1s", "searchLimit": 10000},
  "matchFound": {"acceptDuration": "15s"},
  "casualSelect": {"pickDuration": "60s", "presenceTimeout": "10s", "finalDuration": "0s"},
  "selection": {"tickInterval": "1s", "startingTimeout": "60s"},
  "matches": {"devCreate": {"enabled": true}, "readyTimeout": "120s", "maxDuration": "4h", "reapInterval": "5s", "removeServerAfter": "2m", "historyPageSize": 20,
    "maps": {"play": "/Game/Maps/L_Play", "development": "/Game/Maps/L_Dev"}},
  "chat": {"maxCharacters": 250, "maxPerWindow": 5, "window": "5s", "historyMessages": 100, "pageSize": 200, "retention": "168h", "postMatchWindow": "10m", "pruneInterval": "10m"},
  "conduct": {"reasons": ["afk", "other"], "detailsMaxCharacters": 500, "reportWindow": "336h", "commendWindow": "10m"},
  "profile": {"icons": ["default", "vanguard_cairn"], "backgrounds": ["default", "vanguard_oriel"], "defaultIcon": "default", "defaultBackground": "default"},
  "names": {"renameCooldown": "24h", "claimAfter": "8760h", "renamePrice": {"flux": 6000, "refinedFlux": 600}},
  "favorites": {"maxPerAccount": 64},
  "dodges": {"restriction": "5m"},
  "presence": {"offlineAfter": "30s", "touchEvery": "5s", "sweepInterval": "5s", "postMatchGrace": "2m"},
  "progression": {
    "accountXp": {"perMinute": 6, "winBonus": 30, "coopBelowLevel": 10},
    "accountLevels": {"firstLevel": 150, "growthPerLevel": 20, "growthUntilLevel": 100},
    "flux": {"perLevelUp": 400},
    "refinedFlux": {"milestones": [{"level": 30, "amount": 250}, {"level": 50, "amount": 400}], "everyLevels": 25, "amountAfterMilestones": 500},
    "mastery": {"perMinute": 10, "winBonus": 100, "performanceCap": 300,
      "weights": {"kills": 15, "assists": 10, "vanguardDamage": 0.005, "damageShielded": 0.005, "teammateHealing": 0.005, "crowdControlSeconds": 2,
        "towerDamage": 0.005, "wellsSecured": 20, "wardsPlaced": 3, "wardsDestroyed": 5},
      "levels": {"firstLevel": 1000, "growthPerLevel": 500, "growthUntilLevel": 5}, "emoteTierLevels": [1, 5, 10]},
    "prices": {"cairn": {"flux": 1500, "refinedFlux": 300}, "qazharr": {"flux": 1500, "refinedFlux": 300}, "oriel": {"flux": 1500, "refinedFlux": 300},
      "bryn": {"flux": 3000, "refinedFlux": 550}},
    "devGrant": {"enabled": true}},
  "allocator": {"kind": "docker", "docker": {
    "endpoint": "unix:///var/run/docker.sock", "apiVersion": "1.44", "requestTimeout": "30s",
    "image": "veyra-match-server:local", "network": "veyra_default", "containerNamePrefix": "veyra-match-",
    "containerPort": 7777, "hostPorts": {"min": 7780, "max": 7789}, "hostIp": "127.0.0.1",
    "publicHost": "127.0.0.1", "backendUrl": "http://backend:8080",
    "serverArgs": ["-port=7777", "-log"], "stopTimeout": "10s"}}
}`

func TestParseValid(t *testing.T) {
	c, err := Parse([]byte(validJSON))
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}
	if c.LaunchCodeLifetime != 20*time.Second || c.Sessions.Game != 24*time.Hour {
		t.Fatalf("durations not parsed: %+v", c)
	}
	if !c.DevLogin.Enabled || len(c.DevLogin.Accounts) != 2 {
		t.Fatalf("dev login not parsed: %+v", c.DevLogin)
	}
	if !c.Matches.DevCreate || c.Matches.ReadyTimeout != 120*time.Second || c.Matches.RemoveServerAfter != 2*time.Minute {
		t.Fatalf("matches not parsed: %+v", c.Matches)
	}
	if c.Matches.Maps != (Maps{Play: "/Game/Maps/L_Play", Development: "/Game/Maps/L_Dev"}) {
		t.Fatalf("maps not parsed: %+v", c.Matches.Maps)
	}
	if p := c.CustomPractice; p.PlayersPerSide != 5 || len(p.Bots) != 2 || p.Bots[1] != (PracticeBot{Side: "B", VanguardID: "bryn", Difficulty: "intermediate"}) {
		t.Fatalf("practice bots not parsed: %+v", p)
	}
	if l := c.CustomLobby; !l.Enabled || l.Mode != "custom_game" || l.PlayersPerSide != 5 || l.PickDuration != time.Minute || l.InviteLifetime != 2*time.Minute ||
		l.StartingGold != (GoldRange{Min: 0, Max: 20000}) {
		t.Fatalf("custom lobby not parsed: %+v", l)
	}
	if c.Matchmaking.Interval != time.Second || c.Matchmaking.SearchLimit != 10000 {
		t.Fatalf("matchmaking not parsed: %+v", c.Matchmaking)
	}
	d := c.Allocator.Docker
	if c.Allocator.Kind != AllocatorDocker || d == nil || d.HostPortMin != 7780 || d.HostPortMax != 7789 || len(d.ServerArgs) != 2 || d.BackendURL != "http://backend:8080" {
		t.Fatalf("allocator not parsed: %+v %+v", c.Allocator, d)
	}
}

func TestParseAllocatorNone(t *testing.T) {
	raw := strings.Replace(validJSON, `"devCreate": {"enabled": true}`, `"devCreate": {"enabled": false}`, 1)
	start := strings.Index(raw, `"allocator"`)
	raw = raw[:start] + `"allocator": {"kind": "none"}` + "\n}"
	c, err := Parse([]byte(raw))
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}
	if c.Allocator.Kind != AllocatorNone || c.Allocator.Docker != nil {
		t.Fatalf("allocator not parsed: %+v", c.Allocator)
	}
}

// A draft mode's turns must give each side picks for its whole team, or a
// seat would wait for a turn that never comes (ADR-042 §1).
func TestDraftPickTurnsCoverTheTeams(t *testing.T) {
	draft := strings.Replace(validJSON, `"category": "ranked", "enabled": false, "humanPlayersPerTeam": 5, "matchmaking": "notImplemented"`,
		`"category": "ranked", "enabled": false, "humanPlayersPerTeam": 5, "matchmaking": "draftPick"`, 1)
	block := func(picksB int) string {
		return fmt.Sprintf(`"draftPick": {"turns": [{"phase": "ban", "side": "A", "count": 1}, {"phase": "pick", "side": "A", "count": 5},
			{"phase": "pick", "side": "B", "count": %d}], "banDuration": "30s", "pickDuration": "30s", "finalDuration": "10s", "presenceTimeout": "10s"},
  "casualSelect": {`, picksB)
	}
	c, err := Parse([]byte(strings.Replace(draft, `"casualSelect": {`, block(5), 1)))
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}
	if len(c.DraftPick.Turns) != 3 || !c.DraftPick.Turns[0].Ban || c.DraftPick.Turns[2].Side != "B" || c.DraftPick.FinalDuration != 10*time.Second {
		t.Fatalf("draft pick: %+v", c.DraftPick)
	}
	_, err = Parse([]byte(strings.Replace(draft, `"casualSelect": {`, block(4), 1)))
	if err == nil || !strings.Contains(err.Error(), "side B 4 pick(s), fewer than its team of 5") {
		t.Fatalf("a side short of picks: %v", err)
	}
}

func TestParsePlayerLogin(t *testing.T) {
	c, err := Parse([]byte(validJSON))
	if err != nil {
		t.Fatal(err)
	}
	want := FirebaseLogin{ProjectID: "veyra-test1", KeysURL: "https://keys.example.com/certs", KeysFetchTimeout: 10 * time.Second, ClockSkew: 30 * time.Second}
	if c.PlayerLogin.Provider != PlayerLoginFirebase || c.PlayerLogin.Firebase == nil || *c.PlayerLogin.Firebase != want {
		t.Fatalf("player login not parsed: %+v %+v", c.PlayerLogin, c.PlayerLogin.Firebase)
	}
	start := strings.Index(validJSON, `"playerLogin"`)
	end := strings.Index(validJSON, `"party"`)
	c, err = Parse([]byte(validJSON[:start] + `"playerLogin": {"provider": "none"},
  ` + validJSON[end:]))
	if err != nil {
		t.Fatal(err)
	}
	if c.PlayerLogin.Provider != PlayerLoginNone || c.PlayerLogin.Firebase != nil {
		t.Fatalf("provider none not parsed: %+v", c.PlayerLogin)
	}
}

func TestParseRejects(t *testing.T) {
	cases := map[string]struct {
		from, to, want string
	}{
		"unknown field":            {`"listenAddress"`, `"listenAddres"`, "unknown field"},
		"missing lifetime":         {`"launchCodes": {"lifetime": "20s"}`, `"launchCodes": {}`, "launchCodes.lifetime is required"},
		"launch code too long":     {`"lifetime": "20s"`, `"lifetime": "5m"`, "must not exceed"},
		"negative session":         {`"gameLifetime": "24h"`, `"gameLifetime": "-1h"`, "must be positive"},
		"dev login outside local":  {`"environment": "local"`, `"environment": "staging"`, "only allowed when environment"},
		"duplicate dev account":    {`["DevOne", "DevTwo"]`, `["DevOne", "DevOne"]`, "duplicate"},
		"no dev accounts":          {`["DevOne", "DevTwo"]`, `[]`, "at least one account"},
		"party size zero":          {`"maxSize": 5`, `"maxSize": 0`, "party.maxSize must be between 1 and 5"},
		"party size six":           {`"maxSize": 5`, `"maxSize": 6`, "party.maxSize must be between 1 and 5"},
		"bad privacy":              {`"defaultPrivacy": "private"`, `"defaultPrivacy": "open"`, "party.defaultPrivacy must be"},
		"duplicate mode":           {`"id": "ranked"`, `"id": "casual_select"`, "duplicate id casual_select"},
		"mode missing team size":   {`"enabled": false, "humanPlayersPerTeam": 5`, `"enabled": false`, "humanPlayersPerTeam is required"},
		"mode missing matchmaking": {`, "matchmaking": "notImplemented"}`, `}`, "modes[1].matchmaking is required"},
		"mode bad matchmaking":     {`"matchmaking": "casualSelect"`, `"matchmaking": "draft"`, "modes[0].matchmaking must be"},
		"mode missing category":    {`"id": "ranked", "category": "ranked"`, `"id": "ranked"`, "modes[1].category is required"},
		"mode bad category":        {`"category": "casual"`, `"category": "arcade"`, "modes[0].category must be"},
		"coop outside the AI category": {`"matchmaking": "casualSelect"}`, `"matchmaking": "coop", "aiPerTeam": 5, "aiDifficulty": "beginner"}`,
			"modes[0].category must be \"ai\" exactly when"},
		"PvP in the AI category":   {`"category": "casual"`, `"category": "ai"`, "modes[0].category must be \"ai\" exactly when"},
		"coop without its AI team": {`"matchmaking": "casualSelect"`, `"matchmaking": "coop"`, "modes[0].aiPerTeam is required"},
		"coop bad difficulty": {`"matchmaking": "casualSelect"}`, `"matchmaking": "coop", "aiPerTeam": 5, "aiDifficulty": "expert"}`,
			"modes[0].aiDifficulty must be"},
		"AI team on a PvP mode":     {`"matchmaking": "casualSelect"}`, `"matchmaking": "casualSelect", "aiPerTeam": 5}`, "modes[0] has an AI team"},
		"no matchmaking":            {`"matchmaking": {"interval": "1s", "searchLimit": 10000},`, ``, "matchmaking is required"},
		"zero matchmaking interval": {`"interval": "1s"`, `"interval": "0s"`, "matchmaking.interval must be positive"},
		"no search limit":           {`, "searchLimit": 10000`, ``, "matchmaking.searchLimit is required"},
		"zero search limit":         {`"searchLimit": 10000`, `"searchLimit": 0`, "matchmaking.searchLimit must be at least 1"},
		"no match found":            {`"matchFound": {"acceptDuration": "15s"},`, ``, "matchFound is required"},
		"no accept duration":        {`{"acceptDuration": "15s"}`, `{}`, "matchFound.acceptDuration is required"},
		"no casual select":          {`"casualSelect": {"pickDuration": "60s", "presenceTimeout": "10s", "finalDuration": "0s"},`, ``, "casualSelect is required"},
		"no final window":           {`, "finalDuration": "0s"`, ``, "casualSelect.finalDuration is required"},
		"negative final window":     {`"finalDuration": "0s"`, `"finalDuration": "-1s"`, "casualSelect.finalDuration must not be negative"},
		"draft without its turns": {`"category": "ranked", "enabled": false, "humanPlayersPerTeam": 5, "matchmaking": "notImplemented"`,
			`"category": "ranked", "enabled": false, "humanPlayersPerTeam": 5, "matchmaking": "draftPick"`, "draftPick is required"},
		"no presence timeout":       {`, "presenceTimeout": "10s"`, ``, "casualSelect.presenceTimeout is required"},
		"no vanguards":              {`"vanguards": {"released": ["cairn", "qazharr", "oriel", "bryn"], "starters": ["cairn", "qazharr", "oriel"], "rotation": {"slots": 12, "epoch": "2026-09-28T00:00:00Z", "weekSeconds": 604800, "seed": "test", "releases": {}}},`, ``, "vanguards is required"},
		"no flux spells":            {`"fluxSpells": {"roster": ["blink", "mend"]},`, ``, "fluxSpells is required"},
		"empty spell roster":        {`"roster": ["blink", "mend"]`, `"roster": []`, "fluxSpells.roster is required"},
		"duplicate spell":           {`"roster": ["blink", "mend"]`, `"roster": ["blink", "blink"]`, "fluxSpells.roster contains duplicate blink"},
		"nothing released":          {`"released": ["cairn", "qazharr", "oriel", "bryn"]`, `"released": []`, "vanguards.released is required"},
		"bad released id":           {`"released": ["cairn",`, `"released": ["Cairn",`, "must hold content IDs"},
		"duplicate released":        {`"released": ["cairn", "qazharr"`, `"released": ["cairn", "cairn"`, "vanguards.released contains duplicate cairn"},
		"unreleased starter":        {`"starters": ["cairn", "qazharr", "oriel"]`, `"starters": ["cairn", "qazharr", "raska"]`, "raska is not in vanguards.released"},
		"too few starters":          {`"starters": ["cairn", "qazharr", "oriel"]`, `"starters": ["cairn", "qazharr"]`, "must list 3 to 5"},
		"no rotation slots":         {`"slots": 12`, `"slots": 0`, "vanguards.rotation.slots must be at least 1"},
		"bad rotation epoch":        {`"epoch": "2026-09-28T00:00:00Z"`, `"epoch": "Monday"`, "vanguards.rotation.epoch must be an RFC 3339 time"},
		"no rotation week":          {`"weekSeconds": 604800`, `"weekSeconds": 0`, "vanguards.rotation.weekSeconds must be at least 1"},
		"no rotation seed":          {`"seed": "test"`, `"seed": ""`, "vanguards.rotation.seed is required"},
		"unreleased rotation entry": {`"releases": {}`, `"releases": {"test_vanguard": "2026-10-01T00:00:00Z"}`, "vanguards.rotation.releases names test_vanguard"},
		"no custom practice": {`"customPractice": {"enabled": true, "mode": "custom_practice", "hostSide": "A", "pickDuration": "30s", "playersPerSide": 5,
    "bots": [{"side": "B", "vanguardId": "cairn", "difficulty": "beginner"}, {"side": "B", "vanguardId": "bryn", "difficulty": "intermediate"}]},`, ``, "customPractice is required"},
		"practice missing side":   {`"hostSide": "A", `, ``, "customPractice.hostSide is required"},
		"practice no pick time":   {`, "pickDuration": "30s"`, ``, "customPractice.pickDuration is required"},
		"practice no side size":   {`, "playersPerSide": 5`, ``, "customPractice.playersPerSide is required"},
		"practice zero side size": {`"playersPerSide": 5`, `"playersPerSide": 0`, "customPractice.playersPerSide must be at least 1"},
		"practice no bot list": {`,
    "bots": [{"side": "B", "vanguardId": "cairn", "difficulty": "beginner"}, {"side": "B", "vanguardId": "bryn", "difficulty": "intermediate"}]`, ``, "customPractice.bots is required"},
		"no custom lobby":           {`"customLobby": {"enabled": true, "mode": "custom_game", "playersPerSide": 5, "pickDuration": "60s", "inviteLifetime": "2m", "startingGold": {"min": 0, "max": 20000}},`, ``, "customLobby is required"},
		"no settings":               {`"settings": {"maxDocumentBytes": 512},`, ``, "settings.maxDocumentBytes is required"},
		"settings no bytes":         {`"maxDocumentBytes": 512`, `"maxDocumentBytes": 0`, "settings.maxDocumentBytes must be above 0"},
		"settings over the body":    {`"maxDocumentBytes": 512`, `"maxDocumentBytes": 1024`, "settings.maxDocumentBytes must stay under requestBodyLimitBytes"},
		"lobby mode is matchmade":   {`"mode": "custom_game"`, `"mode": "casual_select"`, "customLobby.mode must not be a matchmade mode's id"},
		"lobby mode is practice's":  {`"mode": "custom_game"`, `"mode": "custom_practice"`, "customLobby.mode must differ from customPractice.mode"},
		"lobby zero side size":      {`"playersPerSide": 5, "pickDuration": "60s"`, `"playersPerSide": 0, "pickDuration": "60s"`, "customLobby.playersPerSide must be at least 1"},
		"lobby no invite lifetime":  {`, "inviteLifetime": "2m", "startingGold"`, `, "startingGold"`, "customLobby.inviteLifetime is required"},
		"lobby gold range reversed": {`{"min": 0, "max": 20000}`, `{"min": 500, "max": 100}`, "customLobby.startingGold must have 0 <= min <= max"},
		"lobby negative gold":       {`{"min": 0, "max": 20000}`, `{"min": -1, "max": 100}`, "customLobby.startingGold must have 0 <= min <= max"},
		"bot on no side":            {`{"side": "B", "vanguardId": "cairn"`, `{"side": "C", "vanguardId": "cairn"`, "customPractice.bots[0].side must be"},
		"bot without a Vanguard":    {`{"side": "B", "vanguardId": "cairn", `, `{"side": "B", `, "customPractice.bots[0].vanguardId is required"},
		"bot without a difficulty":  {`, "difficulty": "beginner"`, ``, "customPractice.bots[0].difficulty is required"},
		"bot unknown difficulty":    {`"difficulty": "intermediate"`, `"difficulty": "expert"`, "customPractice.bots[1].difficulty must be"},
		"bot unreleased Vanguard":   {`"vanguardId": "bryn"`, `"vanguardId": "raska"`, "customPractice.bots[1].vanguardId must be in vanguards.released"},
		"bots overfill a side":      {`"playersPerSide": 5`, `"playersPerSide": 1`, "put 2 Vanguards on side B, the host included, more than customPractice.playersPerSide (1)"},
		"bot with extra field":      {`"difficulty": "beginner"}`, `"difficulty": "beginner", "level": 3}`, "unknown field"},
		"no selection":              {`"selection": {"tickInterval": "1s", "startingTimeout": "60s"},`, ``, "selection is required"},
		"zero tick":                 {`"tickInterval": "1s"`, `"tickInterval": "0s"`, "selection.tickInterval must be positive"},
		"starting before allocator": {`"startingTimeout": "60s"`, `"startingTimeout": "30s"`, "must exceed allocator.docker.requestTimeout"},
		"practice bad side":         {`"hostSide": "A"`, `"hostSide": "C"`, "customPractice.hostSide must be"},
		"practice bad mode":         {`"mode": "custom_practice"`, `"mode": "Custom Practice"`, "customPractice.mode must be a content ID"},
		"practice queueable mode":   {`"mode": "custom_practice"`, `"mode": "casual_select"`, "must not be a matchmade mode"},
		"dev matches outside local": {`"environment": "local"`, `"environment": "staging"`, "matches.devCreate.enabled is only allowed"},
		"missing ready timeout":     {`"readyTimeout": "120s", `, ``, "matches.readyTimeout is required"},
		"zero reap interval":        {`"reapInterval": "5s"`, `"reapInterval": "0s"`, "matches.reapInterval must be positive"},
		"unknown allocator":         {`"kind": "docker"`, `"kind": "fleet"`, "allocator.kind must be"},
		"misspelt docker section":   {`"kind": "docker", "docker": {`, `"kind": "docker", "dockerx": {`, "unknown field"},
		"bad endpoint":              {`unix:///var/run/docker.sock`, `http://localhost:2375`, "endpoint must be unix"},
		"old api version":           {`"apiVersion": "1.44"`, `"apiVersion": "1.41"`, "apiVersion must be at least 1.44"},
		"bad api version":           {`"apiVersion": "1.44"`, `"apiVersion": "v1"`, "apiVersion must look like"},
		"bad name prefix":           {`"veyra-match-"`, `"-veyra"`, "containerNamePrefix must start"},
		"port out of range":         {`"containerPort": 7777`, `"containerPort": 70000`, "containerPort must be a port"},
		"inverted ports":            {`{"min": 7780, "max": 7789}`, `{"min": 7789, "max": 7780}`, "hostPorts.min must not exceed"},
		"bad host ip":               {`"hostIp": "127.0.0.1"`, `"hostIp": "localhost"`, "hostIp must be an IP"},
		"backend url with path":     {`"http://backend:8080"`, `"http://backend:8080/v1"`, "backendUrl must be"},
		"backend url scheme":        {`"http://backend:8080"`, `"ftp://backend:8080"`, "backendUrl must be"},
		"backend url with slash":    {`"http://backend:8080"`, `"http://backend:8080/"`, "backendUrl must be"},
		"backend url with query":    {`"http://backend:8080"`, `"http://backend:8080?x=1"`, "backendUrl must be"},
		"backend url with fragment": {`"http://backend:8080"`, `"http://backend:8080#x"`, "backendUrl must be"},
		"backend url with user":     {`"http://backend:8080"`, `"http://user@backend:8080"`, "backendUrl must be"},
		"public host with port":     {`"publicHost": "127.0.0.1"`, `"publicHost": "127.0.0.1:7780"`, "publicHost must be"},
		"public host with scheme":   {`"publicHost": "127.0.0.1"`, `"publicHost": "http://127.0.0.1"`, "publicHost must be"},
		"no server args":            {`["-port=7777", "-log"]`, `[]`, "serverArgs is required"},
		"blank server arg":          {`["-port=7777", "-log"]`, `["-port=7777", " "]`, "must not contain blank"},
		"map in server args":        {`["-port=7777", "-log"]`, `["/Game/Maps/L_Play", "-log"]`, "serverArgs must not name a map"},
		"no maps": {`,
    "maps": {"play": "/Game/Maps/L_Play", "development": "/Game/Maps/L_Dev"}`, ``, "matches.maps is required"},
		"no development map":   {`, "development": "/Game/Maps/L_Dev"`, ``, "matches.maps.development is required"},
		"map not a path":       {`"play": "/Game/Maps/L_Play"`, `"play": "L_Play"`, "matches.maps.play must be a map path"},
		"no provider":          {`"playerLogin": {"provider": "firebase", `, `"playerLogin": {`, "playerLogin.provider is required"},
		"unknown provider":     {`"provider": "firebase"`, `"provider": "auth0"`, "playerLogin.provider must be"},
		"none with firebase":   {`"provider": "firebase"`, `"provider": "none"`, "playerLogin.firebase must be absent"},
		"bad project id":       {`"projectId": "veyra-test1"`, `"projectId": "Veyra Test"`, "projectId must be a Firebase project ID"},
		"plain http keys":      {`"keysUrl": "https://keys.example.com/certs"`, `"keysUrl": "http://keys.example.com/certs"`, "keysUrl must be an https URL"},
		"no keys timeout":      {`"keysFetchTimeout": "10s", `, ``, "keysFetchTimeout is required"},
		"clock skew too large": {`"clockSkew": "30s"`, `"clockSkew": "1h"`, "clockSkew must be from"},
		"negative clock skew":  {`"clockSkew": "30s"`, `"clockSkew": "-1s"`, "clockSkew must be from"},
	}
	for name, tc := range cases {
		t.Run(name, func(t *testing.T) {
			raw := strings.Replace(validJSON, tc.from, tc.to, 1)
			if raw == validJSON {
				t.Fatalf("test fixture did not change")
			}
			_, err := Parse([]byte(raw))
			if err == nil || !strings.Contains(err.Error(), tc.want) {
				t.Fatalf("want error containing %q, got %v", tc.want, err)
			}
		})
	}
}

// The committed local config must always load.
func TestCommittedLocalConfigLoads(t *testing.T) {
	_, file, _, _ := runtime.Caller(0)
	path := filepath.Join(filepath.Dir(file), "..", "..", "config", "local.json")
	if _, err := os.Stat(path); err != nil {
		t.Fatalf("local config missing: %v", err)
	}
	if _, err := Load(path); err != nil {
		t.Fatalf("Load(%s): %v", path, err)
	}
}

// No side the committed config lets the backend fill may be larger than the
// match server's own cap, Game/Tuning/Match.json teams.maxTeamSize: the server
// refuses such an assignment (UVeyraMatchHostSubsystem::SetAssignment), and
// the match would fail before it is ready. Both files are data, so they are
// kept in step here.
func TestTeamSizesFitTheGamesMatchJSON(t *testing.T) {
	_, file, _, _ := runtime.Caller(0)
	root := filepath.Join(filepath.Dir(file), "..", "..", "..")
	cfg, err := Load(filepath.Join(root, "Backend", "config", "local.json"))
	if err != nil {
		t.Fatalf("Load: %v", err)
	}
	raw, err := os.ReadFile(filepath.Join(root, "Game", "Tuning", "Match.json"))
	if err != nil {
		t.Fatalf("read the game's Match.json: %v", err)
	}
	var tuning struct {
		Teams struct {
			MaxTeamSize int `json:"maxTeamSize"`
		} `json:"teams"`
	}
	if err := json.Unmarshal(raw, &tuning); err != nil || tuning.Teams.MaxTeamSize < 1 {
		t.Fatalf("Match.json teams.maxTeamSize: %d %v", tuning.Teams.MaxTeamSize, err)
	}
	limit := tuning.Teams.MaxTeamSize
	if cfg.CustomPractice.PlayersPerSide > limit {
		t.Fatalf("customPractice.playersPerSide is %d, but the match server holds at most %d a side (Match.json teams.maxTeamSize)",
			cfg.CustomPractice.PlayersPerSide, limit)
	}
	if cfg.CustomLobby.PlayersPerSide > limit {
		t.Fatalf("customLobby.playersPerSide is %d, but the match server holds at most %d a side (Match.json teams.maxTeamSize)",
			cfg.CustomLobby.PlayersPerSide, limit)
	}
	for _, m := range cfg.Modes {
		if m.HumanPlayersPerTeam > limit {
			t.Fatalf("mode %s has %d players a side, but the match server holds at most %d (Match.json teams.maxTeamSize)", m.ID, m.HumanPlayersPerTeam, limit)
		}
	}
}

// A matchmade mode fills the game's whole team: five a side, and a co-op
// mode's enemy AI team as many (Modes Bible §1, §4). Scripts shrink a queue
// for their clients in a config of their own, never in the committed one.
func TestMatchmadeModesFillTheGamesTeams(t *testing.T) {
	_, file, _, _ := runtime.Caller(0)
	root := filepath.Join(filepath.Dir(file), "..", "..", "..")
	cfg, err := Load(filepath.Join(root, "Backend", "config", "local.json"))
	if err != nil {
		t.Fatalf("Load: %v", err)
	}
	raw, err := os.ReadFile(filepath.Join(root, "Game", "Tuning", "Match.json"))
	if err != nil {
		t.Fatalf("read the game's Match.json: %v", err)
	}
	var tuning struct {
		Teams struct {
			MaxTeamSize int `json:"maxTeamSize"`
		} `json:"teams"`
	}
	if err := json.Unmarshal(raw, &tuning); err != nil || tuning.Teams.MaxTeamSize < 1 {
		t.Fatalf("Match.json teams.maxTeamSize: %d %v", tuning.Teams.MaxTeamSize, err)
	}
	team := tuning.Teams.MaxTeamSize
	for _, m := range cfg.Modes {
		if m.Matchmaking == MatchmakingNotImplemented {
			continue
		}
		if m.HumanPlayersPerTeam != team {
			t.Errorf("mode %s queues %d humans a side, not the game's team of %d", m.ID, m.HumanPlayersPerTeam, team)
		}
		if m.Matchmaking == MatchmakingCoop && m.AIPerTeam != team {
			t.Errorf("co-op mode %s fields %d enemy AI, not the game's team of %d", m.ID, m.AIPerTeam, team)
		}
	}
}

// The Flux Spells champion select offers are exactly the game's roster,
// Game/Tuning/Abilities.json fluxSpells.roster, in the same order: the match
// server refuses a spell off it (ADR-015 §5). Both files are data, so they are
// kept in step here.
func TestFluxSpellRosterIsTheGamesAbilitiesJSON(t *testing.T) {
	_, file, _, _ := runtime.Caller(0)
	root := filepath.Join(filepath.Dir(file), "..", "..", "..")
	cfg, err := Load(filepath.Join(root, "Backend", "config", "local.json"))
	if err != nil {
		t.Fatalf("Load: %v", err)
	}
	raw, err := os.ReadFile(filepath.Join(root, "Game", "Tuning", "Abilities.json"))
	if err != nil {
		t.Fatalf("read the game's Abilities.json: %v", err)
	}
	var tuning struct {
		FluxSpells struct {
			Roster []string `json:"roster"`
		} `json:"fluxSpells"`
	}
	if err := json.Unmarshal(raw, &tuning); err != nil {
		t.Fatalf("parse Abilities.json: %v", err)
	}
	if !slices.Equal(cfg.FluxSpells.Roster, tuning.FluxSpells.Roster) {
		t.Fatalf("config fluxSpells.roster %v, but Abilities.json's roster is %v", cfg.FluxSpells.Roster, tuning.FluxSpells.Roster)
	}
}
