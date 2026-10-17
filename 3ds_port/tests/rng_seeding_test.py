"""Exercise the production seed source and boot/new-game call sites on the host."""
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def function(text, signature):
    start = text.index(signature + '\n{')
    brace = text.index('{', start)
    depth, end = 1, brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--cc', default='cc')
    args = parser.parse_args()
    code = (ROOT / 'src/3ds_game_full.c').read_text()
    seed_fn = function(code, 'u16 Port_RngSeed(u16 fallback)')
    # Both source layouts must contain the hook; bootstrap does not copy patches.
    patch = ROOT.parent / 'patches/pokeemerald/0061-rng-seeding.patch'
    if patch.exists():
        hook = patch.read_text()
        assert '+    SeedRng(Port_RngSeed(0));' in hook
        assert '+    val = Port_RngSeed(val);' in hook
        assert '+#if defined(PLATFORM_3DS) && defined(PORT_BRIDGE)' in hook
        assert '+#elif defined(BUGFIX)' in hook
        callsite = '''#if defined(PLATFORM_3DS) && defined(PORT_BRIDGE)
    SeedRng(Port_RngSeed(0));
#elif defined(BUGFIX)
    SeedRngWithRtc();
#endif'''
        trainer = '''void SeedRngAndSetTrainerId(void)
{
    u16 val = REG_TM1CNT_L;
#if defined(PLATFORM_3DS) && defined(PORT_BRIDGE)
    val = Port_RngSeed(val);
#endif
    SeedRng(val);
    REG_TM1CNT_H = 0;
    sTrainerId = val;
}'''
    else:
        main_src = (ROOT.parent / 'src/main.c').read_text()
        agb = function(main_src, 'void AgbMain(void)')
        begin = agb.index('#if defined(PLATFORM_3DS) && defined(PORT_BRIDGE)')
        end = agb.index('#endif', begin) + len('#endif')
        callsite = agb[begin:end]
        trainer = function(main_src, 'void SeedRngAndSetTrainerId(void)')
        assert 'SeedRng(Port_RngSeed(0))' in callsite
        assert 'val = Port_RngSeed(val)' in trainer
        assert agb.index('InitMainCallbacks();') < begin < agb.index('ClearDma3Requests();')
    # Soft reset re-enters the same AgbMain after restoring the initial game data.
    soft = function(code, 'void CtrGame_SoftReset(void)')
    init = function(code, 'void CtrGame_Init(void)')
    assert 'longjmp(sAgbMainEntry, 1)' in soft
    assert init.index('setjmp(sAgbMainEntry)') < init.index('AgbMain();')
    harness = r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
typedef uint16_t u16;
static bool scenario;
static uint64_t ticks;
static int64_t wall;
static unsigned reads, calls, rtcCalls;
static u16 lastSeed, sTrainerId, REG_TM1CNT_L, REG_TM1CNT_H;
static bool CtrScenario_Active(void) { return scenario; }
static uint64_t CtrPlatform_Ticks(void) { ++reads; return ticks; }
static int64_t CtrPlatform_WallSeconds(void) { ++reads; return wall; }
static void SeedRng(u16 seed) { ++calls; lastSeed = seed; }
#ifdef BUGFIX
void SeedRngWithRtc(void) { ++rtcCalls; }
#endif
'''
    harness += seed_fn + '\nstatic void Boot(void)\n{\n' + callsite + '\n}\n' + trainer
    harness += r'''
int main(void)
{
    unsigned distinct = 0;
    bool seen[65536] = {0};
    wall = 1792240000;
    ticks = UINT64_C(0x123456789abcdef0);
    u16 initial = Port_RngSeed(0);
    assert(reads == 2);
    assert(Port_RngSeed(0) == initial); /* injected sources are reproducible */
    for (unsigned i = 0; i < 4096; ++i)
    {
        ticks = UINT64_C(0x123456789abcdef0) + i;
        u16 seed = Port_RngSeed(0);
        if (!seen[seed]) { seen[seed] = true; ++distinct; }
    }
    assert(distinct > 3800); /* not the same seed for equal-frame resets */
    ticks = 0;
    wall = 0;
    (void)Port_RngSeed(0); /* zero/unset wall clock is still a defined input */
    wall = INT64_MIN;
    ticks = UINT64_MAX;
    (void)Port_RngSeed(0); /* unsigned mixing has no signed overflow */
    scenario = true;
    reads = 0;
    assert(Port_RngSeed(0) == 0);
    assert(Port_RngSeed(0xabcd) == 0xabcd);
    assert(reads == 0); /* record/play never consult live entropy */
    scenario = false;
    ticks = 12345;
    wall = 1792240000;
    initial = Port_RngSeed(0);
    calls = rtcCalls = 0;
    Boot();
#if defined(PLATFORM_3DS) && defined(PORT_BRIDGE)
    assert(calls == 1 && rtcCalls == 0 && lastSeed == initial);
#elif defined(BUGFIX)
    assert(calls == 0 && rtcCalls == 1);
#else
    assert(calls == 0 && rtcCalls == 0);
#endif
    REG_TM1CNT_L = 0x3456;
    REG_TM1CNT_H = 0x80;
    calls = 0;
    SeedRngAndSetTrainerId();
    assert(calls == 1 && REG_TM1CNT_H == 0 && sTrainerId == lastSeed);
#if defined(PLATFORM_3DS) && defined(PORT_BRIDGE)
    assert(lastSeed == initial);
#else
    assert(lastSeed == 0x3456);
#endif
    scenario = true;
    reads = calls = rtcCalls = 0;
    Boot();
    SeedRngAndSetTrainerId();
    assert(lastSeed == 0x3456 && sTrainerId == 0x3456 && reads == 0);
#if defined(PLATFORM_3DS) && defined(PORT_BRIDGE)
    assert(calls == 2 && rtcCalls == 0);
#else
    assert(calls == 1);
#endif
    puts("PASS RNG seed: boot, trainer reseed, scenario fallback, source variation, bounds");
    return 0;
}
'''
    with tempfile.TemporaryDirectory() as tmp:
        source = Path(tmp) / 'seed.c'
        source.write_text(harness)
        for flags in [[], ['-DBUGFIX'], ['-DPLATFORM_3DS', '-DPORT_BRIDGE'],
                      ['-DPLATFORM_3DS', '-DPORT_BRIDGE', '-DBUGFIX']]:
            binary = Path(tmp) / 'seed'
            subprocess.run([args.cc, '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                            *flags, str(source), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    main()
