<!--
SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Screenshots and the page tree

`docs/screenshots/` holds one screenshot of each page, and `docs/pages-*.png`
(source: `pages.puml`) shows how the pages are reached. Both are kept by hand,
by the rules below, in the same commit as the change that needs them.

## Screenshots

Made on the emulator by `tools/screenshot-pages.sh` (see its header for the options),
logged out and with the logged-in interface mocked. A file is `PAGE_STATE.png`; a new
shot replaces the old one, and git keeps the history.

1. **Change that alters how a page looks** (a page, a component it uses, a theme
   or a string it shows): build for `-i486`, run `tools/screenshot-pages.sh --changed`,
   look at every new image, and commit the images with the change. Pages are
   found from the QML; after a C++ change that shows on a page, name the pages
   instead (`tools/screenshot-pages.sh PuzzlesPage`).
2. **New page:** add a `page_<Label>` function to the script and run it with the page
   name. Declare `states` truthfully: `logged-in` or `logged-out` when the page cannot
   be reached in the other state, `any-login` when it looks the same in both, else
   both. A page that needs a live game or a controller handed in cannot be opened
   alone; leave it out and say so in the script's header.
3. **Removed or renamed page:** delete its function and its images.
4. **Check logged-in and logged-out** for any change to the interface, whatever the
   page; the logged-in state is a mock (no account data), so data-driven views
   cannot be judged from it.
5. Use the emulator's current ambience for a whole run: it changes colour over the
   day, so shoot a set in one go. Never commit shots from an older build than the code.
6. Pages that load from lichess.org use public accounts, so a shot can differ by live
   data (a rating, the TV feed). That is not a reason to reshoot.

## The page tree

`pages.puml` has two diagrams, logged out and logged in. An arrow is a tap or a
pull-down entry that pushes a page, a dashed one a replacement or an app-wide event.
It has to match the `pageStack.push()`, `replace()` and `acceptDestination` calls
and what hides them (`session.loggedIn`).

1. **Update `pages.puml` when** a page is added, removed or renamed; when a tile,
   button or menu entry that opens a page is added, moved or removed; or when what
   decides whether it shows (the login state) changes.
2. Draw the page in the diagrams of each state it can be reached in, and mark pages
   that exist only when logged in (`<<new>>` in the logged-in diagram).
3. **Render both images** and commit them with the source:

       java -jar plantuml.jar -tpng -o docs docs/pages.puml

   PlantUML is not in the repository: download `plantuml-<version>.jar` from Maven
   Central (`net.sourceforge.plantuml:plantuml`). The diagrams use the `smetana`
   layout, so Graphviz is not needed.
4. Keep the script in step: a page whose way in changes may need other `states` in
   `tools/screenshot-pages.sh`.
