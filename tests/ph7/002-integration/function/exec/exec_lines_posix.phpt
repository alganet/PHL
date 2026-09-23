--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
exec()/system()/passthru(): php's line rules, the $output array and $result_code
--SKIPIF--
<?php
if (PHP_OS_FAMILY === "Windows") {
    echo "skip these are the LINE rules and they are spelled with printf/awk, which cmd.exe has no equivalent of; the platform-neutral half is exec_basics.phpt";
}
?>
--FILE--
<?php
echo "== exec: last line, output array, result code\n";
$r = exec('printf "alpha\nbeta\ngamma\n"', $out, $code);
var_dump($r, $out, $code);

echo "== exec appends to an array the caller already holds\n";
$out2 = ['kept'];
exec('printf "one\ntwo\n"', $out2);
var_dump($out2);

echo "== trailing whitespace is stripped, leading is not\n";
$r = exec('printf "  padded   \n"', $out3);
var_dump($r, $out3);

echo "== output with no final newline\n";
$r = exec('printf "p\nq"', $out4);
var_dump($r, $out4);

echo "== a command that prints nothing answers \"\", not false\n";
$r = exec('exit 0', $out5, $code5);
var_dump($r, $out5, $code5);

echo "== the exit status reaches result_code\n";
exec('exit 7', $out6, $code6);
var_dump($code6);
exec('printf "x\n"; exit 3', $out7, $code7);
var_dump($code7);

echo "== a non-array output argument is replaced\n";
$out8 = 'not an array';
exec('printf "z\n"', $out8);
var_dump($out8);

echo "== system writes every line, newline and all, and answers the last\n";
$r = system('printf "s1\ns2  \n"', $scode);
var_dump($r, $scode);

echo "== system output goes through the output buffer\n";
ob_start();
$r = system('printf "b1\nb2\n"');
$buf = ob_get_clean();
var_dump($buf, $r);

echo "== passthru writes raw bytes and answers null\n";
$r = passthru('printf "raw-no-newline"', $pcode);
var_dump($r, $pcode);

echo "\n== passthru is byte-exact: no line handling at all\n";
ob_start();
passthru('printf "a\n\nb   \n"');
$raw = ob_get_clean();
var_dump($raw);

echo "== a line longer than the read buffer is still ONE line\n";
$long = exec('awk \'BEGIN{for(i=0;i<10000;i++)printf "X"; print ""; print "tail"}\'', $lout);
var_dump(count($lout), strlen($lout[0]), $lout[1], $long);

echo "== NUL bytes inside a line survive\n";
$nul = exec('printf "a\0b\nc\0d\n"', $nout);
var_dump(bin2hex($nout[0]), bin2hex($nout[1]), bin2hex($nul));

echo "== a CR is trailing whitespace like any other\n";
$crlf = exec('printf "one\r\ntwo\r\n"', $cout);
var_dump($cout, $crlf);

echo "== blank and whitespace-only lines are kept, and empty the answer\n";
$blank = exec('printf "x\n\n   \n"', $bout);
var_dump($bout, $blank);
?>
--EXPECT--
== exec: last line, output array, result code
string(5) "gamma"
array(3) {
  [0]=>
  string(5) "alpha"
  [1]=>
  string(4) "beta"
  [2]=>
  string(5) "gamma"
}
int(0)
== exec appends to an array the caller already holds
array(3) {
  [0]=>
  string(4) "kept"
  [1]=>
  string(3) "one"
  [2]=>
  string(3) "two"
}
== trailing whitespace is stripped, leading is not
string(8) "  padded"
array(1) {
  [0]=>
  string(8) "  padded"
}
== output with no final newline
string(1) "q"
array(2) {
  [0]=>
  string(1) "p"
  [1]=>
  string(1) "q"
}
== a command that prints nothing answers "", not false
string(0) ""
array(0) {
}
int(0)
== the exit status reaches result_code
int(7)
int(3)
== a non-array output argument is replaced
array(1) {
  [0]=>
  string(1) "z"
}
== system writes every line, newline and all, and answers the last
s1
s2  
string(2) "s2"
int(0)
== system output goes through the output buffer
string(6) "b1
b2
"
string(2) "b2"
== passthru writes raw bytes and answers null
raw-no-newlineNULL
int(0)

== passthru is byte-exact: no line handling at all
string(8) "a

b   
"
== a line longer than the read buffer is still ONE line
int(2)
int(10000)
string(4) "tail"
string(4) "tail"
== NUL bytes inside a line survive
string(6) "610062"
string(6) "630064"
string(6) "630064"
== a CR is trailing whitespace like any other
array(2) {
  [0]=>
  string(3) "one"
  [1]=>
  string(3) "two"
}
string(3) "two"
== blank and whitespace-only lines are kept, and empty the answer
array(3) {
  [0]=>
  string(1) "x"
  [1]=>
  string(0) ""
  [2]=>
  string(0) ""
}
string(0) ""
