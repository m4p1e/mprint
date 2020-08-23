PHP_ARG_ENABLE(mprint, whether to enable mprint support,
dnl Make sure that the comment is aligned:
[  --enable-mprint           Enable mprint support])

 PHP_NEW_EXTENSION(mprint, mprint.c, $ext_shared)