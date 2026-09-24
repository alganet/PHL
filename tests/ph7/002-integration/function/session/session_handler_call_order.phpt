--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The collector runs after the store is open and this session has been read
--DESCRIPTION--
php's probabilistic sweep happens at the END of session_start(): `open`, `read`,
then `gc`. PHL swept BEFORE either, so a user save handler was asked to collect a
store it had not been told to open — a handler whose `gc()` uses the connection its
own `open()` established (a database handle, a lock) had nothing to work with, and
the call sequence a handler test records read `gc,open,read` where php's reads
`open,read,gc`.
--FILE--
<?php
/* The sweep is probabilistic; ask for it on every start so the order is testable. */
ini_set("session.gc_probability", "100");
ini_set("session.gc_divisor", "100");

class ScoHandler implements SessionHandlerInterface
{
    public static $log = [];
    public static $store = [];
    public function open($path, $name): bool { self::$log[] = "open"; return true; }
    public function close(): bool { self::$log[] = "close"; return true; }
    public function read($id): string { self::$log[] = "read"; return self::$store[$id] ?? ""; }
    public function write($id, $d): bool { self::$log[] = "write"; self::$store[$id] = $d; return true; }
    public function destroy($id): bool { self::$log[] = "destroy"; return true; }
    public function gc($max): int { self::$log[] = "gc"; return 0; }
}
session_set_save_handler(new ScoHandler(), true);
session_id("scoA");
session_start();
$_SESSION["a"] = 1;
session_write_close();
echo implode(",", ScoHandler::$log), "\n";
echo "END\n";
?>
--EXPECT--
open,read,gc,write,close
END
