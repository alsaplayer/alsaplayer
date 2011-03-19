dnl @synopsis AP_DEFINE_CONDITIONAL
dnl
dnl Like AC_DEFINE, but allows a test expression as the section parameter.

AC_DEFUN([AP_DEFINE_CONDITIONAL],
[	$2
	test $? -ne 0
	val=$?
	AC_DEFINE_UNQUOTED($1,${val},$3)
])# AP_DEFINE_CONDITIONAL
