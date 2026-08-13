# GBA Reader v0.4.2 — Reader control update

- `Down` opens Reader settings; `L` no longer opens settings.
- `L` and `R` are inactive whenever the app launches.
- `Up` silently toggles a session-only shoulder-button mode. When enabled, either `L` or `R` advances to the next page; pressing `Up` again disables both buttons.
- The shoulder-button mode is not persisted in TXT save footers and always resets to disabled on the next app launch.
- Existing page controls remain: `Right`/`A` advance, `Left`/`B` go back, and `Start` manually saves a TXT book.
