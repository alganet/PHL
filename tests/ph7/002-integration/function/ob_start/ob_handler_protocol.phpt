--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An output handler runs on the way OUT, once per operation, and is told which one: the $phase argument, $chunk_size, and what its answer means
--FILE--
<?php
$obp_log = [];
function obp_say($obp_what, ...$obp_vals)
{
    global $obp_log;
    $obp_log[] = $obp_what . ': ' . implode(' ', array_map(fn ($v) => var_export($v, true), $obp_vals));
}
function obp_trace($obp_b, $obp_p)
{
    obp_say('  handler', $obp_b, $obp_p);
    return "<$obp_b>";
}

// php runs the handler ON THE WAY OUT, once per OPERATION, and tells it which
// operation through $phase. PHL filtered at WRITE time with a phase hardcoded to
// 0, so a handler saw one call per echo and could never tell a flush from a
// clean from the final call.
foreach (['ob_clean', 'ob_flush', 'ob_end_clean', 'ob_end_flush', 'ob_get_clean', 'ob_get_flush'] as $obp_op) {
    ob_start();          // catch what the operation sends, so stdout stays readable
    ob_start('obp_trace');
    echo "abc";
    obp_say($obp_op, $obp_op());
    while (ob_get_level() > 1) { ob_end_flush(); }
    obp_say('  sent', ob_get_clean());
}

// ob_get_contents()/ob_get_length() do not run the handler at all, and what they
// answer is the RAW buffer -- the handler has not seen it yet.
ob_start('obp_trace');
echo "raw";
obp_say('contents', ob_get_contents(), ob_get_length());
ob_end_clean();

// $chunk_size: declared in the signature, never read. php writes out as soon as
// the buffer holds that many bytes, so a long-running page streams instead of
// holding everything to the end.
ob_start();
ob_start('obp_trace', 4);
echo "12";
echo "34";
echo "56789";
ob_end_flush();
obp_say('chunked', ob_get_clean());

// What the handler ANSWERS decides what is sent. FALSE means "the call failed":
// the ORIGINAL bytes go out and the handler is never called again. TRUE means
// "no data". Anything else is cast to a string, NULL included.
$obp_answers = [
    'false'  => fn ($b, $p) => false,
    'true'   => fn ($b, $p) => true,
    'null'   => fn ($b, $p) => null,
    'empty'  => fn ($b, $p) => '',
    'int'    => fn ($b, $p) => 42,
];
foreach ($obp_answers as $obp_what => $obp_cb) {
    ob_start();
    ob_start($obp_cb);
    echo "a";
    ob_flush();
    echo "b";
    ob_end_flush();
    obp_say('answers ' . $obp_what, ob_get_clean());
}

// A handler may not print: php is mid-operation on the buffer stack and has
// nowhere to put it, so the output is dropped (it used to recurse into the
// handler 15 deep and prepend the result).
ob_start();
ob_start(function ($obp_b, $obp_p) { echo "INSIDE"; return "[$obp_b]"; });
echo "x";
ob_end_flush();
obp_say('handler output', ob_get_clean());

// Nested buffers finish innermost-first, so each handler is handed what the one
// inside it made -- at an explicit end and at shutdown alike.
ob_start();
ob_start(fn ($b, $p) => "[o:$b]");
echo "1";
ob_start(fn ($b, $p) => "(i:$b)");
echo "2";
ob_end_flush();
echo "3";
ob_end_flush();
obp_say('nested', ob_get_clean());

// A handler that THROWS is php's other failure shape: the original bytes go out,
// the exception reaches the enclosing catch, and the handler is DISABLED -- so it
// is not called again (it used to throw a SECOND time out of the shutdown flush)
// and the buffer stops buffering, passing later output straight through.
$obp_calls = 0;
ob_start();
try {
    ob_start(function ($obp_b, $obp_p) use (&$obp_calls) {
        $obp_calls++;
        throw new RuntimeException("from the handler");
    });
    echo "payload";
    ob_flush();
    obp_say('not reached');
} catch (Throwable $obp_e) {
    obp_say('caught', get_class($obp_e), $obp_e->getMessage());
}
obp_say('handler calls', $obp_calls, 'level', ob_get_level());
echo "after";
obp_say('buffer stopped buffering', ob_get_contents());
while (ob_get_level() > 1) { ob_end_flush(); }
obp_say('outer holds', ob_get_clean());
obp_say('handler calls after the flush', $obp_calls);

echo implode("\n", $obp_log), "\n";
?>
--EXPECT--
  handler: 'abc' 3
ob_clean: true
  handler: '' 8
  sent: '<>'
  handler: 'abc' 5
ob_flush: true
  handler: '' 8
  sent: '<abc><>'
  handler: 'abc' 11
ob_end_clean: true
  sent: ''
  handler: 'abc' 9
ob_end_flush: true
  sent: '<abc>'
  handler: 'abc' 11
ob_get_clean: 'abc'
  sent: ''
  handler: 'abc' 9
ob_get_flush: 'abc'
  sent: '<abc>'
contents: 'raw' 3
  handler: 'raw' 11
  handler: '1234' 1
  handler: '56789' 0
  handler: '' 8
chunked: '<1234><56789><>'
answers false: 'ab'
answers true: ''
answers null: ''
answers empty: ''
answers int: '4242'
handler output: '[x]'
nested: '[o:1(i:2)3]'
caught: 'RuntimeException' 'from the handler'
handler calls: 1 'level' 2
buffer stopped buffering: ''
outer holds: 'payloadafter'
handler calls after the flush: 1
