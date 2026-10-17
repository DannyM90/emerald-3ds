# RNG seeding on 3DS

Emerald initializes its primary RNG state to zero. Its optional upstream
`SeedRngWithRtc()` fix is guarded by `BUGFIX`; this port enables `UBFIX`, which
is a separate flag. Normal startup therefore repeated the same initial stream.

The 3DS port now seeds the existing RNG at every `AgbMain` entry, including a
soft reset, using a mix of the console's high-resolution system tick and wall
clock. New-game trainer-ID generation uses the same source: the GBA timer
register it otherwise reads is a memory-backed register that this port does
not advance. A startup-only fix would be overwritten by that zero reseed.
Only these seed inputs change. The RNG algorithm, encounter generation, shiny
checks, secondary RNG, and existing saves/Pokémon/trainer IDs are unchanged.
New games can naturally receive different trainer IDs.

The seed remains 16-bit, matching `SeedRng`. Zero is a valid result, collisions
are possible, and a different seed on every reset is not guaranteed. This is
not a cryptographic generator or an increase to shiny odds. Frame-based RNG
manipulation that assumes a zero startup seed will behave differently in
normal play. Scripted/gameplay reseeding (including battle recordings) is not
removed.

Measured scenarios retain the historical input at both seed points: recording
and playback use the caller's zero/timer fallback and never read live ticks or
wall time for these hooks. Existing recorded inputs remain deterministic.
This preserves the historical scenario behavior rather than introducing new
scenario entropy/metadata. The separate seeded species randomizer's own seed
and permutation are unaffected.

`make -C 3ds_port verify-rng-seeding PYTHON=python3 HOSTCC=cc` tests the actual
backend function, source variation, integer boundaries, scenario fallbacks,
trainer-ID propagation and guarded call sites. In a bootstrapped tree it
compiles the actual applied main.c seed blocks; in the overlay repository it
checks the patch and exercises equivalent blocks. Full `make verify` includes
this test. Physical-console reset behavior still requires hardware acceptance.
