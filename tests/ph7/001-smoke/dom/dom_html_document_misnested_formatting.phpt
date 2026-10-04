--TEST--
A formatting element left open across an end tag is reopened after it
--FILE--
<?php
/* The adoption agency algorithm and the reconstruction that feeds it: a
 * `<b>` the source never closed in the right place is not simply dropped --
 * whatever block was caught inside it is lifted out, and a fresh copy of it
 * is reopened around everything that followed the stray end tag. */
$cases = [
	/* Nothing block-level inside: the copy is reopened around the tail. */
	'reopen'        => '<b><i>x</b>y',
	'reopen-deep'   => '<b><em><i>x</b>y',
	'reopen-mid'    => '<p>1<b>2<i>3</b>4</i>5</p>',
	'crossed'       => '<i><b>x</i>y</b>z',
	'attrs-carried' => '<font color=red><p>x</font>y</p>',
	/* A block caught inside one: the block is lifted out of it. */
	'lift-p'        => '<b><p>x</b>y</p>',
	'lift-p-text'   => '<b>1<p>2</b>3</p>',
	'lift-div'      => '<b><div><i>x</b>y</div>',
	'lift-list'     => '<b><ul><li>x</b>y</li></ul>',
	/* Two elements that refuse to nest in themselves. */
	'a-in-a'        => '<a><b><a>',
	'a-href'        => '<a href=1>x<a href=2>y',
	'nobr'          => '<nobr>a<nobr>b',
	/* The scope markers: what is open outside a cell is not reopened in it,
	 * and a stray end tag inside one closes nothing at all. */
	'cell-marker'   => '<b><table><tr><td>x</b>y</td></tr></table>',
	'object-marker' => '<b><object>x</b>y</object>',
	'select'        => '<b><select><option>x</b>y',
	/* A list item and a paragraph close the one they are a sibling of even
	 * when a formatting element sits above it. */
	'li-sibling'    => '<ul><li><b>a<li>b</ul>',
	'dt-sibling'    => '<dl><dt><i>a<dd>b',
	'p-sibling'     => '<p><b>x<p>y</b>z',
	/* Three identical entries are all the list keeps, and a document that
	 * nests properly is untouched by any of it. */
	'noahs-ark'     => '<b><b><b>x</b>y',
	'well-formed'   => '<b>x<div>y</div>z</b>',
	'closed'        => '<b><i>x</i></b>y',
	'never-opened'  => '<b><i></b></i>x',
];
foreach ($cases as $zLabel => $zSrc) {
	$oDoc = \Dom\HTMLDocument::createFromString(
		'<!DOCTYPE html><html><body>'.$zSrc, LIBXML_NOERROR);
	$zOut = $oDoc->saveHtml();
	$zOut = preg_replace('~^.*<body>~s', '', $zOut);
	$zOut = preg_replace('~</body></html>$~', '', $zOut);
	printf("%-14s %-42s => %s\n", $zLabel, $zSrc, $zOut);
}
?>
--EXPECT--
reopen         <b><i>x</b>y                               => <b><i>x</i></b><i>y</i>
reopen-deep    <b><em><i>x</b>y                           => <b><em><i>x</i></em></b><em><i>y</i></em>
reopen-mid     <p>1<b>2<i>3</b>4</i>5</p>                 => <p>1<b>2<i>3</i></b><i>4</i>5</p>
crossed        <i><b>x</i>y</b>z                          => <i><b>x</b></i><b>y</b>z
attrs-carried  <font color=red><p>x</font>y</p>           => <font color="red"></font><p><font color="red">x</font>y</p>
lift-p         <b><p>x</b>y</p>                           => <b></b><p><b>x</b>y</p>
lift-p-text    <b>1<p>2</b>3</p>                          => <b>1</b><p><b>2</b>3</p>
lift-div       <b><div><i>x</b>y</div>                    => <b></b><div><b><i>x</i></b><i>y</i></div>
lift-list      <b><ul><li>x</b>y</li></ul>                => <b></b><ul><b></b><li><b>x</b>y</li></ul>
a-in-a         <a><b><a>                                  => <a><b></b></a><b><a></a></b>
a-href         <a href=1>x<a href=2>y                     => <a href="1">x</a><a href="2">y</a>
nobr           <nobr>a<nobr>b                             => <nobr>a</nobr><nobr>b</nobr>
cell-marker    <b><table><tr><td>x</b>y</td></tr></table> => <b><table><tbody><tr><td>xy</td></tr></tbody></table></b>
object-marker  <b><object>x</b>y</object>                 => <b><object>xy</object></b>
select         <b><select><option>x</b>y                  => <b><select><option>xy</option></select></b>
li-sibling     <ul><li><b>a<li>b</ul>                     => <ul><li><b>a</b></li><li><b>b</b></li></ul>
dt-sibling     <dl><dt><i>a<dd>b                          => <dl><dt><i>a</i></dt><dd><i>b</i></dd></dl>
p-sibling      <p><b>x<p>y</b>z                           => <p><b>x</b></p><p><b>y</b>z</p>
noahs-ark      <b><b><b>x</b>y                            => <b><b><b>x</b>y</b></b>
well-formed    <b>x<div>y</div>z</b>                      => <b>x<div>y</div>z</b>
closed         <b><i>x</i></b>y                           => <b><i>x</i></b>y
never-opened   <b><i></b></i>x                            => <b><i></i></b>x
