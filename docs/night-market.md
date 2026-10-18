# Night Market

The expanded east-world PC build replaces the duplicated west Nook storefront with the Night Market. The native center-town Nook storefront remains the starter shop; the east storefront is reserved for the maritime outfitter.

Enter the west Nook storefront through its native door, then interact with the interior seller to open Milo's Night Market. The distinct Nookling profile and purple uniform are scoped to this room. Sales run from 8 PM through 4:59 AM in in-game time; outside these hours the catalog displays its hours and a closed message. The repurposed door no longer inherits the center Nook shop's closing hours. Six items are offered each night: two Famicom curios and four seasonal Halloween, snow, or holiday decorations. The decorative selection rotates each day through a ten-item pool. Prices are shown in Bells. The market uses the resident's normal wallet and 15 pocket slots; an item is added before Bells are deducted, so a full inventory or insufficient wallet leaves both unchanged.

The twelve-item pool appears in native event furniture lists rather than regular Nook's common, uncommon, and rare furniture lists. The two Famicom pieces are event collectibles, and the seasonal curios are event furniture. This keeps the market's listings out of standard daytime Nook stock while preserving native item behavior and collection tracking.

The shared menu accepts keyboard arrows/Enter/Escape and controller D-pad/A/B. Service catalogs can provide an `on_confirm` callback for actions such as customization; callback rows with zero price are presented as choices. Catalogs support up to eight visible rows, a title, hours/context text, wallet display, and status feedback.

## Smoke checks

`pc/tests/world_shops_smoke.c` covers the 8 PM–5 AM boundary, six unique nightly offerings, the complete twelve-item rotation, night-only classification, and purchase outcomes for success, daytime rejection, insufficient Bells, and full pockets. It also checks controller close, pause restoration, queued-input draining, and roof paint owner/funds/repeat protection. The market should also be checked in the dev build at the west Nook door: confirm the closed daytime message, one successful purchase, and unchanged wallet/pockets after a failed purchase.
