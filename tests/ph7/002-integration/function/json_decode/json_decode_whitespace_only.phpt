--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
json_decode/json_validate: whitespace-only input is a syntax error, not a crash
--DESCRIPTION--
A whitespace-only document tokenizes to NO tokens at all, so the decoder used to
start by dereferencing the end of an EMPTY token set (a NULL base pointer) and
segfault. php reports JSON_ERROR_SYNTAX and answers null / false; a missing member
value ('{"a":') walks off the same end one token later.
--FILE--
<?php
foreach ([' ', "\n", "\t", "\r\n  \t", '', '{"a":', '[1,'] as $bad) {
    $r = json_decode($bad);
    $err = json_last_error();          // json_encode() below would reset it
    printf("%-8s dec=%s err=%d valid=%s\n", json_encode($bad), var_export($r, true),
        $err, var_export(json_validate($bad), true));
}
echo json_last_error_msg(), "\n";

// JSON_THROW_ON_ERROR reports the same syntax error rather than running off the end.
try {
    json_decode("  \t ", false, 512, JSON_THROW_ON_ERROR);
} catch (JsonException $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}

// A whitespace-padded VALUE still decodes; only the empty document is an error.
var_dump(json_decode("  1 "), json_decode(" [ 1 , 2 ] ", true));
?>
--EXPECT--
" "      dec=NULL err=4 valid=false
"\n"     dec=NULL err=4 valid=false
"\t"     dec=NULL err=4 valid=false
"\r\n  \t" dec=NULL err=4 valid=false
""       dec=NULL err=4 valid=false
"{\"a\":" dec=NULL err=4 valid=false
"[1,"    dec=NULL err=4 valid=false
Syntax error
JsonException: Syntax error
int(1)
array(2) {
  [0]=>
  int(1)
  [1]=>
  int(2)
}
--CLEAN--
<?php
