--TEST--
Check for vapor folders
--SKIPIF--
<?php if (!extension_loaded("vapor")) print "skip"; ?>
--FILE--
<?php
$vapor = new Vapor('./', 'php');
$vapor->addFolder('demo');
$vapor->addFolder('tests');
var_dump($vapor->getFolders());
/*
	you can add regression tests for your extension here

  the output of your test code has to be equal to the
  text in the --EXPECT-- section below for the tests
  to pass, differences between the output and the
  expected text are interpreted as failure

	see php7/README.TESTING for further information on
  writing regression tests
*/
?>
--EXPECT--
array(2) {
  [0]=>
  string(22) "/mnt/github/vapor/demo"
  [1]=>
  string(23) "/mnt/github/vapor/tests"
}
