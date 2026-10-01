--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A stream diagnostic names the function the program called, not the door it went through
--FILE--
<?php
// scandir() is an internal function: php names IT, never the directory open it
// performs. This engine writes its body in terms of opendir(), and the message
// used to say `opendir(bogus://x)`.
// php raises a further `scandir(): (errno N): ...` line of its own here that
// this engine does not yet reproduce; the handler drops it so what is left is
// the question this case asks -- which function the other two lines name.
// On Windows php also raises the Win32 reason for the failed open, `(code: 123)`;
// it is counted, and the count pinned per platform.
$codes = 0;
set_error_handler(function ($n, $s) use (&$codes) {
    if (preg_match('/ \(code: 123\)$/', $s)) {
        $codes++;
    } elseif (!str_starts_with($s, 'scandir(): (errno ')) {
        echo $s, "\n";
    }
    return true;
});
var_dump(scandir('bogus://x'));

// dir() reaches the same door as a function of its own and keeps its own name.
var_dump(dir('bogus://x'));
restore_error_handler();
var_dump($codes === (PHP_OS_FAMILY === 'Windows' ? 2 : 0));
?>
--EXPECT--
scandir(): Unable to find the wrapper "bogus" - did you forget to enable it when you configured PHP?
scandir(bogus://x): Failed to open directory: No such file or directory
bool(false)
dir(): Unable to find the wrapper "bogus" - did you forget to enable it when you configured PHP?
dir(bogus://x): Failed to open directory: No such file or directory
bool(false)
bool(true)
--CLEAN--
<?php
