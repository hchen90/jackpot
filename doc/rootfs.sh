#!/usr/bin/sh

echo -e "New directory: \c" && read dir && test -d $dir || mkdir $dir

test -d $dir || exit 1

echo -e "You may need to copy your files to $dir/. Press ENTER after you finished: \c" && read

cd $dir && find | cpio -o>../rootfs.cpio && cd ..
test -f rootfs.cpio && cat rootfs.cpio | cpio -tv

# cleanup

echo -e "Cleanup $dir [y/N]: \c" && read yn && test x$yn = xy && rm -rf $dir
