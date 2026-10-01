--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: stream_get_transports() names tcp and udp (PHL half of the twin pair)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* RECORDED SCOPE DIFFERENCE. php's list is whatever its build registered; this
 * one carries the two transports it can open, in php's own order (ssl://,
 * tls://, unix:// and udg:// are recorded), and this function exists so
 * a script can ASK. Naming a transport that is not there would answer that a
 * connection will work when it cannot, so the honest list is the short one. */
var_dump(stream_get_transports());
/* And the answer a script actually tests: */
var_dump(in_array('tcp', stream_get_transports(), true));
var_dump(in_array('udp', stream_get_transports(), true));
var_dump(in_array('ssl', stream_get_transports(), true));
?>
--EXPECT--
array(2) {
  [0]=>
  string(3) "tcp"
  [1]=>
  string(3) "udp"
}
bool(true)
bool(true)
bool(false)
