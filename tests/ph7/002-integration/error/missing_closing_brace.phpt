--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
an unterminated ${ is a parse error (PHL reports end of file where php names the ";" -- see the SKIPIF)
--SKIPIF--
<?php
// An unterminated `${` runs the dynamic-name scan off the end of the statement
// slice, and at that point the token php names (the ';') is no longer reachable
// from the generator state, so PHL reports "unexpected end of file" instead. The
// sibling forms -- $( , $1 , ${} -- all name php's token exactly (see the tests
// beside this one); only the run-off-the-end case is left. A recorded residual.
if (function_exists('zend_version')) { echo 'skip unterminated ${ reports end of file; php names the terminator'; }
?>
--FILE--
<?php
$var = ${unclosed;
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected end of file%A
--CLEAN--
<?php
unset($var);
