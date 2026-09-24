--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: what a session id may be — the alphabet, session_create_id(), and what session_start() refuses
--FILE--
<?php
// Nothing is printed until the end: the first byte of output marks the headers
// sent, and session_start() is refused after that.
$dir = rtrim(sys_get_temp_dir(), "/") . "/phlsessid_" . getmypid();
@mkdir($dir);
session_save_path($dir);

$log = [];
$warn = [];
set_error_handler(function ($no, $msg) use (&$warn) {
    $warn[] = $msg;
    return true;
});

// A generated id is 32 characters of php's 4-bits-per-character alphabet.
$log[] = "create:        " . preg_replace('/^[0-9a-f]{32}$/', "<32 hex>", session_create_id());
foreach ([""  => "empty", "ok-id,1" => "valid", "bad space" => "space",
          "up.per" => "dot", "sl/ash" => "slash"] as $prefix => $what) {
    $r = session_create_id($prefix);
    $log[] = "create " . str_pad($what, 8) . ": "
        . (is_string($r) ? preg_replace('/[0-9a-f]{32}$/', "<32 hex>", $r) : var_export($r, true));
}
try {
    session_create_id(str_repeat("a", 257));
} catch (Throwable $e) {
    $log[] = "create long:   " . get_class($e) . ": " . $e->getMessage();
}

// session_start() decides what the id may be. A byte that would break the header
// the id is written into is dropped WITHOUT a word and a fresh id made; any other
// character outside the alphabet is reported and starts nothing.
foreach (["ok9,-Z", "a<b", "a b", "a_b", "a.b"] as $id) {
    session_id($id);
    $r = session_start();
    $now = session_id();
    $log[] = "start " . str_pad(var_export($id, true), 8) . ": " . var_export($r, true)
        . " id=" . ($now === $id ? "<kept>" : (preg_match('/^[0-9a-f]{32}$/', $now) ? "<fresh>" : var_export($now, true)));
    if ($r) {
        session_write_close();
    }
    session_id("");
}

// session_regenerate_id() leaves the OLD id holding what the session has now (a
// request that regenerates is the one whose reply may not arrive), opens the new
// one empty, and keeps the session active over the same variables.
session_id("regen" . getmypid());
session_start();
$_SESSION["v"] = 1;
$old = $dir . "/sess_regen" . getmypid();
session_regenerate_id();
$new = $dir . "/sess_" . session_id();
$log[] = "regen: status=" . session_status()
    . " sess=" . json_encode($_SESSION)
    . " old=" . var_export(file_get_contents($old), true)
    // The new file is still LOCKED here, and on Windows a lock refuses a read,
    // so its size is what is asked: no bytes yet on every platform.
    . " new=" . var_export(filesize($new), true);
session_write_close();
$log[] = "after close: old=" . var_export(file_get_contents($old), true)
    . " new=" . var_export(file_get_contents($new), true);

// The old id is written through the SERIALIZER, so what it cannot spell is
// reported once — and not at all when the old store is being destroyed instead.
foreach ([false, true] as $del) {
    session_id("rgnum" . getmypid() . ($del ? "b" : "a"));
    session_start();
    $_SESSION[3] = "an integer key";
    $mark = count($warn);
    session_regenerate_id($del);
    $log[] = "regen delete=" . var_export($del, true) . ": "
        . json_encode(array_slice($warn, $mark));
    session_abort();
}

restore_error_handler();
echo implode("\n", $log), "\n== warnings ==\n",
    str_replace($dir, "<dir>", implode("\n", $warn)), "\n";
?>
--EXPECT--
create:        <32 hex>
create empty   : <32 hex>
create valid   : ok-id,1<32 hex>
create space   : false
create dot     : false
create slash   : false
create long:   ValueError: session_create_id(): Argument #1 ($prefix) cannot be longer than 256 characters
start 'ok9,-Z': true id=<kept>
start 'a<b'   : true id=<fresh>
start 'a b'   : true id=<fresh>
start 'a_b'   : false id=''
start 'a.b'   : false id=''
regen: status=2 sess={"v":1} old='v|i:1;' new=0
after close: old='v|i:1;' new='v|i:1;'
regen delete=false: ["session_regenerate_id(): Skipping numeric key 3"]
regen delete=true: []
== warnings ==
session_create_id(): Prefix cannot contain special characters. Only the A-Z, a-z, 0-9, "-", and "," characters are allowed
session_create_id(): Prefix cannot contain special characters. Only the A-Z, a-z, 0-9, "-", and "," characters are allowed
session_create_id(): Prefix cannot contain special characters. Only the A-Z, a-z, 0-9, "-", and "," characters are allowed
session_start(): Session ID is too long or contains illegal characters. Only the A-Z, a-z, 0-9, "-", and "," characters are allowed
session_start(): Failed to read session data: files (path: <dir>)
session_start(): Session ID is too long or contains illegal characters. Only the A-Z, a-z, 0-9, "-", and "," characters are allowed
session_start(): Failed to read session data: files (path: <dir>)
session_regenerate_id(): Skipping numeric key 3
--CLEAN--
<?php
$dir = rtrim(sys_get_temp_dir(), "/") . "/phlsessid_" . getmypid();
foreach (glob($dir . "/sess_*") as $f) { @unlink($f); }
@rmdir($dir);
