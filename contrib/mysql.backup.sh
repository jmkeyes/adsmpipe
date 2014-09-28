#!/bin/bash

# Alter the following lines if your Tivoli Client directory is different.
DSMI_DIR=/opt/tivoli/tsm/client/ba/bin
DSMI_CONFIG=${DSMI_DIR}/dsm.opt
export DSMI_DIR DSMI_CONFIG

# Connect to and dump databases using the following host and credentials.
MYSQL_HOSTNAME="localhost"
MYSQL_USERNAME="root"
MYSQL_PASSWORD="password"
# The (optional) first argument to this script is the name of a database to backup.
MYSQL_DATABASE="$1";
shift

# Uncomment the following line to verify backups when executing.
#VERIFY="true"
# Uncomment the following line to enable verbose logging.
#VERBOSE="-v"

# Use a secure temporary directory for all temporary work.
ADSMPIPE_TEMPORARY=$(mktemp -d /tmp/adsmpipe-XXXXXX)
# Log to a standard location in /var/log/ instead of /tmp
ADSMPIPE_LOG_FILE="/var/log/adsmpipe.log"
# Upload all databases to the following filespace.
ADSMPIPE_FILESPACE="/adsmpipe/mysql"

BACKUP_RESULT=0

# Create a list of all databases known to the target MySQL server.
for database in $(echo 'show databases' | mysql -s -h ${MYSQL_HOSTNAME} -u ${MYSQL_USERNAME} -p${MYSQL_PASSWORD}); do
    # Skip the information_schema database as it's a virtual one that MySQL builds itself.
    [ "$database" == "information_schema" ] && continue;

    echo $database >> $ADSMPIPE_TEMPORARY/adsmpipe.$$

    # If we supplied a database on the command line, than just back that one up.
    [ -n "$MYSQL_DATABASE" -a "$MYSQL_DATABASE" != "$database" ] && continue;

    [ -n "$VERBOSE" ] && echo "BACKING up database [$database] $(date)" >> $ADSMPIPE_LOG_FILE

    DB_CMD="mysqldump -B -F --add-drop-table -h ${MYSQL_HOSTNAME} -u ${MYSQL_USERNAME} -p${MYSQL_PASSWORD} ${database}"

    if [ -n "$VERIFY" ]; then
        $DB_CMD 2>> $ADSMPIPE_LOG_FILE | tee $ADSMPIPE_TEMPORARY/$database.sql | adsmpipe -f ${database} -c -s ${ADSMPIPE_FILESPACE} ${VERBOSE:-} >> $ADSMPIPE_LOG_FILE 2>&1
    else
        $DB_CMD 2>> $ADSMPIPE_LOG_FILE | adsmpipe -f ${database} -c -s ${ADSMPIPE_FILESPACE} ${VERBOSE:-} >> $ADSMPIPE_LOG_FILE 2>&1
    fi

    RETURNCODE=$?

    if [ -n "$VERIFY" -a $RETURNCODE -eq 0 ]; then
        echo "- Verify DB checksum [$(md5sum $ADSMPIPE_TEMPORARY/$database.sql | awk '{print $1}')]" >> $ADSMPIPE_LOG_FILE 2>&1
        echo "- Verify TSM checksump [$(adsmpipe -f ${database} -x -s ${ADSMPIPE_FILESPACE} | md5sum | awk '{print $1}')]" >> $ADSMPIPE_LOG_FILE 2>&1
        rm -f $ADSMPIPE_TEMPORARY/$database.sql
    fi

    [ -n "$VERBOSE" ] && echo "= Backing COMPLETED with code [$RETURNCODE] $(date)" >> $ADSMPIPE_LOG_FILE

    # If there was an error, then report it
    [ $RETURNCODE -gt 0 ] && echo "! ERROR [${RETURNCODE}] while backing up database [${database}]" >> $ADSMPIPE_LOG_FILE && RESULT=1
done

# Now list all our backups, and see which ones we need to expire.
for database in $(adsmpipe -f \* -t -s ${ADSMPIPE_FILESPACE} 2>&1|awk '{if ($2 == "(A)") {print $5}}' | cut -c2-); do
    # If we supplied a database on the command line, than just work on that one.
    [ -n "$MYSQL_DATABASE" -a "$MYSQL_DATABASE" != "$database" ] && continue;

    [ -n "$VERBOSE" ] && echo "- Checking DB [$database]" >> $ADSMPIPE_LOG_FILE

    if [ -e $ADSMPIPE_TEMPORARY/adsmpipe.$$ ]; then
        if grep -q $database $ADSMPIPE_TEMPORARY/adsmpipe.$$; then
            [ -n "$VERBOSE" ] && echo "- DB [$database] is still current." >> $ADSMPIPE_LOG_FILE
        else
            [ -n "$VERBOSE" ] && echo "- DB [$database] is no longer current - expiring..." >> $ADSMPIPE_LOG_FILE
            adsmpipe -f $database -d -s ${ADSMPIPE_FILESPACE} ${VERBOSE:-} >> $ADSMPIPE_LOG_FILE 2>&1
        fi
    fi
done

rm -rf $ADSMPIPE_TEMPORARY

exit $BACKUP_RESULT
