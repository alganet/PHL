--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: session.serialize_handler — php / php_binary / php_serialize, session_encode()/session_decode()
--FILE--
<?php
// Nothing is printed until the very end: php marks the headers sent on the first
// byte of output, and every session ini write and session_start() after that is
// refused.
$dir = rtrim(sys_get_temp_dir(), "/") . "/phlsessser_" . getmypid();
@mkdir($dir);
session_save_path($dir);

$log = [];
$warn = [];
set_error_handler(function ($no, $msg) use (&$warn) {
    if (!(error_reporting() & $no)) {
        return true;
    }
    $warn[] = $msg;
    return true;
});

foreach (["php" => "serA", "php_binary" => "serB", "php_serialize" => "serC"] as $handler => $tag) {
    ini_set("session.serialize_handler", $handler);
    $sid = $tag . getmypid();
    session_id($sid);
    session_start();
    $_SESSION = [];
    $_SESSION["a"] = 1;
    $_SESSION["bb"] = [1, 2, ["k" => "v"]];
    $_SESSION[3] = "an integer key";
    $_SESSION["s"] = "a value with a | in it";
    $log[] = $handler . " encode: " . addcslashes(session_encode(), "\0..\37");
    session_write_close();
    $log[] = $handler . " file:   "
        . addcslashes(file_get_contents($dir . "/sess_" . $sid), "\0..\37");
    session_start();
    $log[] = $handler . " read:   " . json_encode($_SESSION);
    session_write_close();
}

// The `php` handler cannot spell a key holding its own delimiter, so php writes
// nothing at all rather than a store it could not read back.
ini_set("session.serialize_handler", "php");
session_id("serD" . getmypid());
session_start();
$_SESSION = ["ok" => 1, "a|b" => 2];
$log[] = "invalid key encode: " . var_export(session_encode(), true);
$log[] = "invalid key close:  " . var_export(session_write_close(), true);
$log[] = "invalid key file:   "
    . var_export(file_get_contents($dir . "/sess_serD" . getmypid()), true);

// session_decode() overlays what is already there for the two keyed handlers,
// and a payload php cannot decode destroys the session outright.
session_start();
$_SESSION = ["kept" => 1];
$log[] = "decode ok:      " . var_export(session_decode("added|i:2;"), true)
    . " " . json_encode($_SESSION);
$log[] = "decode garbage: " . var_export(session_decode("no delimiter here"), true)
    . " " . json_encode($_SESSION) . " status=" . session_status()
    . " id=" . var_export(session_id(), true);
$log[] = "decode closed:  " . var_export(session_decode("x|i:1;"), true);

// php walks the session variables before it decodes over them and reports the
// ones the store cannot NAME — a key the serializer would refuse for any other
// reason is not looked at here, and neither is the value.
session_id("serE" . getmypid());
session_start();
$_SESSION = [9 => "an integer key", "a|b" => 1, "obj" => new stdClass()];
$log[] = "normalize: " . var_export(session_decode("z|i:1;"), true);
session_abort();

// A NAME that came out of the store goes in raw, so a numeric-looking one stays a
// string key — one no $_SESSION[7] or $_SESSION["7"] can reach, exactly as in php.
session_id("serF" . getmypid());
session_start();
$_SESSION = [];
session_decode("7|i:1;");
$log[] = "raw key: " . json_encode(array_map("gettype", array_keys($_SESSION)))
    . " isset=" . var_export(isset($_SESSION[7]) || isset($_SESSION["7"]), true)
    . " " . serialize($_SESSION) . " " . session_encode();
session_abort();

// Its own name is validated: php looks the handler up in its registered list.
$log[] = "bogus handler: " . var_export(ini_set("session.serialize_handler", "bogus"), true)
    . " " . ini_get("session.serialize_handler");

restore_error_handler();
echo implode("\n", $log), "\n== warnings ==\n", implode("\n", $warn), "\n";
?>
--EXPECT--
php encode: a|i:1;bb|a:3:{i:0;i:1;i:1;i:2;i:2;a:1:{s:1:"k";s:1:"v";}}s|s:22:"a value with a | in it";
php file:   a|i:1;bb|a:3:{i:0;i:1;i:1;i:2;i:2;a:1:{s:1:"k";s:1:"v";}}s|s:22:"a value with a | in it";
php read:   {"a":1,"bb":[1,2,{"k":"v"}],"s":"a value with a | in it"}
php_binary encode: \001ai:1;\002bba:3:{i:0;i:1;i:1;i:2;i:2;a:1:{s:1:"k";s:1:"v";}}\001ss:22:"a value with a | in it";
php_binary file:   \001ai:1;\002bba:3:{i:0;i:1;i:1;i:2;i:2;a:1:{s:1:"k";s:1:"v";}}\001ss:22:"a value with a | in it";
php_binary read:   {"a":1,"bb":[1,2,{"k":"v"}],"s":"a value with a | in it"}
php_serialize encode: a:4:{s:1:"a";i:1;s:2:"bb";a:3:{i:0;i:1;i:1;i:2;i:2;a:1:{s:1:"k";s:1:"v";}}i:3;s:14:"an integer key";s:1:"s";s:22:"a value with a | in it";}
php_serialize file:   a:4:{s:1:"a";i:1;s:2:"bb";a:3:{i:0;i:1;i:1;i:2;i:2;a:1:{s:1:"k";s:1:"v";}}i:3;s:14:"an integer key";s:1:"s";s:22:"a value with a | in it";}
php_serialize read:   {"a":1,"bb":[1,2,{"k":"v"}],"3":"an integer key","s":"a value with a | in it"}
invalid key encode: false
invalid key close:  true
invalid key file:   ''
decode ok:      true {"kept":1,"added":2}
decode garbage: false [] status=1 id=''
decode closed:  false
normalize: true
raw key: ["string"] isset=false a:1:{s:1:"7";i:1;} 7|i:1;
bogus handler: false php
== warnings ==
session_encode(): Skipping numeric key 3
session_write_close(): Skipping numeric key 3
session_encode(): Skipping numeric key 3
session_write_close(): Skipping numeric key 3
session_encode(): Failed to write session data. Data contains invalid key "a|b"
session_write_close(): Failed to write session data. Data contains invalid key "a|b"
session_decode(): Failed to decode session object. Session has been destroyed
session_decode(): Session data cannot be decoded when there is no active session
session_decode(): Skipping numeric key 9
ini_set(): Serialization handler "bogus" cannot be found
--CLEAN--
<?php
$dir = rtrim(sys_get_temp_dir(), "/") . "/phlsessser_" . getmypid();
foreach (glob($dir . "/sess_*") as $f) { @unlink($f); }
@rmdir($dir);
