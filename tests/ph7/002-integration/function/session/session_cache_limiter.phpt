--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: session_cache_limiter() / session_cache_expire() — read, write, and what a live session refuses
--FILE--
<?php
$log = [];
$warn = [];
set_error_handler(function ($no, $msg) use (&$warn) {
    $warn[] = preg_replace('/ \((started|sent) from .*$/', '', $msg);
    return true;
});
$t = function ($fn) use (&$log) {
    $log[] = var_export($fn(), true);
};
$t(fn() => session_cache_limiter());
$t(fn() => session_cache_limiter("public"));
$t(fn() => session_cache_limiter());
// php validates NOTHING here: an unknown limiter is stored and simply sends no
// headers, which is also how a program turns the whole thing off.
$t(fn() => session_cache_limiter("bogus"));
$t(fn() => session_cache_limiter());
$t(fn() => session_cache_limiter(""));
$t(fn() => session_cache_limiter());
$t(fn() => ini_get("session.cache_limiter"));
$t(fn() => session_cache_expire());
$t(fn() => session_cache_expire(60));
$t(fn() => session_cache_expire());
$t(fn() => session_cache_expire(-1));
$t(fn() => session_cache_expire());

$dir = rtrim(sys_get_temp_dir(), "/") . "/phlsesscl_" . getmypid();
@mkdir($dir);
session_save_path($dir);
session_id("cl" . getmypid());
session_start();
// The headers went out with the session, so there is nothing left to decide —
// and the two answer that differently.
$t(fn() => session_cache_limiter("private"));
$t(fn() => session_cache_expire(30));
session_write_close();

restore_error_handler();
echo implode("\n", $log), "\n== warnings ==\n", implode("\n", $warn), "\n";
?>
--EXPECT--
'nocache'
'nocache'
'public'
'public'
'bogus'
'bogus'
''
''
180
180
60
60
-1
false
-1
== warnings ==
session_cache_limiter(): Session cache limiter cannot be changed when a session is active
session_cache_expire(): Session cache expiration cannot be changed when a session is active
--CLEAN--
<?php
$dir = rtrim(sys_get_temp_dir(), "/") . "/phlsesscl_" . getmypid();
foreach (glob($dir . "/sess_*") as $f) { @unlink($f); }
@rmdir($dir);
