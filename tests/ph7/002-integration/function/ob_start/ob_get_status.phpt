--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ob_get_status() describes the active buffers: handler name, type, flags, level, chunk size and php's own allocation bookkeeping
--FILE--
<?php
// ob_get_status() did not exist, so the standard way to ask what an output
// handler IS and what it may do -- the guard every framework writes before it
// touches somebody else's buffer -- was an undefined-function fatal.
var_dump(ob_get_status(), ob_get_status(true));

class ObStK2
{
    public function m($obs_b, $obs_p) { return $obs_b; }
    public static function s($obs_b, $obs_p) { return $obs_b; }
}
function obs_named($obs_b, $obs_p) { return $obs_b; }

// The NAME is the callable's own display name. It used to be "Class Method" for
// every array callback and "default output handler" for every CLOSURE, so a
// closure handler was indistinguishable from no handler at all.
$obs_cbs = [null, 'obs_named', [new ObStK2, 'm'], ['ObStK2', 's'], 'ObStK2::s'];
$obs_cbs[] = function ($obs_b, $obs_p) { return $obs_b; };
foreach ($obs_cbs as $obs_cb) {
    ob_start($obs_cb);
}
$obs_full = ob_get_status(true);
$obs_top = ob_get_status();
for ($obs_i = 0; $obs_i < 6; $obs_i++) {
    ob_end_clean();
}
foreach ($obs_full as $obs_one) {
    // The closure's name carries this file's path; keep the shape, drop the path.
    $obs_one['name'] = preg_replace('/\{closure:.*:(\d+)\}/', '{closure:LINE $1}', $obs_one['name']);
    echo implode(' ', array_map(fn ($k, $v) => "$k=$v", array_keys($obs_one), $obs_one)), "\n";
}
var_dump($obs_top['level'], $obs_top['type']);
echo "handlers: ", implode(', ', array_map(
    fn ($n) => preg_replace('/\{closure:.*:(\d+)\}/', '{closure:LINE $1}', $n),
    (function () {
        $obs_r = [];
        foreach ([null, 'obs_named', [new ObStK2, 'm']] as $obs_cb) { ob_start($obs_cb); }
        $obs_r = ob_list_handlers();
        for ($obs_i = 0; $obs_i < 3; $obs_i++) { ob_end_clean(); }
        return $obs_r;
    })())), "\n";

// $chunk_size and the buffer's own size are reported too: php starts at 16 KB, or
// the chunk size rounded up to a 4 KB boundary, and grows by whichever is larger
// of that and the shortfall rounded the same way.
foreach ([0, 1, 100, 4096, 4097, 8192] as $obs_chunk) {
    ob_start('obs_named', $obs_chunk, PHP_OUTPUT_HANDLER_STDFLAGS);
    $obs_s = ob_get_status();
    ob_end_clean();
    echo "chunk=$obs_chunk size={$obs_s['buffer_size']} reported={$obs_s['chunk_size']}\n";
}
foreach ([0, 1, 4096, 16383, 16384, 20000, 32768, 40000, 100000] as $obs_n) {
    ob_start();
    if ($obs_n) { echo str_repeat('x', $obs_n); }
    $obs_s = ob_get_status();
    ob_end_clean();
    echo "used={$obs_s['buffer_used']} size={$obs_s['buffer_size']}\n";
}

// ...and the allocation depends on how the bytes ARRIVED, not just on how many
// there are, which is why it is tracked rather than derived from the length.
foreach ([[20, 1000], [40, 1000], [100, 1000]] as [$obs_times, $obs_each]) {
    ob_start();
    for ($obs_i = 0; $obs_i < $obs_times; $obs_i++) { echo str_repeat('y', $obs_each); }
    $obs_s = ob_get_status();
    ob_end_clean();
    echo "{$obs_times} x {$obs_each} (used={$obs_s['buffer_used']}) size={$obs_s['buffer_size']}\n";
}

// A handler that FAILED leaves the buffer disabled, and php reports that: the
// flags carry STARTED|DISABLED and the buffer is gone (size 0), which is the
// machine-readable form of "this level stopped buffering".
ob_start(fn ($obs_b, $obs_p) => false);
echo "a";
ob_flush();
$obs_s = ob_get_status();
ob_end_flush();
printf("failed handler: flags=%d started=%d disabled=%d processed=%d size=%d\n",
    $obs_s['flags'],
    (bool)($obs_s['flags'] & PHP_OUTPUT_HANDLER_STARTED),
    (bool)($obs_s['flags'] & PHP_OUTPUT_HANDLER_DISABLED),
    (bool)($obs_s['flags'] & PHP_OUTPUT_HANDLER_PROCESSED),
    $obs_s['buffer_size']);

// A buffer with NO handler has php's internal one, and that one is taken to have
// run -- and produced its output -- from the first operation onwards, empty
// buffer included.
ob_start();
$obs_a = ob_get_status()['flags'];
echo "abc";
$obs_b = ob_get_status()['flags'];
ob_clean();
$obs_c = ob_get_status()['flags'];
ob_end_clean();
printf("internal handler: fresh=%d after-write=%d after-clean=%d\n", $obs_a, $obs_b, $obs_c);

