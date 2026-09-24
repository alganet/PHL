--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
mb_strrpos's negative $offset window is the encoding's own characters
--FILE--
<?php
/* One rule, in whatever encoding was asked for: a negative $offset is an upper
 * bound on where the match may START, counted back from the end in CHARACTERS
 * of that encoding — bytes under 8bit. php up to 8.5.9 validated the offset in
 * those characters and then applied the window in another space, refusing the
 * first three matches (this was a twin pair); 8.5.10 answers as PHL does. */
var_dump(mb_strrpos("ááá", "á", -4, "8bit"));
var_dump(mb_strrpos("áá", "áá", -4, "8bit"));
var_dump(mb_strrpos("ééééé", "éé", -6, "8bit"));
/* what both engines agree on, byte encoding included */
var_dump(mb_strrpos("aaaaaa", "aa", -6, "8bit"));
var_dump(mb_strrpos("aaaaaa", "aa", -1, "8bit"));
var_dump(mb_strrpos("ááá", "á", -2));
var_dump(mb_strrpos("ááá", "á", 0, "8bit"));
?>
--EXPECT--
int(2)
int(0)
int(4)
int(0)
int(4)
int(1)
int(4)
--CLEAN--
<?php
