--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
SVG: the one image whose size is text, and the only one with a unit
--SKIPIF--
<?php
if (!extension_loaded('libxml')) {
    echo "skip the SVG reader is ext/libxml's";
}
?>
--FILE--
<?php
/* SVG is php's one REGISTERED image handler rather than a built-in one, which
 * is why it sits past every signature in the detection ladder, why its
 * constant is the first past the fixed enum, and why a build without libxml
 * has neither. The reader is a pull parse that stops at the FIRST element:
 * that element must be an `svg` case-insensitively and by LOCAL name, and it
 * must carry both a `width` and a `height` matching `[0-9]+[a-zA-Z]*`. That
 * grammar is a GUARD and not a parser -- it exists so a unit cannot carry
 * markup -- so a sign, a decimal point and a percentage are all refused while
 * a plain zero is accepted. A unit that is not `px` is kept, and index 3
 * disappears with it, because php's `width="..." height="..."` string means
 * nothing outside pixels. */
function gis_svg(string $label, string $xml): void {
    $r = @getimagesizefromstring($xml);
    printf("%-24s %s\n", $label, json_encode($r));
}
function getimagesize_svg(): void {
    echo "## the root element, its name and its namespace\n";
    gis_svg('plain', '<svg width="1" height="2"/>');
    gis_svg('upper case', '<SVG width="1" height="2"/>');
    gis_svg('prefixed', '<svg:svg xmlns:svg="http://www.w3.org/2000/svg" width="5" height="6"/>');
    gis_svg('after a doctype', '<!DOCTYPE svg><svg width="3" height="4"/>');
    gis_svg('after a comment', "<?xml version=\"1.0\"?>\n<!-- <svg width='9' height='9'/> -->\n<svg width=\"3\" height=\"4\"/>");
    gis_svg('not the root', '<a><svg width="1" height="1"/></a>');
    gis_svg('another element', '<foo width="1" height="1"/>');
    gis_svg('name with a tail', '<svgx width="1" height="1"/>');
    gis_svg('declaration only', '<?xml version="1.0"?>');
    gis_svg('no markup at all', 'svg width="1" height="1"');
    gis_svg('leading space', '  <svg width="1" height="2"/>');

    echo "## both dimensions are required, and the attribute names are exact\n";
    gis_svg('no height', '<svg width="1"/>');
    gis_svg('no width', '<svg height="1"/>');
    gis_svg('upper attributes', '<svg WIDTH="1" HEIGHT="2"/>');
    gis_svg('empty', '<svg width="" height=""/>');

    echo "## the dimension grammar: digits, then letters, and nothing else\n";
    gis_svg('zero', '<svg width="0" height="1"/>');
    gis_svg('signed', '<svg width="+1" height="1"/>');
    gis_svg('fractional', '<svg width="1.5" height="1"/>');
    gis_svg('percent', '<svg width="1%" height="1"/>');
    gis_svg('unit only', '<svg width="cm" height="1"/>');
    gis_svg('non-ascii', "<svg width=\"\xc3\xa9\" height=\"1\"/>");
    gis_svg('digits after unit', '<svg width="1cm2" height="1"/>');

    echo "## a unit that is not px is kept, and index 3 goes with it\n";
    gis_svg('px stated', '<svg width="1px" height="2px"/>');
    gis_svg('one in mm', '<svg width="1" height="2mm"/>');
    gis_svg('both in cm', '<svg width="4cm" height="8cm"/>');
    gis_svg('a long unit', '<svg width="1" height="2verylongunitname"/>');

    echo "## the number is a C int, saturated then truncated like every other\n";
    gis_svg('4294967296', '<svg width="4294967296" height="1"/>');
    gis_svg('4294967297', '<svg width="4294967297" height="1"/>');
    gis_svg('twenty nines', '<svg width="99999999999999999999" height="1"/>');

    echo "## php's own case: an unterminated root still answers\n";
    gis_svg('unterminated', "<?xml version=\"1.0\" standalone=\"yes\"?>\n"
        . "<!-- foo <svg width=\"1\" height=\"1\"/> -->\n"
        . "<x:svg width='4cm' height=\"8cm\" xmlns:x=\"http://www.w3.org/2000/svg\">");
}
getimagesize_svg();
--EXPECT--
## the root element, its name and its namespace
plain                    {"0":1,"1":2,"2":21,"3":"width=\"1\" height=\"2\"","mime":"image\/svg+xml","width_unit":"px","height_unit":"px"}
upper case               {"0":1,"1":2,"2":21,"3":"width=\"1\" height=\"2\"","mime":"image\/svg+xml","width_unit":"px","height_unit":"px"}
prefixed                 {"0":5,"1":6,"2":21,"3":"width=\"5\" height=\"6\"","mime":"image\/svg+xml","width_unit":"px","height_unit":"px"}
after a doctype          {"0":3,"1":4,"2":21,"3":"width=\"3\" height=\"4\"","mime":"image\/svg+xml","width_unit":"px","height_unit":"px"}
after a comment          {"0":3,"1":4,"2":21,"3":"width=\"3\" height=\"4\"","mime":"image\/svg+xml","width_unit":"px","height_unit":"px"}
not the root             false
another element          false
name with a tail         false
declaration only         false
no markup at all         false
leading space            false
## both dimensions are required, and the attribute names are exact
no height                false
no width                 false
upper attributes         false
empty                    false
## the dimension grammar: digits, then letters, and nothing else
zero                     {"0":0,"1":1,"2":21,"3":"width=\"0\" height=\"1\"","mime":"image\/svg+xml","width_unit":"px","height_unit":"px"}
signed                   false
fractional               false
percent                  false
unit only                false
non-ascii                false
digits after unit        false
## a unit that is not px is kept, and index 3 goes with it
px stated                {"0":1,"1":2,"2":21,"3":"width=\"1\" height=\"2\"","mime":"image\/svg+xml","width_unit":"px","height_unit":"px"}
one in mm                {"0":1,"1":2,"2":21,"mime":"image\/svg+xml","width_unit":"px","height_unit":"mm"}
both in cm               {"0":4,"1":8,"2":21,"mime":"image\/svg+xml","width_unit":"cm","height_unit":"cm"}
a long unit              {"0":1,"1":2,"2":21,"mime":"image\/svg+xml","width_unit":"px","height_unit":"verylongunitname"}
## the number is a C int, saturated then truncated like every other
4294967296               {"0":0,"1":1,"2":21,"3":"width=\"0\" height=\"1\"","mime":"image\/svg+xml","width_unit":"px","height_unit":"px"}
4294967297               {"0":1,"1":1,"2":21,"3":"width=\"1\" height=\"1\"","mime":"image\/svg+xml","width_unit":"px","height_unit":"px"}
twenty nines             {"0":4294967295,"1":1,"2":21,"3":"width=\"-1\" height=\"1\"","mime":"image\/svg+xml","width_unit":"px","height_unit":"px"}
## php's own case: an unterminated root still answers
unterminated             {"0":4,"1":8,"2":21,"mime":"image\/svg+xml","width_unit":"cm","height_unit":"cm"}
