# dm-era

dm-era is a Device Mapper target that acts as a proxy to an existing block device, like dm-linear,
but also keeps track of which blocks were written to. It is included in mainline Linux kernel since 3.15.

Update: dm-era seems to be unsafe in kernel versions before 5.12 because of
[https://www.spinics.net/lists/dm-devel/msg45023.html](two bugs) which were only fixed in 5.12.

# era_copy and era_apply

era_copy parses dm-era metadata from `era_invalidate` output and saves changed blocks into a stream
that you can save to a file or copy over network.

era_apply takes era_copy output and applies it to a file (or to a block device) to create
a mirror of the original device.

With dm-era and these two small utilities you can perform incremental backups of raw block devices
without the need for a COW FS, LVM or a storage hypervisor. dm-era almost doesn't hurt performance
and seems to handle fsyncs correctly.

It's not a one-click solution, but it works :-)

# How to try dm-era for incremental backups

1. Install dm-era tools (`era_invalidate`) with `apt-get install thin-provisioning-tools` on your target host.
2. Install `era_copy` and `era_apply` (`make install` from this repository) on both hosts (target host and backup host).
3. Setup a small partition for dm-era metadata. We'll use 1024 block (512 KB) granularity
   so a bitmap for 1 TB device will only take 8 MB on disk. dm-era keeps slightly more than one bitmap
   on disk at a time, but anyway, a 512 MB or 1 GB partition will be more than enough.
   For example you can shrink the main partition a bit and add the dm-era partition after it.
4. Zero out the new metadata partition: `dd if=/dev/zero of=<META_PARTITION> bs=1048576`
5. Copy `dm-era.service` to `/etc/systemd/system` and edit the ExecStart command.
   The syntax is: `/sbin/dmsetup create ERA_DEVICE_NAME --table "0 SIZE_IN_SECTORS era METADATA_DEVICE DATA_DEVICE 1024"`.
6. Enable the unit: `systemctl enable dm-era`.
7. Change the desired /etc/fstab entry and reboot (or unmount the partition, stop services, mount it back and start services).
8. Do an initial full partition backup with block-level copy. For example, to copy an ext4 filesystem
   to another host over ssh run: `ssh root@host "e2image -f -p -ra /dev/mapper/root_era - | gzip" | gzip -d | cp --sparse=always /dev/stdin rootfs.bin`.
9. Now you can use `backup.sh` to perform incremental backups of the dm-era device
   over ssh from the backup host. Just change variables at the top of the script so it matches
   your device configuration.

# dm-era on root partition (Debian, GRUB)

1. Copy `zz_dm-era.sh` to `/etc/initramfs-tools/scripts/local-block`
   and adjust `DATA_DEVICE`, `META_DEVICE` and `ERA_DEVICE_NAME` in it.
   Use partition IDs (`/dev/disk/by-partuuid/*` for GPT partitions, `/dev/disk/by-id/md-uuid-*` for mdadm, etc)
   to be safe because dm-era doesn't check if you supply correct partitions to it.
2. Add `dm_era` to `/etc/initramfs-tools/modules`.
3. Edit `/etc/fstab` and change your actual device to `/dev/mapper/<ERA_DEVICE_NAME>`,
   for example `/dev/mapper/root_era`.
4. Run `update-initramfs -u -k all`.
5. Change `/etc/default/grub`: set
   `GRUB_CMDLINE_LINUX="root=/dev/mapper/root_era"` and refresh grub config with `update-grub`.
6. Reboot.

# Author and license

Vitaliy Filippov

GNU GPLv3.0 or later

Of course I don't take any responsibility if you kill your data while trying to setup dm-era :-)
