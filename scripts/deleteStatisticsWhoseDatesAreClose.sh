
debugFile=/tmp/deleteStatisticsWhoseDatesAreClose.log

if [ $# -ne 4 ];
then
	echo "Usage $0 <workspaceKey> <secondsDatesTooClose> <startDate (2022-09-01 00:00:00)> <endDate (2022-09-02 00:00:00)>" >> $debugFile

	exit 1
fi

filesize=$(stat -c %s $debugFile)
if [ $filesize -gt 1000000 ]
then
	echo "" > $debugFile
fi

cibortvWorkspaceKey=$1
datesTooCloseInSeconds=$2
startDate=$3
endDate=$4

#10 minutes
periodInSeconds=600

rowsBeforeCommand=$(echo "select count(*) from MMS_RequestStatistic where workspaceKey = $cibortvWorkspaceKey and requestTimestamp >= '$startDate' and requestTimestamp < '$endDate';" | mysql -N -u mms -pF_-A*kED-34-r*U -h db-server-active mms 2> /dev/null)

startInSeconds=$(date +%s -d "$startDate")
endInSeconds=$(date +%s -d "$endDate")

currentInSeconds=$startInSeconds

startCommand=$(date +%s)
while [ $currentInSeconds -lt $endInSeconds ]; do
        startFormatted=$(date -d @$currentInSeconds +"%Y-%m-%d %H:%M:%S")

        currentInSecondsForNestedSelect=$((currentInSeconds-datesTooCloseInSeconds))
        startFormattedForNestedSelect=$(date -d @$currentInSecondsForNestedSelect +"%Y-%m-%d %H:%M:%S")

        currentInSeconds=$((currentInSeconds+periodInSeconds))

        endFormatted=$(date -d @$currentInSeconds +"%Y-%m-%d %H:%M:%S")

		#sqlCommand="CREATE TEMPORARY TABLE t SELECT A.requestStatisticKey FROM MMS_RequestStatistic AS A WHERE A.workspaceKey = $cibortvWorkspaceKey AND A.requestTimestamp between '$startFormatted' AND '$endFormatted' AND EXISTS (SELECT B.requestStatisticKey FROM MMS_RequestStatistic AS B WHERE B.workspaceKey = $cibortvWorkspaceKey and A.userId = B.userId AND B.requestTimestamp between '$startFormattedForNestedSelect' AND '$endFormatted' AND B.requestTimestamp <= DATE_ADD(A.requestTimestamp, INTERVAL $datesTooCloseInSeconds SECOND) AND B.requestTimestamp > A.requestTimestamp); SELECT COUNT(*) from t; drop temporary table t;"
		sqlCommand="CREATE TEMPORARY TABLE t SELECT A.requestStatisticKey FROM MMS_RequestStatistic AS A WHERE A.workspaceKey = $cibortvWorkspaceKey AND A.requestTimestamp between '$startFormatted' AND '$endFormatted' AND EXISTS (SELECT B.requestStatisticKey FROM MMS_RequestStatistic AS B WHERE B.workspaceKey = $cibortvWorkspaceKey and A.userId = B.userId AND B.requestTimestamp between '$startFormattedForNestedSelect' AND '$endFormatted' AND B.requestTimestamp <= DATE_ADD(A.requestTimestamp, INTERVAL $datesTooCloseInSeconds SECOND) AND B.requestTimestamp > A.requestTimestamp); SELECT COUNT(*) from t; delete from MMS_RequestStatistic where requestStatisticKey in (select requestStatisticKey from t); drop temporary table t;"

		#echo $sqlCommand >> $debugFile

        startSqlCommand=$(date +%s)
        echo $sqlCommand | mysql -N -u mms -pF_-A*kED-34-r*U -h db-server-active mms 2> /dev/null
        endSqlCommand=$(date +%s)

		#echo "$startFormatted   -   $endFormatted -> $((endSqlCommand-startSqlCommand)) seconds" >> $debugFile

done

rowsAfterCommand=$(echo "select count(*) from MMS_RequestStatistic where workspaceKey = $cibortvWorkspaceKey and requestTimestamp >= '$startDate' and requestTimestamp < '$endDate';" | mysql -N -u mms -pF_-A*kED-34-r*U -h db-server-active mms 2> /dev/null)

endCommand=$(date +%s)
echo "$startDate   -   $endDate -> $((endCommand-startCommand)) seconds, removed $((rowsAfterCommand-rowsBeforeCommand)) ($rowsAfterCommand-$rowsBeforeCommand) rows" >> $debugFile

