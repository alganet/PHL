--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: the ssl:// and tls:// transports, their method mask and the crypto switch
--FILE--
<?php
/* The METHOD mask is php's own numbering, not OpenSSL's: bit 0 says client and
 * one bit per protocol above it, so a method is a SET of protocols a handshake
 * may settle on. Two of them are the same number, which is not a typo -- php
 * numbers the server side without the client bit and TLS_SERVER's set happens
 * to coincide with SSLv23_SERVER's. */
foreach (['SSLv2', 'SSLv3', 'SSLv23', 'TLS', 'TLSv1_0', 'TLSv1_1', 'TLSv1_2', 'TLSv1_3', 'ANY'] as $sct_p) {
    foreach (['CLIENT', 'SERVER'] as $sct_s) {
        $sct_c = "STREAM_CRYPTO_METHOD_{$sct_p}_{$sct_s}";
        printf("%-40s %s\n", $sct_c, defined($sct_c) ? constant($sct_c) : 'UNDEFINED');
    }
}
foreach (['SSLv3', 'TLSv1_0', 'TLSv1_1', 'TLSv1_2', 'TLSv1_3'] as $sct_p) {
    $sct_c = "STREAM_CRYPTO_PROTO_{$sct_p}";
    printf("%-40s %s\n", $sct_c, defined($sct_c) ? constant($sct_c) : 'UNDEFINED');
}

/* The transports themselves. php registers one per protocol pin, so tlsv1.2://
 * is a transport of its own rather than tls:// with an option -- and asked as a
 * membership question rather than for the whole list, because the unix:// and
 * udg:// pair php also registers is a scope gap here. */
$sct_have = stream_get_transports();
foreach (['tcp', 'udp', 'ssl', 'tls', 'tlsv1.0', 'tlsv1.1', 'tlsv1.2', 'tlsv1.3'] as $sct_t) {
    printf("%-8s %s\n", $sct_t, in_array($sct_t, $sct_have, true) ? 'yes' : 'no');
}
/* ...and each is a name stream_socket_client() ACCEPTS: an address it cannot
 * reach fails on the connection, never on the transport lookup. */
foreach (['ssl', 'tls', 'tlsv1.3'] as $sct_t) {
    $sct_r = @stream_socket_client("$sct_t://127.0.0.1:1", $sct_e, $sct_es, 1);
    var_dump($sct_r);
    var_dump(str_contains($sct_es, 'socket transport'));
}

/* Crypto on a stream that has no socket under it. php warns for BOTH
 * directions and then answers them differently: false for a setup it could not
 * do, TRUE for a teardown that had nothing to undo. */
$sct_f = fopen('php://memory', 'r+');
var_dump(@stream_socket_enable_crypto($sct_f, true, STREAM_CRYPTO_METHOD_TLS_CLIENT));
var_dump(@stream_socket_enable_crypto($sct_f, false));
fclose($sct_f);

/* Enabling with no method named anywhere is a ValueError, not a false: php has
 * no default to fall back on once the context carries none. */
$sct_srv = stream_socket_server('tcp://127.0.0.1:0');
$sct_cli = stream_socket_client('tcp://' . stream_socket_get_name($sct_srv, false));
try {
    stream_socket_enable_crypto($sct_cli, true);
} catch (ValueError $sct_ex) {
    echo get_class($sct_ex), ': ', $sct_ex->getMessage(), "\n";
}
/* Disabling crypto on a SOCKET is FALSE whether or not there was a session to
 * tear down: the value that reaches the script is the one php's crypto op
 * means by "no handshake completed", so it says nothing about the handle. */
var_dump(stream_socket_enable_crypto($sct_cli, false));
fclose($sct_cli);
fclose($sct_srv);
?>
--EXPECT--
STREAM_CRYPTO_METHOD_SSLv2_CLIENT        3
STREAM_CRYPTO_METHOD_SSLv2_SERVER        2
STREAM_CRYPTO_METHOD_SSLv3_CLIENT        5
STREAM_CRYPTO_METHOD_SSLv3_SERVER        4
STREAM_CRYPTO_METHOD_SSLv23_CLIENT       57
STREAM_CRYPTO_METHOD_SSLv23_SERVER       120
STREAM_CRYPTO_METHOD_TLS_CLIENT          121
STREAM_CRYPTO_METHOD_TLS_SERVER          120
STREAM_CRYPTO_METHOD_TLSv1_0_CLIENT      9
STREAM_CRYPTO_METHOD_TLSv1_0_SERVER      8
STREAM_CRYPTO_METHOD_TLSv1_1_CLIENT      17
STREAM_CRYPTO_METHOD_TLSv1_1_SERVER      16
STREAM_CRYPTO_METHOD_TLSv1_2_CLIENT      33
STREAM_CRYPTO_METHOD_TLSv1_2_SERVER      32
STREAM_CRYPTO_METHOD_TLSv1_3_CLIENT      65
STREAM_CRYPTO_METHOD_TLSv1_3_SERVER      64
STREAM_CRYPTO_METHOD_ANY_CLIENT          127
STREAM_CRYPTO_METHOD_ANY_SERVER          126
STREAM_CRYPTO_PROTO_SSLv3                4
STREAM_CRYPTO_PROTO_TLSv1_0              8
STREAM_CRYPTO_PROTO_TLSv1_1              16
STREAM_CRYPTO_PROTO_TLSv1_2              32
STREAM_CRYPTO_PROTO_TLSv1_3              64
tcp      yes
udp      yes
ssl      yes
tls      yes
tlsv1.0  yes
tlsv1.1  yes
tlsv1.2  yes
tlsv1.3  yes
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(true)
ValueError: stream_socket_enable_crypto(): Argument #3 ($crypto_method) must be specified when enabling encryption
bool(false)
