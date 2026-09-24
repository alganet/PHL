--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: stream_get_transports() names only tcp (PHL half of the twin pair)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* RECORDED SCOPE DIFFERENCE. php's list is whatever its build registered;
 * PHL's socket layer is tcp-only (ssl://, tls://, udp://, unix:// and udg://
 * are §7.4 slice-2 (a)), and this function exists so a script can ASK. Naming
 * a transport that is not there would answer that a connection will work when
 * it cannot, so the honest list is the short one. */
var_dump(stream_get_transports());
/* And the answer a script actually tests: */
var_dump(in_array('tcp', stream_get_transports(), true));
var_dump(in_array('ssl', stream_get_transports(), true));
?>
--EXPECT--
array(1) {
  [0]=>
  string(3) "tcp"
}
bool(true)
bool(false)
