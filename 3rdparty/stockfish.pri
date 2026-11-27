# SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Stockfish (3rdparty/stockfish), vendored from the sf_19 tag with its own
# main.cpp, Makefile and universal-binary entry points left out: the app drives Stockfish::Engine directly
# instead of speaking UCI to a second process, which also keeps Harbour happy
# about there being one binary.
#
# The Makefile is where upstream keeps the per-architecture switches, so they
# are restated here. They follow "make ARCH=armv8", "ARCH=armv7-neon" and
# "ARCH=x86-32-sse2", the three that match the Sailfish targets.

SF = $$PWD/stockfish
INCLUDEPATH += $$SF

SOURCES += \
    $$SF/attacks.cpp \
    $$SF/benchmark.cpp \
    $$SF/bitboard.cpp \
    $$SF/engine.cpp \
    $$SF/evaluate.cpp \
    $$SF/memory.cpp \
    $$SF/misc.cpp \
    $$SF/movegen.cpp \
    $$SF/movepick.cpp \
    $$SF/position.cpp \
    $$SF/score.cpp \
    $$SF/search.cpp \
    $$SF/thread.cpp \
    $$SF/timeman.cpp \
    $$SF/tt.cpp \
    $$SF/tune.cpp \
    $$SF/uci.cpp \
    $$SF/ucioption.cpp \
    $$SF/nnue/network.cpp \
    $$SF/nnue/nnue_accumulator.cpp \
    $$SF/nnue/nnue_misc.cpp \
    $$SF/nnue/features/half_ka_v2_hm.cpp \
    $$SF/nnue/features/full_threats.cpp \
    $$SF/nnue/features/pp_3wide.cpp \
    $$SF/syzygy/tbprobe.cpp

# The network is downloaded on first use (NnueWeights) rather than built into
# the binary: it is close to 100 MB.
DEFINES += NNUE_EMBEDDING_OFF USE_PTHREADS

# The one place the vendored engine is patched (shm.h): no POSIX shared memory.
# Stockfish 19 would put its network in a shared segment and serve it to other
# Stockfish processes over a socket; there is only ever one here, and the
# sandbox does not offer that.
DEFINES += SF_NO_SHM

# An unoptimised Stockfish is not worth running, so the engine is built the
# way upstream builds it whatever the app's build mode is.
QMAKE_CXXFLAGS += -O3

equals(QT_ARCH, arm64) {
    # ARCH=armv8
    DEFINES += IS_64BIT USE_PREFETCH USE_POPCNT USE_NEON=8
} else:equals(QT_ARCH, arm) {
    # ARCH=armv7-neon. No -mfloat-abi here: Sailfish armv7hl is hard-float
    # and the target already says so.
    DEFINES += USE_PREFETCH USE_POPCNT USE_NEON=7
    QMAKE_CXXFLAGS += -mfpu=neon
    # 64-bit atomics are not inline on 32-bit ARM.
    LIBS += -latomic
} else:equals(QT_ARCH, i386) {
    # ARCH=x86-32-sse2, which is what the emulator runs.
    DEFINES += USE_PREFETCH USE_SSE2
    QMAKE_CXXFLAGS += -msse -msse2
} else:equals(QT_ARCH, x86_64) {
    # Only the host test build; upstream's x86-64-sse41-popcnt.
    DEFINES += IS_64BIT USE_PREFETCH USE_POPCNT USE_SSE2 USE_SSSE3 USE_SSE41
    QMAKE_CXXFLAGS += -msse -msse2 -mssse3 -msse4.1 -mpopcnt
} else {
    # Anything else builds, just without the hand-written fast paths.
    DEFINES += NO_PREFETCH
    contains(QMAKE_HOST.arch, .*64.*): DEFINES += IS_64BIT
}
