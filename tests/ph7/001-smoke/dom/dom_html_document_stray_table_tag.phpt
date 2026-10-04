--TEST--
Table furniture written where no table is open is dropped rather than nested
--FILE--
<?php
/* `caption`, `col`, `colgroup`, `tbody`, `td`, `tfoot`, `th`, `thead` and `tr`
 * each belong to a table's own insertion mode.  Written where no table is open
 * at all -- the source forgot the `<table>`, or put the tag inside a `<select>`
 * or a `<p>` -- the tag is DROPPED and the parse carries on, so what follows it
 * lands in whatever was already open instead of inside an element nothing can
 * nest.  `head` and `frame` are the same refusal.
 *
 * The question is whether a table is open ANYWHERE, not whether one is the
 * current node: inside a `<td>` a stray `<td>` still opens the next cell. */
$aTags = ['caption','col','colgroup','tbody','td','tfoot','th','thead','tr',
	'head','frame'];
$aCtx = [
	/* No table is open: every one of them is dropped. */
	'body'    => 'a',
	'div'     => '<div>a',
	'p'       => '<p>a',
	'select'  => '<select><option>a',
	/* A table IS open, so each name is answered by the table instead. */
	'cell'    => '<table><tr><td>a',
	'caption' => '<table><caption>a',
];
foreach ($aCtx as $zCtx => $zOpen) {
	foreach ($aTags as $zTag) {
		$oDoc = \Dom\HTMLDocument::createFromString("$zOpen<$zTag>Z", LIBXML_NOERROR);
		printf("%-8s %-9s %s\n", $zCtx, $zTag, $oDoc->saveHtml($oDoc->body));
	}
}
--EXPECT--
body     caption   <body>aZ</body>
body     col       <body>aZ</body>
body     colgroup  <body>aZ</body>
body     tbody     <body>aZ</body>
body     td        <body>aZ</body>
body     tfoot     <body>aZ</body>
body     th        <body>aZ</body>
body     thead     <body>aZ</body>
body     tr        <body>aZ</body>
body     head      <body>aZ</body>
body     frame     <body>aZ</body>
div      caption   <body><div>aZ</div></body>
div      col       <body><div>aZ</div></body>
div      colgroup  <body><div>aZ</div></body>
div      tbody     <body><div>aZ</div></body>
div      td        <body><div>aZ</div></body>
div      tfoot     <body><div>aZ</div></body>
div      th        <body><div>aZ</div></body>
div      thead     <body><div>aZ</div></body>
div      tr        <body><div>aZ</div></body>
div      head      <body><div>aZ</div></body>
div      frame     <body><div>aZ</div></body>
p        caption   <body><p>aZ</p></body>
p        col       <body><p>aZ</p></body>
p        colgroup  <body><p>aZ</p></body>
p        tbody     <body><p>aZ</p></body>
p        td        <body><p>aZ</p></body>
p        tfoot     <body><p>aZ</p></body>
p        th        <body><p>aZ</p></body>
p        thead     <body><p>aZ</p></body>
p        tr        <body><p>aZ</p></body>
p        head      <body><p>aZ</p></body>
p        frame     <body><p>aZ</p></body>
select   caption   <body><select><option>aZ</option></select></body>
select   col       <body><select><option>aZ</option></select></body>
select   colgroup  <body><select><option>aZ</option></select></body>
select   tbody     <body><select><option>aZ</option></select></body>
select   td        <body><select><option>aZ</option></select></body>
select   tfoot     <body><select><option>aZ</option></select></body>
select   th        <body><select><option>aZ</option></select></body>
select   thead     <body><select><option>aZ</option></select></body>
select   tr        <body><select><option>aZ</option></select></body>
select   head      <body><select><option>aZ</option></select></body>
select   frame     <body><select><option>aZ</option></select></body>
cell     caption   <body><table><tbody><tr><td>a</td></tr></tbody><caption>Z</caption></table></body>
cell     col       <body>Z<table><tbody><tr><td>a</td></tr></tbody><colgroup><col></colgroup></table></body>
cell     colgroup  <body>Z<table><tbody><tr><td>a</td></tr></tbody><colgroup></colgroup></table></body>
cell     tbody     <body>Z<table><tbody><tr><td>a</td></tr></tbody><tbody></tbody></table></body>
cell     td        <body><table><tbody><tr><td>a</td><td>Z</td></tr></tbody></table></body>
cell     tfoot     <body>Z<table><tbody><tr><td>a</td></tr></tbody><tfoot></tfoot></table></body>
cell     th        <body><table><tbody><tr><td>a</td><th>Z</th></tr></tbody></table></body>
cell     thead     <body>Z<table><tbody><tr><td>a</td></tr></tbody><thead></thead></table></body>
cell     tr        <body>Z<table><tbody><tr><td>a</td></tr><tr></tr></tbody></table></body>
cell     head      <body><table><tbody><tr><td>aZ</td></tr></tbody></table></body>
cell     frame     <body><table><tbody><tr><td>aZ</td></tr></tbody></table></body>
caption  caption   <body><table><caption>a</caption><caption>Z</caption></table></body>
caption  col       <body>Z<table><caption>a</caption><colgroup><col></colgroup></table></body>
caption  colgroup  <body>Z<table><caption>a</caption><colgroup></colgroup></table></body>
caption  tbody     <body>Z<table><caption>a</caption><tbody></tbody></table></body>
caption  td        <body><table><caption>a</caption><tbody><tr><td>Z</td></tr></tbody></table></body>
caption  tfoot     <body>Z<table><caption>a</caption><tfoot></tfoot></table></body>
caption  th        <body><table><caption>a</caption><tbody><tr><th>Z</th></tr></tbody></table></body>
caption  thead     <body>Z<table><caption>a</caption><thead></thead></table></body>
caption  tr        <body>Z<table><caption>a</caption><tbody><tr></tr></tbody></table></body>
caption  head      <body><table><caption>aZ</caption></table></body>
caption  frame     <body><table><caption>aZ</caption></table></body>
