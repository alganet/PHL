--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: session_get_cookie_params() / session_set_cookie_params() and the seven session.cookie_* directives
--FILE--
<?php
// Nothing is printed until the end: the parameters may not be changed once a
// header has gone out, and the first byte of output is what sends them.
$log = [];
$warn = [];
set_error_handler(function ($no, $msg) use (&$warn) {
    // php names where the session started / the output began; PHL has no record of
    // either position yet (headers_sent()'s out-params).
    $warn[] = preg_replace('/ \((started|sent) from .*$/', '', $msg);
    return true;
});
$t = function ($fn) use (&$log) {
    try {
        $log[] = json_encode($fn());
    } catch (Throwable $e) {
        $log[] = get_class($e) . ": " . $e->getMessage();
    }
};

$log[] = json_encode(session_get_cookie_params());
$t(fn() => session_set_cookie_params(3600, "/a", "d.com", true, true));
$log[] = json_encode(session_get_cookie_params());
// The array form is the only spelling that reaches samesite and partitioned.
$t(fn() => session_set_cookie_params(["lifetime" => 7, "samesite" => "Lax", "partitioned" => true]));
$log[] = json_encode(session_get_cookie_params());
// An unrecognized key is reported and skipped; it is only an error when NONE of
// the keys were ones php knows.
$t(fn() => session_set_cookie_params(["bogus" => 1, "path" => "/z"]));
$t(fn() => session_set_cookie_params(["bogus" => 1]));
$t(fn() => session_set_cookie_params([]));
$t(fn() => session_set_cookie_params(-5));
$t(fn() => session_set_cookie_params("abc"));
$log[] = json_encode(session_get_cookie_params());
// The parameters ARE the directives — there is no second store.
$log[] = json_encode([ini_get("session.cookie_lifetime"), ini_get("session.cookie_path"),
    ini_get("session.cookie_domain"), ini_get("session.cookie_secure"),
    ini_get("session.cookie_httponly"), ini_get("session.cookie_samesite"),
    ini_get("session.cookie_partitioned")]);

$dir = rtrim(sys_get_temp_dir(), "/") . "/phlsessck_" . getmypid();
@mkdir($dir);
session_save_path($dir);
session_id("ck" . getmypid());
session_start();
$t(fn() => session_set_cookie_params(1));
session_write_close();

restore_error_handler();
echo implode("\n", $log), "\n== warnings ==\n", implode("\n", $warn), "\n";
?>
--EXPECT--
{"lifetime":0,"path":"\/","domain":"","secure":false,"partitioned":false,"httponly":false,"samesite":""}
true
{"lifetime":3600,"path":"\/a","domain":"d.com","secure":true,"partitioned":false,"httponly":true,"samesite":""}
true
{"lifetime":7,"path":"\/a","domain":"d.com","secure":true,"partitioned":true,"httponly":true,"samesite":"Lax"}
true
ValueError: session_set_cookie_params(): Argument #1 ($lifetime_or_options) must contain at least 1 valid key
ValueError: session_set_cookie_params(): Argument #1 ($lifetime_or_options) must contain at least 1 valid key
false
TypeError: session_set_cookie_params(): Argument #1 ($lifetime_or_options) must be of type array|int, string given
{"lifetime":7,"path":"\/z","domain":"d.com","secure":true,"partitioned":true,"httponly":true,"samesite":"Lax"}
["7","\/z","d.com","1","1","Lax","1"]
false
== warnings ==
session_set_cookie_params(): Argument #1 ($lifetime_or_options) contains an unrecognized key "bogus"
session_set_cookie_params(): Argument #1 ($lifetime_or_options) contains an unrecognized key "bogus"
session_set_cookie_params(): CookieLifetime cannot be negative
session_set_cookie_params(): Session cookie parameters cannot be changed when a session is active
--CLEAN--
<?php
$dir = rtrim(sys_get_temp_dir(), "/") . "/phlsessck_" . getmypid();
foreach (glob($dir . "/sess_*") as $f) { @unlink($f); }
@rmdir($dir);
