--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: session_start()'s $options array — every key is a session.<key> directive for this request
--FILE--
<?php
$dir = rtrim(sys_get_temp_dir(), "/") . "/phlsessopt_" . getmypid();
@mkdir($dir);
$log = [];
$warn = [];
set_error_handler(function ($no, $msg) use (&$warn) {
    $warn[] = preg_replace('/ \((started|sent) from .*$/', '', $msg);
    return true;
});
$try = function ($opts) use (&$log, $dir) {
    $opts["save_path"] = $dir;
    try {
        $r = session_start($opts);
        $log[] = var_export($r, true) . " status=" . session_status()
            . " name=" . session_name()
            . " gc=" . var_export(ini_get("session.gc_maxlifetime"), true)
            . " cl=" . var_export(ini_get("session.cookie_lifetime"), true);
        if (session_status() === PHP_SESSION_ACTIVE) {
            session_write_close();
        }
    } catch (Throwable $e) {
        $log[] = get_class($e) . ": " . $e->getMessage();
    }
    // Put the two directives the cases move back where they were.
    ini_set("session.name", "PHPSESSID");
    ini_set("session.gc_maxlifetime", "1440");
    ini_set("session.cookie_lifetime", "0");
};

$try(["name" => "MYSID", "gc_maxlifetime" => 77, "cookie_lifetime" => 99]);
// A key the directive table refuses is reported, and the session still starts.
$try(["bogus" => 1]);
// `read_and_close` is the one key that is not a directive: the store is read and
// released again before the script gets to run.
$try(["read_and_close" => 1]);
// session.name goes out as a COOKIE name, so php holds it to the cookie alphabet
// and refuses a numeric or empty one.
$try(["name" => ""]);
$try(["name" => "123"]);
$try(["name" => "a=b"]);
// A key that is not a string, or a value that is not a scalar, is a hard error
// before anything is opened.
$try(["name" => [1]]);
$try([5 => 1]);
$try([]);

restore_error_handler();
echo implode("\n", $log), "\n== warnings ==\n", implode("\n", $warn), "\n";
?>
--EXPECT--
true status=2 name=MYSID gc='77' cl='99'
true status=2 name=PHPSESSID gc='1440' cl='0'
true status=1 name=PHPSESSID gc='1440' cl='0'
true status=2 name=PHPSESSID gc='1440' cl='0'
true status=2 name=PHPSESSID gc='1440' cl='0'
true status=2 name=PHPSESSID gc='1440' cl='0'
TypeError: session_start(): Option "name" must be of type string|int|bool, array given
ValueError: session_start(): Argument #1 ($options) must be of type array with keys as string
true status=2 name=PHPSESSID gc='1440' cl='0'
== warnings ==
session_start(): Setting option "bogus" failed
session_start(): session.name "" must not be numeric, empty, contain null bytes or any of the following characters "=,;.[ \t\r\n\013\014"
session_start(): Setting option "name" failed
session_start(): session.name "123" must not be numeric, empty, contain null bytes or any of the following characters "=,;.[ \t\r\n\013\014"
session_start(): Setting option "name" failed
session_start(): session.name "a=b" must not be numeric, empty, contain null bytes or any of the following characters "=,;.[ \t\r\n\013\014"
session_start(): Setting option "name" failed
--CLEAN--
<?php
$dir = rtrim(sys_get_temp_dir(), "/") . "/phlsessopt_" . getmypid();
foreach (glob($dir . "/sess_*") as $f) { @unlink($f); }
@rmdir($dir);
