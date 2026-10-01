--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A `?->` beside an assignment is READ, not written
--DESCRIPTION--
php binds `A op $lv = B` as `A op ($lv = B)`: the assignment takes the immediate
lvalue on its left, not the whole binary subtree. So in
`$o?->m() !== null && $x = f()` the write target is `$x` and the nullsafe belongs
to a comparison that is merely read. PHL asked its PHP 8.0 refusal
("Can't use nullsafe operator in write context") of the WHOLE left operand, so the
shape was a compile fatal -- aws-sdk-php's AbstractRpcV2ErrorParser writes it, and
so does anything that tests a nullsafe call and captures a value in one condition.

The refusal itself is unchanged for every chain that IS the target, including the
ones reached through a neighbouring operator.
--FILE--
<?php
class NwnC {
    public $p;
    public function m() { return 'v'; }
}
/* A FILE rather than `-r`: cmd.exe does not quote an inline program the way a
 * POSIX shell does, and the message is reduced to a token so no path or line
 * number reaches the expectation. No output is shell_exec()'s NULL in both
 * engines, and PLAN.md §10 makes handing that to trim() a TypeError here. */
function nwnRun($src) {
    $f = sys_get_temp_dir() . '/phl_nwn_' . getmypid() . '.php';
    file_put_contents($f, "<?php\n" . $src . "\n");
    $out = trim((string) shell_exec(escapeshellarg(PHP_BINARY) . ' ' . escapeshellarg($f) . ' 2>&1'));
    @unlink($f);
    if ($out === '') { echo "ok\n"; return; }
    if (strpos($out, "Can't use nullsafe operator in write context") !== false) {
        echo "nullsafe-write\n";
    } elseif (strpos($out, 'Cannot take reference of a nullsafe chain') !== false) {
        echo "nullsafe-ref\n";
    } else {
        echo 'OTHER: ', trim(explode("\n", $out)[0]), "\n";
    }
}

// The neighbour shapes, which must RUN.
$nwnO = new NwnC;
$nwnH = true;
if ($nwnO?->m() !== null && $nwnH && $nwnE = $nwnO->m()) {
    echo "captured:$nwnE\n";
}
$nwnN = null;
var_dump($nwnN?->m() && $nwnX = 1, isset($nwnX));
var_dump($nwnN?->m() || $nwnY = 2, $nwnY);
var_dump($nwnO?->m() !== null && $nwnZ = 3, $nwnZ);

// And the target shapes, which must not.
nwnRun('$a = null; $a?->b = 1;');
nwnRun('$a = null; $a?->b->c = 1;');
nwnRun('$a = null; $a?->b[0] = 1;');
nwnRun('$a = null; $q = 1; $q && $a?->b = 1;');
nwnRun('$a = null; $q = 1; $q && $a?->b->c = 1;');
nwnRun('$a = null; !$a?->b = 1;');
nwnRun('$a = null; $a?->b += 1;');
nwnRun('$a = null; $a?->b ??= 1;');
nwnRun('$a = null; $a?->b++;');
nwnRun('$a = null; unset($a?->b);');
nwnRun('$a = null; $r = 1; $a?->b =& $r;');
nwnRun('$a = null; $r =& $a?->b;');
// The spine walk php shares with them still reaches its lvalue.
nwnRun('while (false !== $p = strpos("ab","b")) { break; } echo "";');
nwnRun('function nwf(){ return 1; } $c = 1; $c && !$d = nwf(); echo "";');
?>
--EXPECT--
captured:v
bool(false)
bool(false)
bool(true)
int(2)
bool(true)
int(3)
nullsafe-write
nullsafe-write
nullsafe-write
nullsafe-write
nullsafe-write
nullsafe-write
nullsafe-write
nullsafe-write
nullsafe-write
nullsafe-write
nullsafe-write
nullsafe-ref
ok
ok
