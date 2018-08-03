--TEST--
Check for vapor render()
--SKIPIF--
<?php if (!extension_loaded("vapor")) print "skip"; ?>
--FILE--
<?php
$v1 = new Vapor('/tmp');
$fp = tmpfile();
fwrite($fp, '<h1>The quick brown fox jumped over the lazy dog.</h1>');
$fname = basename(stream_get_meta_data($fp)['uri']);
var_dump($v1->render($fname));
fclose($fp);
?>
--EXPECT--
string(54) "<h1>The quick brown fox jumped over the lazy dog.</h1>"