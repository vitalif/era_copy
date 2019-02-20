#!/bin/bash
# Example incremental backup script

HOST=your-host.ru
ERA_DEVICE=md1p3_era
METADATA_DEVICE=/dev/md1p4
GRANULARITY=1024
MOUNTPOINT=/var
FREEZE_LIMIT=1073741824
DIR=/media/backup
FILE=var.bin

[ -f "$DIR/era" ] || exit 1

for i in {1..5}; do

    ERA=`cat $DIR/era`

    SIZE=`ssh root@$HOST "dmsetup message $ERA_DEVICE 0 drop_metadata_snap >/dev/null; \
        dmsetup message $ERA_DEVICE 0 take_metadata_snap >/dev/null && \
        (era_invalidate --metadata-snapshot --written-since $ERA $METADATA_DEVICE | era_copy $GRANULARITY | perl -pe 's/\s+.*//')"`

    if [ $SIZE -le 0 ]; then
        echo "Failed to estimate delta size"
        exit 1
    fi

    echo "Estimated delta size $((SIZE/1048576)) MB"

    if [ $SIZE -le $FREEZE_LIMIT ]; then

        echo "Performing final copy with fsfreeze"

        (ssh root@$HOST "dmsetup message $ERA_DEVICE 0 drop_metadata_snap >/dev/null; \
            trap 'fsfreeze --unfreeze $MOUNTPOINT >/dev/null; exit 1' 1 2 3 15
            fsfreeze --freeze $MOUNTPOINT >/dev/null && \
            dmsetup message $ERA_DEVICE 0 take_metadata_snap >/dev/null && \
            (era_invalidate --metadata-snapshot --written-since $ERA $METADATA_DEVICE | era_copy --progress $GRANULARITY /dev/mapper/$ERA_DEVICE | gzip); \
            fsfreeze --unfreeze $MOUNTPOINT >/dev/null" | \
            gzip -d | era_apply $DIR/$FILE) && \
            (ssh root@$HOST "dmsetup status $ERA_DEVICE | perl -e 'print [split /\s+/, <>]->[5]'" > $DIR/era) || exit 1

        exit 0

    else

        echo "Performing intermediate copy without fsfreeze"

        (ssh root@$HOST "dmsetup message $ERA_DEVICE 0 drop_metadata_snap >/dev/null; \
            dmsetup message $ERA_DEVICE 0 take_metadata_snap >/dev/null && \
            (era_invalidate --metadata-snapshot --written-since $ERA $METADATA_DEVICE | era_copy --progress $GRANULARITY /dev/mapper/$ERA_DEVICE | gzip)" | \
            gzip -d | era_apply $DIR/$FILE) && \
            (ssh root@$HOST "dmsetup status $ERA_DEVICE | perl -e 'print [split /\s+/, <>]->[5]'" > $DIR/era) || exit 1

    fi
done

echo "Failed to perform final copy"

exit 1
