--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php: an IPv6 literal address binds (zend half of the twin pair)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
} elseif (@stream_socket_server('tcp://[::1]:0') === false) {
    echo "skip no IPv6 loopback on this host";
}
?>
--FILE--
<?php
/* php parses the bracketed form itself and binds a real AF_INET6 socket; the
 * name it answers keeps the brackets. PHL's half of this pair records the
 * refusal that its AF_INET-only socket layer answers instead. */
$s6 = stream_socket_server('tcp://[::1]:0', $s6_errno, $s6_errstr);
var_dump(is_resource($s6), $s6_errstr);
var_dump((bool)preg_match('/^\[::1\]:\d+$/', stream_socket_get_name($s6, false)));
fclose($s6);
?>
--EXPECT--
bool(true)
string(0) ""
bool(true)
--CLEAN--
<?php
unset($s6, $s6_errno, $s6_errstr);