// Asked from INSIDE its own handler, a buffer reports what it is, not the
// bookkeeping of the call in progress: the first call has not STARTED yet, and
// the buffer still has its allocation.
ob_start(function ($obs_b2, $obs_p) {
    $obs_s2 = ob_get_status();
    return $obs_s2['flags'] . '/' . $obs_s2['buffer_size'] . ' ';
});
echo "x";
ob_flush();
echo "y";
ob_end_flush();
echo "\n";

// The refused ob_get_clean()/ob_get_flush() still ANSWER the contents they could
// not take away -- as they were when the call was made, not after the two
// notices, which reach a user error handler that is php code and may print into
// this very buffer.
set_error_handler(function ($obs_n, $obs_m) { echo "<E>"; return true; });
ob_start(null, 0, 0);
echo "PAYLOAD";
var_dump(ob_get_clean());
ob_start(null, 0, 0);
echo "PAYLOAD2";
var_dump(ob_get_flush());
restore_error_handler();

// php keeps whatever $flags it is given except the two nibbles it reserves for
// itself -- the phase bits and its own STARTED/DISABLED/PROCESSED state -- and
// reports the rest back verbatim, sign and all; and a negative $chunk_size is
// no chunk at all. (A chunk past 32 bits is a real allocation of that size in
// php, which a 128M memory_limit refuses, so it is not asked here.)
foreach ([0, 1, 15, 16, 112, 128, 255, 4080, 4096, 0xF000, 0xFFFF, -1, 0x10000] as $obs_f) {
    ob_start(null, 0, $obs_f);
    $obs_s = ob_get_status();
    if (!@ob_end_clean()) { @ob_get_clean(); }
    printf("flags in=%-8d out=%d\n", $obs_f, $obs_s['flags']);
}
foreach ([-1, 0, 1, 4096, 100000] as $obs_c) {
    ob_start(null, $obs_c);
    $obs_s = ob_get_status();
    ob_end_clean();
    printf("chunk in=%-12s reported=%s size=%s\n", $obs_c, $obs_s['chunk_size'], $obs_s['buffer_size']);
}

// A handler answering TRUE ("no data") counts as having PROCESSED the buffer.
ob_start(fn ($obs_b3, $obs_p3) => true);
echo "abc";
ob_flush();
$obs_s = ob_get_status();
ob_end_flush();
printf("true handler: flags=%d processed=%d\n", $obs_s['flags'], (bool)($obs_s['flags'] & PHP_OUTPUT_HANDLER_PROCESSED));
?>
--EXPECT--
array(0) {
}
array(0) {
}
name=default output handler type=0 flags=112 level=0 chunk_size=0 buffer_size=16384 buffer_used=0
name=obs_named type=1 flags=113 level=1 chunk_size=0 buffer_size=16384 buffer_used=0
name=ObStK2::m type=1 flags=113 level=2 chunk_size=0 buffer_size=16384 buffer_used=0
name=ObStK2::s type=1 flags=113 level=3 chunk_size=0 buffer_size=16384 buffer_used=0
name=ObStK2::s type=1 flags=113 level=4 chunk_size=0 buffer_size=16384 buffer_used=0
name={closure:LINE 18} type=1 flags=113 level=5 chunk_size=0 buffer_size=16384 buffer_used=0
int(5)
int(1)
handlers: default output handler, obs_named, ObStK2::m
chunk=0 size=16384 reported=0
chunk=1 size=4096 reported=1
chunk=100 size=4096 reported=100
chunk=4096 size=4096 reported=4096
chunk=4097 size=8192 reported=4097
chunk=8192 size=8192 reported=8192
used=0 size=16384
used=1 size=16384
used=4096 size=16384
used=16383 size=16384
used=16384 size=32768
used=20000 size=32768
used=32768 size=32768
used=40000 size=40960
used=100000 size=102400
20 x 1000 (used=20000) size=32768
40 x 1000 (used=40000) size=49152
100 x 1000 (used=100000) size=114688
afailed handler: flags=12401 started=1 disabled=1 processed=0 size=0
internal handler: fresh=112 after-write=112 after-clean=20592
113/16384 20593/16384 
PAYLOAD<E><E>string(7) "PAYLOAD"
PAYLOAD2<E><E>string(8) "PAYLOAD2"
flags in=0        out=0
flags in=1        out=0
flags in=15       out=0
flags in=16       out=16
flags in=112      out=112
flags in=128      out=128
flags in=255      out=240
flags in=4080     out=4080
flags in=4096     out=0
flags in=61440    out=0
flags in=65535    out=4080
flags in=-1       out=-61456
flags in=65536    out=65536
chunk in=-1           reported=0 size=16384
chunk in=0            reported=0 size=16384
chunk in=1            reported=1 size=4096
chunk in=4096         reported=4096 size=4096
chunk in=100000       reported=100000 size=102400
true handler: flags=20593 processed=1
