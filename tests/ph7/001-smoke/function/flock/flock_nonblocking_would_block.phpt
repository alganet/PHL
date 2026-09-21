--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
flock() honours LOCK_NB and writes &$would_block
--FILE--
<?php
$flknb_path = tempnam(sys_get_temp_dir(), 'flknb');
$flknb_a = fopen($flknb_path, 'w');
$flknb_b = fopen($flknb_path, 'w');
// A granted lock writes 0, whether or not LOCK_NB was asked for.
$flknb_w = 'PRESET';
var_dump(flock($flknb_a, LOCK_EX | LOCK_NB, $flknb_w), $flknb_w);
// A second holder asking non-blockingly is REFUSED rather than waiting, and the
// out-param says it was contention, not an IO error.
$flknb_w = 'PRESET';
var_dump(flock($flknb_b, LOCK_EX | LOCK_NB, $flknb_w), $flknb_w);
$flknb_w = 'PRESET';
var_dump(flock($flknb_b, LOCK_SH | LOCK_NB, $flknb_w), $flknb_w);
// Once released, the same request succeeds.
var_dump(flock($flknb_a, LOCK_UN, $flknb_w), $flknb_w);
$flknb_w = 'PRESET';
var_dump(flock($flknb_b, LOCK_EX | LOCK_NB, $flknb_w), $flknb_w);
// A shared lock is grantable twice over.
var_dump(flock($flknb_b, LOCK_UN));
var_dump(flock($flknb_a, LOCK_SH), flock($flknb_b, LOCK_SH | LOCK_NB, $flknb_w), $flknb_w);
// A stream that cannot lock at all answers false. (Its $would_block is NOT
// asserted cross-engine: php pre-sets 0 and then overwrites with 1 whenever the
// leftover errno happens to be EWOULDBLOCK, so the value there reports the
// PREVIOUS syscall. PHL writes a deterministic 0.)
$flknb_mem = fopen('php://memory', 'r+');
var_dump(flock($flknb_mem, LOCK_EX));
// The invalid-operation ValueError throws BEFORE the out-param is written.
$flknb_w = 'PRESET';
try {
    flock($flknb_a, 0, $flknb_w);
} catch (\ValueError $e) {
    echo $e->getMessage(), "\n";
}
var_dump($flknb_w);
fclose($flknb_a);
fclose($flknb_b);
unlink($flknb_path);
?>
--EXPECT--
bool(true)
int(0)
bool(false)
int(1)
bool(false)
int(1)
bool(true)
int(0)
bool(true)
int(0)
bool(true)
bool(true)
bool(true)
int(0)
bool(false)
flock(): Argument #2 ($operation) must be one of LOCK_SH, LOCK_EX, or LOCK_UN
string(6) "PRESET"
--CLEAN--
<?php
