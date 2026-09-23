--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
chroot() reports why it could not change the root directory
--SKIPIF--
skip: win macos for now
--FILE--
<?php
/* php answers false and says why -- `chroot(): <strerror> (errno N)` -- where
 * PHL used to answer the bare false in silence, so a refused chroot() and one
 * that had done nothing looked identical to the caller. The handler takes the
 * message BODY: the runner's prefix differs between the two engines. */
set_error_handler(function ($n, $s) { echo 'W: ', $s, "\n"; return true; });
var_dump(chroot('/'));
restore_error_handler();
?>
--EXPECT--
W: chroot(): Operation not permitted (errno 1)
bool(false)
--CLEAN--
<?php
