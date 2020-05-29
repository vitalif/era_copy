# dm-era

dm-era is a Device Mapper target that acts as a proxy to an existing block device, like dm-linear,
but also keeps track of which blocks were written to. It is included in mainline Linux kernel since 3.15.

# era_copy and era_apply

era_copy parses dm-era metadata from `era_invalidate` output and saves changed blocks into a stream
that you can save to a file or copy over network.

era_apply takes era_copy output and applies it to a file (or to a block device) to create
a mirror of the original device.

With dm-era and these two small utilities you can perform incremental backups of raw block devices
without the need for a COW FS, LVM or a storage hypervisor. dm-era almost doesn't hurt performance
and seems to handle fsyncs correctly.

# How to try dm-era for incremental backups (Debian, GRUB)

1. Setup a small partition for dm-era metadata. We'll use 1024 block (512 KB) granularity
   so a bitmap for 1 TB device will only take 8 MB on disk. dm-era keeps slightly more than one bitmap
   on disk at a time, but anyway, a 512 MB or 1 GB partition will be more than enough.
   For example you can shrink the main partition a bit and add the dm-era partition after it.
2. Zero out the new metadata partition: `dd if=/dev/zero of=<META_PARTITION> bs=1048576`
3. Copy `local-block_dm-era.sh` to `/etc/initramfs-tools/scripts/local-block`
   and adjust `DATA_DEVICE`, `META_DEVICE` and `ERA_DEVICE_NAME` in it.
   Use GPT and partition UUIDs to be safe because dm-era doesn't check
   if you supply the correct partition to it.
4. Edit `/etc/fstab` and change your actual device to `/dev/mapper/<ERA_DEVICE_NAME>`,
   for example `/dev/mapper/root_era`.
5. Repeat it for more partitions if you want.
6. If you do it for the root partition also change `/etc/default/grub`:
   `GRUB_CMDLINE_LINUX="root=/dev/mapper/root_era"` and refresh grub config with `update-grub`.
7. Reboot.
8. Install dm-era tools (`era_invalidate`) with `apt-get install thin-provisioning-tools` on your target host.
9. Install `era_copy` and `era_apply` (`make install` from this repository) on both hosts (target host and backup host).
10. Do an initial full partition backup with block-level copy.
11. Now you can use `backup.sh` to perform incremental backups of the dm-era device
    over ssh from the backup host. Just change variables at the top of the script so it matches
    your device configuration.

It's not a one-click solution, but it works :-)

# Author and license

Vitaliy Filippov

GNU GPLv3.0 or later

Of course I don't take any responsibility if you kill your data while trying to setup dm-era :-)
