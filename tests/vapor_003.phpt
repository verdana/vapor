--TEST--
Check for vapor addFolder()
--SKIPIF--
<?php if (!extension_loaded("vapor")) {
    print "skip";
}
?>
--FILE--
<?php
// Prepare some folders
$cwd = getcwd();
$dirs = ["$cwd/www/layout", "$cwd/www/shared"];
foreach ($dirs as $dir) {
    if (is_dir($dir)) {
        continue;
    }
    mkdir($dir, 0777, true);
}

// Start vapor
$vapor = new Vapor\Engine('/tmp', 'php');
$vapor->addFolder('layout', 'www/layout');
$vapor->addFolder('shared', 'www/shared');
$folders = $vapor->getFolders();

printf("count folders: %d\n", count($folders));
printf("folder layout: %d\n", realpath("$cwd/www/layout") == $folders['layout']);
printf("folder shared: %d\n", realpath("$cwd/www/shared") == $folders['shared']);

rmdir("$cwd/www/layout");
rmdir("$cwd/www/shared");
rmdir("$cwd/www");
?>
--EXPECT--
count folders: 2
folder layout: 1
folder shared: 1
