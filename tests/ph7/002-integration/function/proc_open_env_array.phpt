--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
proc_open(): an empty env value unsets the name, and a keyless entry is already NAME=VALUE
--SKIPIF--
<?php if (PHP_OS == 'WINNT') { echo "skip POSIX-only behavior; not applicable on Windows"; } ?>
--FILE--
<?php
/* php builds the child's envp from the array with two rules this engine did not
 * have. An entry whose stringified VALUE is empty is DROPPED, not exported as
 * `NAME=` -- passing '' is how a caller unsets a name, and the child's getenv()
 * has to answer false for it. And only a non-empty STRING key names a variable:
 * an integer-keyed (or ''-keyed) entry is read as a ready-made `NAME=VALUE`
 * string and exported verbatim, so a plain list is a valid environment. This
 * engine prefixed the synthesised key instead, making `PATH_X=/bin` arrive as
 * a variable literally named `0`. */
$poeNames = ['POE_A', 'POE_B', 'POE_C', 'POE_D', 'POE_X', 'POE_Y', '0'];
$poeChild = 'foreach ([' . implode(',', array_map(fn($n) => var_export($n, true), $poeNames))
          . '] as $n) { printf("  %-6s %s\n", $n, var_export(getenv($n), true)); }';

function poe_run(array $poeEnv, string $poeLabel) {
    global $poeChild;
    echo $poeLabel, "\n";
    $poeProc = proc_open([PHP_BINARY, '-r', $poeChild],
                         [1 => ['pipe', 'w'], 2 => ['pipe', 'w']], $poePipes, null, $poeEnv);
    if (!is_resource($poeProc)) { echo "  NO-PROC\n"; return; }
    echo stream_get_contents($poePipes[1]);
    $poeErr = stream_get_contents($poePipes[2]);
    fclose($poePipes[1]); fclose($poePipes[2]);
    proc_close($poeProc);
    if ($poeErr !== '') { echo "  STDERR: ", $poeErr; }
}

// An empty value is an UNSET, not an empty export. '0' is a real value and stays.
poe_run(['POE_A' => 'a', 'POE_B' => '', 'POE_C' => '0', 'POE_D' => ''], 'empty-value');
// A list array carries its own NAME=VALUE strings.
poe_run(['POE_X=x-val', 'POE_Y=y-val'], 'list-array');
// Mixed: the named entry is prefixed, the keyless one is not.
poe_run(['POE_A' => 'a', 'POE_X=x-val'], 'mixed');
// An empty STRING key is keyless too -- php's test is `key && ZSTR_LEN(key)`.
poe_run(['' => 'POE_Y=y-val'], 'empty-key');
// An empty array is an empty environment, not an inherited one.
poe_run([], 'empty-array');
--EXPECT--
empty-value
  POE_A  'a'
  POE_B  false
  POE_C  '0'
  POE_D  false
  POE_X  false
  POE_Y  false
  0      false
list-array
  POE_A  false
  POE_B  false
  POE_C  false
  POE_D  false
  POE_X  'x-val'
  POE_Y  'y-val'
  0      false
mixed
  POE_A  'a'
  POE_B  false
  POE_C  false
  POE_D  false
  POE_X  'x-val'
  POE_Y  false
  0      false
empty-key
  POE_A  false
  POE_B  false
  POE_C  false
  POE_D  false
  POE_X  false
  POE_Y  'y-val'
  0      false
empty-array
  POE_A  false
  POE_B  false
  POE_C  false
  POE_D  false
  POE_X  false
  POE_Y  false
  0      false
