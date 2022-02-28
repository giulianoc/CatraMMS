Non è uno script ma potrebbe diventarlo.

Assumiamo bisogna spostare i contenuti dalla partizione 7 alla partizione 6

TUTTI i media da spostare:
select relativePath, encodingProfileKey  from MMS_MediaItem mi, MMS_PhysicalPath pp where mi.mediaItemKey = pp.mediaItemKey and partitionNumber = 7 order by physicalPathKey asc;

HLS (elenca i contenuti HLS da spostare. Qui si vuole recuperare i media che sono rappresentati da una directory (m3u8 + ts)). Da questa select prendere il min e max physicalPathKey
select physicalPathKey , relativePath, encodingProfileKey  from MMS_MediaItem mi, MMS_PhysicalPath pp where mi.mediaItemKey = pp.mediaItemKey and partitionNumber = 7 and encodingProfileKey in (19, 28) order by physicalPathKey asc;

ALTRI (elenca i singoli file (non directory come HLS) da spostare. Da questa select prendere il min e max physicalPathKey
select physicalPathKey, relativePath, encodingProfileKey  from MMS_MediaItem mi, MMS_PhysicalPath pp where mi.mediaItemKey = pp.mediaItemKey and partitionNumber = 7 and (encodingProfileKey is null or encodingProfileKey not in (19, 28)) order by physicalPathKey asc;

————————

ALTRI
Il -d del cut contiene un tab separator (CTRL V più il tasto tab)
echo "SET @row_number = 0; select (@row_number:=@row_number + 1) AS num, concat('sleep 1; echo ', @row_number, '; mkdir -p ', '/var/catramms/storage/MMSRepository/MMS_0006/', workspaceKey, relativePath, '; mv ', '/var/catramms/storage/MMSRepository/MMS_0007/', workspaceKey, relativePath, fileName, ' /var/catramms/storage/MMSRepository/MMS_0006/', workspaceKey, relativePath) from MMS_MediaItem mi, MMS_PhysicalPath pp where mi.mediaItemKey = pp.mediaItemKey and partitionNumber = 7 and (encodingProfileKey is null or encodingProfileKey not in (19, 28)) and physicalPathKey >= 146005 and physicalPathKey <= 147026;" | mysql -u mms -pF_-A*kED-34-r*U -h db-server-active mms | cut -d'   ' -f2 > c.sh 

update MMS_PhysicalPath set partitionNumber = 6 where partitionNumber = 7 and (encodingProfileKey is null or encodingProfileKey not in (19, 28)) and physicalPathKey >= 146005 and physicalPathKey <= 147026;



HLS
Il -d del cut contiene un tab separator (CTRL V più il tasto tab)
echo "SET @row_number = 0; select (@row_number:=@row_number + 1) AS num, concat('sleep 1; echo ', @row_number, '; mkdir -p ', '/var/catramms/storage/MMSRepository/MMS_0006/', workspaceKey, SUBSTRING_INDEX(relativePath, '/', 4), '; mv ', '/var/catramms/storage/MMSRepository/MMS_0007/', workspaceKey, relativePath, ' /var/catramms/storage/MMSRepository/MMS_0006/', workspaceKey, SUBSTRING_INDEX(relativePath, '/', 4)) from MMS_MediaItem mi, MMS_PhysicalPath pp where mi.mediaItemKey = pp.mediaItemKey and partitionNumber = 7 and encodingProfileKey in (19, 28) and physicalPathKey >= 49541 and physicalPathKey <= 146756;" | mysql -u mms -pF_-A*kED-34-r*U -h db-server-active mms | cut -d'   ' -f2 > c.sh 

update MMS_PhysicalPath set partitionNumber = 6 where partitionNumber = 7 and encodingProfileKey in (19, 28) and physicalPathKey >= 49541 and physicalPathKey <= 146756;

