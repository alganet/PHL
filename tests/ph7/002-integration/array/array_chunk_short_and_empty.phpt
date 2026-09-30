--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_chunk(): an empty input has no chunks, and a short one is still reindexed
--DESCRIPTION--
array_chunk() had a shortcut for "the whole array fits in one chunk" that answered the
INPUT unchanged, which is two wrong answers at once. An EMPTY array came back as ONE
EMPTY CHUNK where php answers no chunks at all, and a short array kept its string keys
where php reindexes every chunk unless $preserve_keys says otherwise.

The empty case is not a corner: twig's ArrayExpression chunks its node list into
key/value pairs, and that list is empty for a template as small as `{{ foo.bar }}`. It
then read `$pair[0]` out of the phantom chunk, compiled the resulting null into the
template, and the render looped forever. The general loop underneath was already right
about both, so the shortcut is simply gone.
--FILE--
<?php
function d(string $label, array $a, int $n, bool $p = false): void {
    echo str_pad($label, 30), json_encode(array_chunk($a, $n, $p)), "\n";
}
echo "== an EMPTY input has no chunks at all ==\n";
d('empty, size 2', [], 2);
d('empty, size 2, preserved', [], 2, true);
d('empty, size 1', [], 1);

echo "== a short one is still REINDEXED unless asked otherwise ==\n";
d('two string keys, size 5', ['a' => 1, 'b' => 2], 5);
d('same, preserved', ['a' => 1, 'b' => 2], 5, true);
d('one entry at key 5, size 3', [5 => 'x'], 3);
d('same, preserved', [5 => 'x'], 3, true);
d('exact fit', [1, 2, 3], 3);
d('exact fit, preserved', [1, 2, 3], 3, true);
d('size past the end', [1, 2, 3], 10);
d('size past the end, preserved', [1, 2, 3], 10, true);

echo "== and the ordinary splits are unchanged ==\n";
d('five by two', [1, 2, 3, 4, 5], 2);
d('five by two, preserved', [1, 2, 3, 4, 5], 2, true);
d('five by three', [1, 2, 3, 4, 5], 3);
d('holes', [0 => 'a', 2 => 'b', 5 => 'c'], 2);
d('holes, preserved', [0 => 'a', 2 => 'b', 5 => 'c'], 2, true);
d('mixed keys', [0 => 'x', 'k' => 'y', 1 => 'z'], 2);
d('mixed keys, preserved', [0 => 'x', 'k' => 'y', 1 => 'z'], 2, true);

echo "== the refusals ==\n";
try { array_chunk([1], 0); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { array_chunk([1], -1); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
echo "done\n";
?>
--EXPECT--
== an EMPTY input has no chunks at all ==
empty, size 2                 []
empty, size 2, preserved      []
empty, size 1                 []
== a short one is still REINDEXED unless asked otherwise ==
two string keys, size 5       [[1,2]]
same, preserved               [{"a":1,"b":2}]
one entry at key 5, size 3    [["x"]]
same, preserved               [{"5":"x"}]
exact fit                     [[1,2,3]]
exact fit, preserved          [[1,2,3]]
size past the end             [[1,2,3]]
size past the end, preserved  [[1,2,3]]
== and the ordinary splits are unchanged ==
five by two                   [[1,2],[3,4],[5]]
five by two, preserved        [[1,2],{"2":3,"3":4},{"4":5}]
five by three                 [[1,2,3],[4,5]]
holes                         [["a","b"],["c"]]
holes, preserved              [{"0":"a","2":"b"},{"5":"c"}]
mixed keys                    [["x","y"],["z"]]
mixed keys, preserved         [{"0":"x","k":"y"},{"1":"z"}]
== the refusals ==
ValueError: array_chunk(): Argument #2 ($length) must be greater than 0
ValueError: array_chunk(): Argument #2 ($length) must be greater than 0
done
