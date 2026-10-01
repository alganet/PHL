--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
copy() stats both ends before it opens either, so a directory and a self-copy are refused
--DESCRIPTION--
The destination is opened for writing, so an unscreened copy() truncates its
source the moment the two paths name one file. php never reaches that open: it
stats both ends first, refuses a directory at either end with a warning, and
refuses one file under two names silently. Identity is the device and the inode,
not the spelling, so a redundant "." component is the same file.
--FILE--
<?php
$warn = [];
set_error_handler(function ($no, $msg) use (&$warn) { $warn[] = $msg; return true; });
function show($label, $result, &$warn) {
    printf("%-44s -> %-5s %s\n", $label, var_export($result, true), json_encode($warn));
    $warn = [];
}

$d = sys_get_temp_dir() . "/copyscreen" . getmypid();
@mkdir($d);
@mkdir("$d/sub");
file_put_contents("$d/a.txt", "hello world");

show("src is a directory", copy("$d/sub", "$d/from_dir.txt"), $warn);
show("  and nothing was created", file_exists("$d/from_dir.txt"), $warn);
show("dest is a directory", copy("$d/a.txt", "$d/sub"), $warn);
show("same path twice", copy("$d/a.txt", "$d/a.txt"), $warn);
show("  source intact", file_get_contents("$d/a.txt"), $warn);
show("same file, spelled differently", copy("$d/./a.txt", "$d/a.txt"), $warn);
show("  source intact", file_get_contents("$d/a.txt"), $warn);
show("two different files", copy("$d/a.txt", "$d/b.txt"), $warn);
show("  destination written", file_get_contents("$d/b.txt"), $warn);
show("destination does not exist yet", copy("$d/a.txt", "$d/fresh.txt"), $warn);
show("  destination written", file_get_contents("$d/fresh.txt"), $warn);

foreach (glob("$d/*") as $f) { @unlink($f); }
@rmdir("$d/sub");
@rmdir($d);
?>
--EXPECT--
src is a directory                           -> false ["copy(): The first argument to copy() function cannot be a directory"]
  and nothing was created                    -> false []
dest is a directory                          -> false ["copy(): The second argument to copy() function cannot be a directory"]
same path twice                              -> false []
  source intact                              -> 'hello world' []
same file, spelled differently               -> false []
  source intact                              -> 'hello world' []
two different files                          -> true  []
  destination written                        -> 'hello world' []
destination does not exist yet               -> true  []
  destination written                        -> 'hello world' []
