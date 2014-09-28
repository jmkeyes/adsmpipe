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

# Log to a standard location in /var/log/ instead of /tmp
ADSMPIPE_LOG_FILE="/var/log/adsmpipe.log"
# Upload all databases to the following filespace.
ADSMPIPE_FILESPACE="/adsmpipe/mysql"

RESTORE_RESULT=0

# List all databases we've backed up and only show the archived version's name.
for database in $(adsmpipe -t -f /\* -s $ADSMPIPE_FILESPACE 2>&1 | awk '{if ($2 == "(A)") {print $5}}' | cut -c2-); do
    # If we supplied a database on the command line, than just restore that one.
    [ -n "$MYSQL_DATABASE" -a "$MYSQL_DATABASE" != "$database" ] && continue;

    [ -n "$VERBOSE" ] && echo "RESTORING database [$database] $(date)" >> $ADSMPIPE_LOG_FILE

    DB_CMD="mysql -h ${MYSQL_HOSTNAME} -u ${MYSQL_USERNAME} -p${MYSQL_PASSWORD} ${database}"

    adsmpipe -f ${database} -x -s ${ADSMPIPE_FILESPACE} ${VERBOSE:-} 2>> $ADSMPIPE_LOG_FILE | $DB_CMD 2>> $ADSMPIPE_LOG_FILE 2>&1

    RETURNCODE=$?

    [ -n "$VERBOSE" ] && echo "= Restore COMPLETED with code [$RETURNCODE] $(date)" >> $ADSMPIPE_LOG_FILE

    # If there was an error, then report it
    [ $RETURNCODE -gt 0 ] && echo "! ERROR [${RETURNCODE}] while restoring database [${database}]" >> $ADSMPIPE_LOG_FILE && RESTORE_RESULT=1
done

exit $RESTORE_RESULT
