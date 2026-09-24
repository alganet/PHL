--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An option a builtin READS is not rewritten in the caller's array (filter_var, password_hash, stream filter params, proc_open)
--SKIPIF--
<?php if (PHP_OS == 'WINNT') { echo "skip the proc_open half is POSIX-only"; } ?>
--FILE--
<?php
/* Every ph7_value_to_xxx() is destructive — it converts the value it is handed
 * and throws the prior representation away. Handing one an entry FETCHED from
 * an array the SCRIPT still holds rewrote that array: an $options member, a
 * stream-filter parameter and a proc_open descriptor all came back a different
 * type than they went in. php's zval_get_long()/zval_get_string() never touch
 * the caller's zval, so nothing below changes on either engine. */
function opt_show($label, $a)
{
    echo str_pad($label, 22), ' ', str_replace(["\n", '  '], '', var_export($a, true)), "\n";
}

// filter_var(): the range options and the flags entry
$fv = ['min_range' => '3', 'max_range' => true];
var_dump(filter_var('5', FILTER_VALIDATE_INT, ['options' => $fv]));
opt_show('filter_var range', $fv);

$fvo = ['options' => ['min_range' => 3], 'flags' => '0'];
var_dump(filter_var('5', FILTER_VALIDATE_INT, $fvo));
opt_show('filter_var flags', $fvo);

// password_hash()/password_needs_rehash(): the cost option
$pw = ['cost' => '5'];
$h = password_hash('secret', PASSWORD_BCRYPT, $pw);
opt_show('password cost', $pw);
echo 'verify: ', var_export(password_verify('secret', $h), true), "\n";
$pw2 = ['cost' => '5'];
var_dump(password_needs_rehash($h, PASSWORD_BCRYPT, $pw2));
opt_show('needs_rehash cost', $pw2);

// stream_filter_append(): the convert.* filter's own parameters
$fp = fopen('php://memory', 'w+');
$params = ['line-length' => '8', 'binary' => 1, 'line-break-chars' => 10];
stream_filter_append($fp, 'convert.base64-encode', STREAM_FILTER_WRITE, $params);
fwrite($fp, 'the quick brown fox');
rewind($fp);
var_dump(stream_get_contents($fp));
fclose($fp);
opt_show('filter params', $params);

// proc_open(): the descriptor spec
$spec = [0 => ['pipe', 114], 1 => ['pipe', 'w'], 2 => ['file', '/dev/null', 'w']];
$pipes = [];
$p = proc_open([PHP_BINARY, '-r', 'echo "child";'], $spec, $pipes);
if (is_resource($p)) {
    fclose($pipes[0]);
    echo stream_get_contents($pipes[1]), "\n";
    fclose($pipes[1]);
    proc_close($p);
}
opt_show('proc_open spec', $spec);
?>
--EXPECT--
bool(false)
filter_var range       array ('min_range' => '3','max_range' => true,)
int(5)
filter_var flags       array ('options' => array ('min_range' => 3,),'flags' => '0',)
password cost          array ('cost' => '5',)
verify: true
bool(false)
needs_rehash cost      array ('cost' => '5',)
string(28) "dGhlIHF110aWNrIGJy10b3duIGZv"
filter params          array ('line-length' => '8','binary' => 1,'line-break-chars' => 10,)
child
proc_open spec         array (0 => array (0 => 'pipe',1 => 114,),1 => array (0 => 'pipe',1 => 'w',),2 => array (0 => 'file',1 => '/dev/null',2 => 'w',),)
--CLEAN--
<?php
unset($fv, $fvo, $pw, $pw2, $h, $fp, $params, $spec, $pipes, $p);
