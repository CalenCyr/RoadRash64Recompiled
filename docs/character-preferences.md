# Remembered local characters

The native game resets its character selector to the current bike's default
unless the player has explicitly selected a rider. The new preference bridge
restores each local slot's last confirmed character before that default is
applied, and sets the same explicit-choice flag used by manual navigation.
The original code still loads the previews and commits the race choices.

`rr64_character_preferences` holds four atomic preferences and writes a small
versioned `character-preferences.cfg` beside the user's launcher settings.
Menu hooks never read or write files. The UI update flushes changed preferences;
failed writes remain pending. Invalid or truncated files are rejected as a unit.
The preferences belong to local player slots, not controller device identifiers.

`rr64_character_menu.cpp` is the guest-memory bridge. Normal selectors expose
riders 0–39; the native cop-unlock flag extends this to 44. Custom Cop's selected
cop bike permits only cop riders. An unavailable saved choice falls back to the
original selector; it does not alter unlocks. Once a new choice is confirmed it
becomes the preference. Bike selection itself is unchanged.

Online menus have one private local selection row. Only that row is remembered
during CharacterSelect, before the online commit copies the shared roster.
Remote participants never overwrite this computer's preferences.

Campaign New Game seeds only the menu character and explicit-choice flag after
the two stock record-reset calls. Native purchase confirmation still controls
the saved character, bike and money. Loading an existing campaign always keeps
that campaign's character. Confirmed campaign choices also update local slot 1's
preference for future selections.

The hooks live in `config/roadrash64.us.toml`; generated source is rebuilt from
that file. Offline storage tests cover validation, concurrency and failed writes;
the menu tests check full-memory write scope, all local slots, unlocks, mode
restrictions and online ownership. These checks do not establish visual acceptance.
