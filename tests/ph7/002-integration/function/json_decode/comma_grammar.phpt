--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
JSON wants exactly one comma between two elements and none anywhere else
--FILE--
<?php
/* php's JSON grammar puts a comma BETWEEN two elements and nowhere else: not
 * before the first one, not two in a row, and not in front of the closing
 * bracket. Every spelling below that breaks that rule is a syntax error there,
 * at every nesting depth and in an object exactly as in an array -- and a
 * missing comma is one too, so `{"a":1 "b":2}` does not decode.
 *
 * `json_validate()` is the same parser and answers the same question, which is
 * the reason this matters beyond decoding: a program that screens untrusted
 * input with json_validate() is only as strict as the parser under it.
 */
$cases = [
    '[]', '[ ]', '{}', '{ }',
    '[1]', '[1,2]', '{"a":1}', '{"a":1,"b":2}',
    '[1,2,]', '[1,]', '[1,,2]', '[,1]', '[,]', '[,,]', '[1,2,,]',
    '{"a":1,}', '{"a":1,,"b":2}', '{,"a":1}', '{,}', '{"a":1,"b":2,}',
    '{"a":1 "b":2}', '[1 2]',
    '[[1,],2]', '{"a":[1,]}', '{"a":{"b":1,}}', '[[1],[2]]', '{"a":{"b":1}}',
];
foreach ($cases as $j) {
    $r = json_decode($j, true);
    printf("%-18s decode=%-14s err=%d %-13s validate=%s\n",
        $j,
        $r === null ? 'NULL' : json_encode($r),
        json_last_error(), json_last_error_msg(),
        var_export(json_validate($j), true));
}
/* The throwing door reports the same refusal. */
try {
    json_decode('[1,]', true, 512, JSON_THROW_ON_ERROR);
} catch (JsonException $e) {
    echo get_class($e), '(', $e->getCode(), '): ', $e->getMessage(), "\n";
}
--EXPECT--
[]                 decode=[]             err=0 No error      validate=true
[ ]                decode=[]             err=0 No error      validate=true
{}                 decode=[]             err=0 No error      validate=true
{ }                decode=[]             err=0 No error      validate=true
[1]                decode=[1]            err=0 No error      validate=true
[1,2]              decode=[1,2]          err=0 No error      validate=true
{"a":1}            decode={"a":1}        err=0 No error      validate=true
{"a":1,"b":2}      decode={"a":1,"b":2}  err=0 No error      validate=true
[1,2,]             decode=NULL           err=4 Syntax error  validate=false
[1,]               decode=NULL           err=4 Syntax error  validate=false
[1,,2]             decode=NULL           err=4 Syntax error  validate=false
[,1]               decode=NULL           err=4 Syntax error  validate=false
[,]                decode=NULL           err=4 Syntax error  validate=false
[,,]               decode=NULL           err=4 Syntax error  validate=false
[1,2,,]            decode=NULL           err=4 Syntax error  validate=false
{"a":1,}           decode=NULL           err=4 Syntax error  validate=false
{"a":1,,"b":2}     decode=NULL           err=4 Syntax error  validate=false
{,"a":1}           decode=NULL           err=4 Syntax error  validate=false
{,}                decode=NULL           err=4 Syntax error  validate=false
{"a":1,"b":2,}     decode=NULL           err=4 Syntax error  validate=false
{"a":1 "b":2}      decode=NULL           err=4 Syntax error  validate=false
[1 2]              decode=NULL           err=4 Syntax error  validate=false
[[1,],2]           decode=NULL           err=4 Syntax error  validate=false
{"a":[1,]}         decode=NULL           err=4 Syntax error  validate=false
{"a":{"b":1,}}     decode=NULL           err=4 Syntax error  validate=false
[[1],[2]]          decode=[[1],[2]]      err=0 No error      validate=true
{"a":{"b":1}}      decode={"a":{"b":1}}  err=0 No error      validate=true
JsonException(4): Syntax error
