--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: an IPv6 literal address is refused loudly (PHL half of the twin pair)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* RECORDED SCOPE DIFFERENCE. net.c speaks AF_INET only, so php's bracketed
 * IPv6 form (`[::1]:port`, which php parses before it looks for a port at all)
 * names an address this build cannot open. It fails the way any unresolvable
 * host does — a warning and false — rather than silently binding something
 * else, which is what a wildcard fallback would do. */
$s6 = stream_socket_server('tcp://[::1]:0', $s6_errno, $s6_errstr);
var_dump($s6, $s6_errstr !== '');
var_dump(stream_socket_client('tcp://[::1]:9', $s6_e2, $s6_es2, 1));
?>
--EXPECTF--
%AWarning:%Astream_socket_server(): Unable to connect to tcp://[::1]:0 (php_network_getaddresses: getaddrinfo for %s failed: Name or service not known)%Abool(false)
bool(true)
%AWarning:%Astream_socket_client(): Unable to connect to tcp://[::1]:9 (php_network_getaddresses: getaddrinfo for %s failed: Name or service not known)%Abool(false)
--CLEAN--
<?php
unset($s6, $s6_errno, $s6_errstr, $s6_e2, $s6_es2);
