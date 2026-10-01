--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: stream_get_transports() names the transports it can open (PHL half of the twin pair)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* RECORDED SCOPE DIFFERENCE. php's list is whatever its build registered; this
 * one carries the transports it can open, in php's own REGISTRATION order --
 * tcp and udp first, then the crypto set. php lists unix:// and udg:// between
 * them, and those two are the recorded gap. This function exists so a script
 * can ASK, and naming a transport that is not there would answer that a
 * connection will work when it cannot, so the list stays honest. */
var_dump(stream_get_transports());
/* And the answer a script actually tests: */
var_dump(in_array('tcp', stream_get_transports(), true));
var_dump(in_array('udp', stream_get_transports(), true));
var_dump(in_array('ssl', stream_get_transports(), true));
var_dump(in_array('unix', stream_get_transports(), true));
?>
--EXPECT--
array(8) {
  [0]=>
  string(3) "tcp"
  [1]=>
  string(3) "udp"
  [2]=>
  string(3) "ssl"
  [3]=>
  string(3) "tls"
  [4]=>
  string(7) "tlsv1.0"
  [5]=>
  string(7) "tlsv1.1"
  [6]=>
  string(7) "tlsv1.2"
  [7]=>
  string(7) "tlsv1.3"
}
bool(true)
bool(true)
bool(true)
bool(false)
