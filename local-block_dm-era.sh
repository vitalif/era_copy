#!/bin/sh

set -e

PREREQ=""

prereqs()
{
    echo "${PREREQ}"
}

case "${1}" in
    prereqs)
        prereqs
        exit 0
        ;;
esac

modprobe dm-era

DATA_DEVICE=/dev/disk/by-partuuid/ca313f78-7911-994b-b465-5cda9a3ceafb
META_DEVICE=/dev/disk/by-partuuid/be47b5e2-1587-1243-9c5f-f9e165df5c30
ERA_DEVICE_NAME=root_era
dmsetup create $ERA_DEVICE_NAME --table "0 `/sbin/blockdev --getsz $DATA_DEVICE` era $META_DEVICE $DATA_DEVICE 1024"
