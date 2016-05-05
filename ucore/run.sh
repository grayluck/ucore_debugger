qemu-system-i386 -serial stdio -hda bin/ucore.img -drive file=bin/swap.img,media=disk,cache=writeback -drive file=bin/sfs.img,media=disk,cache=writeback  -parallel null
