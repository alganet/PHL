--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
session.lazy_write needs a read to compare against, so a NEW store is written
--DESCRIPTION--
`session.lazy_write` is php's default: a session whose data did not change is not
written again, the store is only told it was USED. "Did not change" is a
comparison against what the READ returned, so there has to have been one — php
keeps the read's value and skips the lazy branch entirely when the store had
nothing to give.

PHL compared the two payloads and nothing else. A brand-new id whose `$_SESSION`
stayed empty has an empty payload on both sides, so it looked unchanged: the one
call that would have CREATED the store never happened. `updateTimestamp()` is
optional (it is the `SessionUpdateTimestampHandlerInterface` half), so a handler
that implements `write()` and not that one was never told about the session at
all — and on a handler that implements both, the store still never came into
existence.
--FILE--
<?php
ini_set("session.gc_probability", "0");

class SlwHandler implements SessionHandlerInterface, SessionUpdateTimestampHandlerInterface
{
    public static $log = [];
    public static $store = [];
    public function open($p, $n): bool { return true; }
    public function close(): bool { return true; }
    public function read($id): string { self::$log[] = "read($id)"; return self::$store[$id] ?? ""; }
    public function write($id, $d): bool { self::$log[] = "write($id)"; self::$store[$id] = $d; return true; }
    public function destroy($id): bool { self::$log[] = "destroy($id)"; unset(self::$store[$id]); return true; }
    public function gc($max): int { return 0; }
    public function validateId($id): bool { return isset(self::$store[$id]); }
    public function updateTimestamp($id, $d): bool { self::$log[] = "updateTimestamp($id)"; return true; }
}
session_set_save_handler(new SlwHandler(), true);

/* (1) A brand-new id whose session stays EMPTY: php creates the store. */
session_id("slwNew");
session_start();
session_write_close();

/* (2) A brand-new id with data: written, as it always was. */
session_id("slwData");
session_start();
$_SESSION["k"] = 1;
session_write_close();

/* (3) An EXISTING store read back unchanged: the lazy path, which is the point. */
session_id("slwData");
session_start();
session_write_close();

/* (4) An existing store whose data changed: written. */
session_id("slwData");
session_start();
$_SESSION["k"] = 2;
session_write_close();

echo implode("\n", SlwHandler::$log), "\n";
var_dump(array_keys(SlwHandler::$store));
echo "END\n";
?>
--EXPECT--
read(slwNew)
write(slwNew)
read(slwData)
write(slwData)
read(slwData)
updateTimestamp(slwData)
read(slwData)
write(slwData)
array(2) {
  [0]=>
  string(6) "slwNew"
  [1]=>
  string(7) "slwData"
}
END
