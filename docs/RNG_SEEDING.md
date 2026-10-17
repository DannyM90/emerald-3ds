# RNG seeding on 3DS

Emerald initializes its primary RNG state to zero. Its optional upstream
`SeedRngWithRtc()` fix is guarded by `BUGFIX`; this port enables `UBFIX`, which
is a separate flag. Normal startup therefore repeated the same initial stream.

The 3DS port now seeds the existing RNG at every `AgbMain` entry, including a
soft reset, using a mix of the console's high-resolution system tick and wall
clock. The reachable portable `InitPlayerTrainerId` path also samples the
same source immediately before its two `Random()` calls. The GBA naming
screen timer hook is disabled by `PORTABLE`, so it is not used for this fix.
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
and playback use zero at startup and skip new-game reseeding entirely, so
the portable trainer-ID path continues its historical RNG state. These hooks
never read live ticks or wall time during scenarios. Existing recorded inputs
remain deterministic.
This preserves the historical scenario behavior rather than introducing new
scenario entropy/metadata. The separate seeded species randomizer's own seed
and permutation are unaffected.

`make -C 3ds_port verify-rng-seeding PYTHON=python3 HOSTCC=cc` tests the actual
backend function, source variation, integer boundaries, scenario fallbacks,
trainer-ID propagation and guarded call sites. It compiles the actual applied
`main.c` startup block and `new_game.c` trainer-ID function in eight macro
configurations, including `PORTABLE`, and checks that scenario new games do not
reseed or truncate the previous RNG state. From an overlay checkout, provide
an applied source tree explicitly:

```
python3 3ds_port/tests/rng_seeding_test.py --source-tree /path/to/bootstrapped/tree
```

Full `make verify` in a bootstrapped tree includes this test. Physical-console
reset behavior still requires hardware acceptance.
