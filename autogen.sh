#! /bin/sh

run_cmd() {
    echo -n "running $* ... "
    if ! $*; then
		echo "failed!"
		exit 1
		fi
    echo "ok"
}

run_cmd intltoolize --force --automake
run_cmd aclocal -I m4
run_cmd autoheader
run_cmd libtoolize --automake
run_cmd automake --add-missing
run_cmd autoconf
echo
echo "Now type './configure' to configure AlsaPlayer"
echo
