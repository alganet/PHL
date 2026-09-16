#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause
#
# Guarded probe runner for cross-engine experiments.
#
#   build-aux/safeprobe.sh <engine> [engine args...]
#   build-aux/safeprobe.sh /usr/bin/php -r 'function f(){return f();} f();'
#
# Why this exists: php enforces no function-nesting depth, so a plain
# `php -r 'function f(){return f();} f();'` recurses until it exhausts the
# machine -- which is exactly how one such probe killed an editor session on
# 20 Jul 2026. Any probe that can recurse, allocate without bound, or produce
# unbounded output belongs here.
#
# Caps apply to the CHILD only (a subshell), so the calling shell is never at
# risk. Bound the engine as well where it offers a knob: PHL_MAX_RECURSION=64
# makes the PHL half of a recursion probe finish instantly.
(
  ulimit -v 4194304   # address space, KB (php reserves large arenas up front)
  ulimit -s 8192      # stack, KB
  ulimit -t 20        # CPU seconds
  ulimit -c 0         # no core dumps
  exec timeout -s KILL 30 "$@"
) 2>&1 | head -c 16384 | head -200
