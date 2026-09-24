--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: session_set_save_handler() — a store a program brings of its own, and SessionHandler as the one to decorate
--FILE--
<?php
$dir = rtrim(sys_get_temp_dir(), "/") . "/phlsesssh_" . getmypid();
@mkdir($dir);
$out = [];
// Every gc() below must be one this test ASKED for. php's own collector fires from
// session_start() with probability gc_probability/gc_divisor -- 1/100 by default --
// and both handlers log their calls, so a spontaneous sweep adds a log line: the
// second half's create_sid() answers "custom" . count($log), so one extra entry
// renames the session and this test failed roughly one run in fifty. Pinning the
// probability is what makes the log a record of what the SCRIPT did.
ini_set("session.gc_probability", "0");

// Six operations reach the handler: open, read, gc, write, close, destroy.
class Mem implements SessionHandlerInterface
{
    public static $store = [];
    public static $log = [];
    public function open($path, $name): bool { self::$log[] = "open($name)"; return true; }
    public function close(): bool { self::$log[] = "close"; return true; }
    public function read($id): string { self::$log[] = "read($id)"; return self::$store[$id] ?? ""; }
    public function write($id, $data): bool { self::$log[] = "write($id,$data)"; self::$store[$id] = $data; return true; }
    public function destroy($id): bool { self::$log[] = "destroy($id)"; unset(self::$store[$id]); return true; }
    public function gc($max): int { self::$log[] = "gc($max)"; return 7; }
}
$out["module0"] = session_module_name();
$out["set"] = session_set_save_handler(new Mem(), true);
$out["module1"] = session_module_name();
session_save_path($dir);
session_id("shA");
session_start();
$_SESSION["a"] = 1;
// The collector is the handler's too, and its answer is the count.
$out["gc"] = session_gc();
session_write_close();
$out["store"] = Mem::$store;
session_id("shA");
session_start();
$out["reread"] = $_SESSION;
session_destroy();
$out["after_destroy"] = Mem::$store;
$out["log"] = Mem::$log;

// SessionHandler IS the built-in files store, so a program can put itself in
// front of it and still let it do the work.
class Deco extends SessionHandler
{
    public array $seen = [];
    public function read($id): string { $this->seen[] = "read"; return parent::read($id); }
    public function write($id, $data): bool { $this->seen[] = "write"; return parent::write($id, $data); }
}
$d = new Deco();
$out["interfaces"] = [$d instanceof SessionHandlerInterface, $d instanceof SessionIdInterface];
session_set_save_handler($d, true);
session_id("shB");
session_start();
$_SESSION["q"] = 42;
session_write_close();
$out["decorated"] = $d->seen;
$out["file"] = @file_get_contents($dir . "/sess_shB");
$out["module2"] = session_module_name();
// Naming the built-in module again drops the userland handler.
$out["back"] = session_module_name("files");
$out["module3"] = session_module_name();

// The three optional operations, each behind its own interface. create_sid()
// names the session (a store that knows how to key itself chooses the key);
// updateTimestamp() replaces write() when the data did not change, which is
// php's session.lazy_write default; validateId() is what session.use_strict_mode
// asks before it will adopt an id a REQUEST supplied.
class Full implements SessionHandlerInterface, SessionIdInterface, SessionUpdateTimestampHandlerInterface
{
    public static $store = [];
    public static $log = [];
    public function open($p, $n): bool { self::$log[] = "open"; return true; }
    public function close(): bool { self::$log[] = "close"; return true; }
    public function read($id): string { self::$log[] = "read:$id"; return self::$store[$id] ?? ""; }
    public function write($id, $d): bool { self::$log[] = "write:$id"; self::$store[$id] = $d; return true; }
    public function destroy($id): bool { self::$log[] = "destroy"; return true; }
    public function gc($m): int { self::$log[] = "gc"; return 0; }
    public function create_sid(): string { self::$log[] = "create_sid"; return "custom" . count(self::$log); }
    public function validateId($id): bool { self::$log[] = "validateId:$id"; return isset(self::$store[$id]); }
    public function updateTimestamp($id, $d): bool { self::$log[] = "updateTimestamp:$id"; return true; }
}
session_set_save_handler(new Full(), true);
session_start();
$out["created_id"] = session_id();
$_SESSION["a"] = 1;
session_write_close();
// Same id, same data: the store is told the session was USED, not written again.
session_id($out["created_id"]);
session_start();
session_write_close();
// An id no store has seen, with strict mode on: php will not adopt it.
ini_set("session.use_strict_mode", "1");
session_id("neverseen1");
session_start();
$out["strict"] = session_id() === "neverseen1" ? "kept" : "fresh";
session_abort();
$out["full_log"] = Full::$log;
echo json_encode($out, JSON_PRETTY_PRINT), "\n";
?>
--EXPECT--
{
    "module0": "files",
    "set": true,
    "module1": "user",
    "gc": 7,
    "store": {
        "shA": "a|i:1;"
    },
    "reread": {
        "a": 1
    },
    "after_destroy": [],
    "log": [
        "open(PHPSESSID)",
        "read(shA)",
        "gc(1440)",
        "write(shA,a|i:1;)",
        "close",
        "open(PHPSESSID)",
        "read(shA)",
        "destroy(shA)",
        "close"
    ],
    "interfaces": [
        true,
        true
    ],
    "decorated": [
        "read",
        "write"
    ],
    "file": "q|i:42;",
    "module2": "user",
    "back": "user",
    "module3": "files",
    "created_id": "shB",
    "strict": "fresh",
    "full_log": [
        "open",
        "read:shB",
        "write:shB",
        "close",
        "open",
        "read:shB",
        "updateTimestamp:shB",
        "close",
        "open",
        "validateId:neverseen1",
        "create_sid",
        "read:custom11",
        "close"
    ]
}
--CLEAN--
<?php
$dir = rtrim(sys_get_temp_dir(), "/") . "/phlsesssh_" . getmypid();
foreach (glob($dir . "/sess_*") as $f) { @unlink($f); }
@rmdir($dir);
