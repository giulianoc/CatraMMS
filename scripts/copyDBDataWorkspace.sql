/*
This procedure copy the database data, for a specific Workspace, from the DB called 'sourceMms' to the destination DB called 'mms'.
It is assumed that all the media files were already copied:
REMARK: HAS TO BE USED THE SOURCE/DESTINATION WORKSPACE KEY
	log in mms@ec2-34-248-199-119.eu-west-1.compute.amazonaws.com
		mkdir /var/catramms/storage/MMSRepository/MMS_0003/<DESTINATION WORKSPACE KEY>
	nohup rsync -az -e "ssh -i ~/aws-key-ireland.pem" --delete --partial --progress --archive --verbose --compress --omit-dir-times /var/catramms/storage/MMSRepository/MMS_0003/<SOURCE WORKSPACE KEY>/* mms@ec2-34-248-199-119.eu-west-1.compute.amazonaws.com:/var/catramms/storage/MMSRepository/MMS_0003/<DESTINATION WORKSPACE KEY> &

1. create a new database called 'sourceMms' at the same place where the 'mms' destination DB is present
	as root
		dbName=sourceMms
		dbUser=mms
		dbPassword=<same pwd used by 'mms'>
		echo "create database $dbName CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci" | mysql -u root -p$dbPassword

2. get the last export (dbdump) of the source data and import it into the 'sourceMms' DB
	mysql -u mms -p... -h db-server-active sourceMms < mms_....sql 

3. load the stored procedure (if not already done)
	mysql -u mms -p... -h db-server-active mms < ~/copyDBDataWorkspace.sql

4. start mysql to entern into the 'mms' DB
	mysql -u mms -p... -h db-server-active mms

5. call this stored procedure
	CALL copyDBDataWorkspace(3, 2);

6. drop the 'sourceMms' database
	as root
		dbName=sourceMms
		dbUser=<same user used by 'mms'>
		dbPassword=<same pwd used by 'mms'>
		echo "drop database $dbName" | mysql -u root -p$dbPassword

*/

DELIMITER $$

DROP PROCEDURE IF EXISTS copyDBDataWorkspace;

CREATE PROCEDURE copyDBDataWorkspace (
	IN sourceWorkspaceKey BIGINT,
	IN destWorkspaceKey BIGINT
)
BEGIN
	DECLARE mediaItemFinished INTEGER DEFAULT 0;
	DECLARE mediaItemCursorIndex BIGINT UNSIGNED DEFAULT 0;

	DECLARE sourceMediaItemKey BIGINT UNSIGNED;
	DECLARE sourceTitle VARCHAR (256) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;
	DECLARE sourceIngester VARCHAR (128);
	DECLARE sourceUserData JSON;
	DECLARE sourceDeliveryFileName VARCHAR (128);
	DECLARE sourceIngestionDate DATETIME;
	DECLARE sourceContentType VARCHAR (32);
	DECLARE sourceStartPublishing DATETIME;
	DECLARE sourceEndPublishing DATETIME;
	DECLARE sourceRetentionInMinutes BIGINT UNSIGNED;
	DECLARE sourceMarkedAsRemoved TINYINT;

	DECLARE destMediaItemKey BIGINT DEFAULT 0;

	DECLARE sourceFaceExist INTEGER DEFAULT 0;
	DECLARE sourceFaceMediaItemKey BIGINT DEFAULT 0;
	DECLARE destFaceMediaItemKey BIGINT DEFAULT 0;

	DECLARE destContentProviderKey BIGINT;

	-- cursor declaration and handler has to be
	-- after all the declarations
	DECLARE mediaItems 
		CURSOR FOR 
			SELECT mediaItemKey, title,
				ingester, userData, deliveryFileName,
				ingestionDate, contentType, startPublishing, endPublishing,
				retentionInMinutes, markedAsRemoved
			FROM sourceMms.MMS_MediaItem
			WHERE workspaceKey = sourceWorkspaceKey
				and contentType = 'Video';
	DECLARE CONTINUE HANDLER FOR NOT FOUND SET mediaItemFinished = 1;

	select contentProviderKey into destContentProviderKey
		from mms.MMS_ContentProvider
		where workspaceKey = destWorkspaceKey;

	select concat('A: destContentProviderKey: ', destContentProviderKey);

	OPEN mediaItems;

	getMediaItem: LOOP
		FETCH mediaItems INTO sourceMediaItemKey, sourceTitle,
			sourceIngester, sourceUserData, sourceDeliveryFileName,
			sourceIngestionDate, sourceContentType, sourceStartPublishing,
			sourceEndPublishing, sourceRetentionInMinutes, sourceMarkedAsRemoved;
		IF mediaItemFinished = 1 THEN 
			LEAVE getMediaItem;
		END IF;
		SELECT concat('B: ', mediaItemCursorIndex, ': ', sourceMediaItemKey, ', ', sourceTitle);

		insert into mms.MMS_MediaItem (mediaItemKey, workspaceKey, contentProviderKey,
			title, ingester, userData, deliveryFileName, ingestionJobKey,
                    	ingestionDate, contentType, startPublishing, endPublishing,
                    	retentionInMinutes, markedAsRemoved)
		values (NULL, destWorkspaceKey, destContentProviderKey,
			sourceTitle, sourceIngester, sourceUserData, sourceDeliveryFileName, 0,
                        sourceIngestionDate, sourceContentType, sourceStartPublishing,
			sourceEndPublishing, sourceRetentionInMinutes, sourceMarkedAsRemoved);
		SET destMediaItemKey = LAST_INSERT_ID();

		insert into mms.MMS_Tag (tagKey, mediaItemKey, name)
			select NULL, destMediaItemKey, name
			from sourceMms.MMS_Tag
			where mediaItemKey = sourceMediaItemKey;

		insert into mms.MMS_ExternalUniqueName (workspaceKey, uniqueName, mediaItemKey)
			select destWorkspaceKey, uniqueName, destMediaItemKey
			from sourceMms.MMS_ExternalUniqueName
			where mediaItemKey = sourceMediaItemKey;

		BLOCK2: BEGIN
			DECLARE physicalPathCursorIndex BIGINT UNSIGNED DEFAULT 0;

			DECLARE sourcePhysicalPathKey BIGINT UNSIGNED;
			DECLARE sourceDrm TINYINT;
			DECLARE sourceExternalReadOnlyStorage TINYINT;
			DECLARE sourceFileName VARCHAR (128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;
			DECLARE sourceRelativePath VARCHAR (256);
			DECLARE sourcePartitionNumber INT;
                        DECLARE sourceSizeInBytes BIGINT UNSIGNED;
			DECLARE sourceEncodingProfileKey BIGINT UNSIGNED;
			DECLARE sourceDurationInMilliSeconds BIGINT;
			DECLARE sourceBitRate INT;
                        DECLARE sourceDeliveryInfo JSON;
			DECLARE sourceIsAlias INT;
			DECLARE sourceCreationDate TIMESTAMP;
			DECLARE sourceRetentionInMinutes bigint unsigned;

			DECLARE destPhysicalPathKey BIGINT UNSIGNED;
			DECLARE sourceEncodingProfileLabel VARCHAR (64);
			DECLARE destEncodingProfileKey BIGINT UNSIGNED;

			DECLARE physicalPathFinished INTEGER DEFAULT 0;
			DECLARE duplicateKey INTEGER DEFAULT 0;

			-- cursor declaration and handler has to be after all the declarations
			DECLARE physicalPaths 
				CURSOR FOR 
					select physicalPathKey, drm, externalReadOnlyStorage,
						fileName, relativePath, partitionNumber,
                    				sizeInBytes, encodingProfileKey,
						durationInMilliSeconds, bitRate,
                    				deliveryInfo, isAlias, creationDate,
						retentionInMinutes
					from sourceMms.MMS_PhysicalPath
					where mediaItemKey = sourceMediaItemKey;
			-- declare NOT FOUND handler
			DECLARE CONTINUE HANDLER FOR NOT FOUND SET physicalPathFinished = 1;

			DECLARE CONTINUE HANDLER FOR 1062 SET duplicateKey = 1;


			OPEN physicalPaths;

			getPhysicalPath: LOOP
				FETCH physicalPaths INTO sourcePhysicalPathKey, sourceDrm,
					sourceExternalReadOnlyStorage, sourceFileName,
					sourceRelativePath, sourcePartitionNumber,
                        		sourceSizeInBytes, sourceEncodingProfileKey,
					sourceDurationInMilliSeconds, sourceBitRate,
                        		sourceDeliveryInfo, sourceIsAlias, sourceCreationDate,
					sourceRetentionInMinutes;
				IF physicalPathFinished = 1 THEN 
					LEAVE getPhysicalPath;
				END IF;

				SELECT concat('C: physicalPathCursorIndex: ', physicalPathCursorIndex, ', ', sourcePhysicalPathKey);

				IF sourceEncodingProfileKey is not null THEN
					select label into sourceEncodingProfileLabel
						from sourceMms.MMS_EncodingProfile
						where encodingProfileKey = sourceEncodingProfileKey;
					SELECT concat('D: sourceEncodingProfileLabel: ', sourceEncodingProfileLabel);
					IF sourceEncodingProfileLabel = 'MMS_HLS_H264_2500Kb_medium_720p25_AAC_160' THEN
						SET sourceEncodingProfileLabel = 'MMS_HLS_H264_2500Kb_medium_720p25_high422_AAC_160';
						SELECT concat('E: sourceEncodingProfileLabel: ', sourceEncodingProfileLabel);
					END IF;
					select encodingProfileKey into destEncodingProfileKey
						from mms.MMS_EncodingProfile
						where label = sourceEncodingProfileLabel;
				ELSE
					SET destEncodingProfileKey = NULL;
				END IF;

				SELECT concat('F: destEncodingProfileKey: ', destEncodingProfileKey);

				SET duplicateKey = 0;
				insert into mms.MMS_PhysicalPath (physicalPathKey, mediaItemKey,
					drm, externalReadOnlyStorage, fileName, relativePath,
					partitionNumber, sizeInBytes, encodingProfileKey,
					durationInMilliSeconds, bitRate, deliveryInfo,
					isAlias, creationDate, retentionInMinutes)
				values (NULL, destMediaItemKey, sourceDrm,
                    			sourceExternalReadOnlyStorage, sourceFileName,
					sourceRelativePath, sourcePartitionNumber,
					sourceSizeInBytes, destEncodingProfileKey,
					sourceDurationInMilliSeconds,
					sourceBitRate, sourceDeliveryInfo, sourceIsAlias,
					sourceCreationDate, sourceRetentionInMinutes);
				IF duplicateKey = 1 THEN 
					ITERATE getPhysicalPath;
				END IF;

				SET destPhysicalPathKey = LAST_INSERT_ID();

				SELECT concat('G: destPhysicalPathKey: ', destPhysicalPathKey);

				insert into mms.MMS_VideoTrack (videoTrackKey, physicalPathKey,
                    			trackIndex, durationInMilliSeconds, width, height,
                    			avgFrameRate, codecName, bitRate, profile)
				select NULL, destPhysicalPathKey,
					trackIndex, durationInMilliSeconds, width, height,
                                        avgFrameRate, codecName, bitRate, profile
					from sourceMms.MMS_VideoTrack
					where physicalPathKey = sourcePhysicalPathKey;

				insert into mms.MMS_AudioTrack (audioTrackKey, physicalPathKey,
                    			trackIndex, durationInMilliSeconds,
                    			codecName, bitRate, sampleRate, channels, language)
				select NULL, destPhysicalPathKey,
					trackIndex, durationInMilliSeconds,
                                        codecName, bitRate, sampleRate, channels, language
					from sourceMms.MMS_AudioTrack
					where physicalPathKey = sourcePhysicalPathKey;

				SET physicalPathCursorIndex = physicalPathCursorIndex + 1;
			END LOOP getPhysicalPath;

			CLOSE physicalPaths;
		END BLOCK2;

		/* FaceOfVideo */

		select count(*) into sourceFaceExist
			from sourceMms.MMS_CrossReference
			where targetMediaItemKey = sourceMediaItemKey;
		SELECT concat('H: sourceFaceExist: ', sourceFaceExist);
		IF sourceFaceExist = 1 THEN 
			select cr.sourceMediaItemKey into sourceFaceMediaItemKey
				from sourceMms.MMS_CrossReference cr
				where cr.targetMediaItemKey = sourceMediaItemKey;

			SELECT concat('I: sourceFaceMediaItemKey: ', sourceFaceMediaItemKey);

			insert into mms.MMS_MediaItem (mediaItemKey, workspaceKey,
				contentProviderKey, title, ingester, userData,
				deliveryFileName, ingestionJobKey, ingestionDate, contentType,
				startPublishing, endPublishing,
                    		retentionInMinutes, markedAsRemoved)
			select NULL, destWorkspaceKey,
				destContentProviderKey, title, ingester, userData,
				deliveryFileName, 0, ingestionDate, contentType,
				startPublishing, endPublishing,
                        	retentionInMinutes, markedAsRemoved
				from sourceMms.MMS_MediaItem
				where mediaItemKey = sourceFaceMediaItemKey;
			SET destFaceMediaItemKey = LAST_INSERT_ID();

			SELECT concat('L: destFaceMediaItemKey: ', destFaceMediaItemKey);

			insert into mms.MMS_Tag (tagKey, mediaItemKey, name)
				select NULL, destFaceMediaItemKey, name
				from sourceMms.MMS_Tag
				where mediaItemKey = sourceFaceMediaItemKey;

			insert into mms.MMS_ExternalUniqueName (workspaceKey,
				uniqueName, mediaItemKey)
				select destWorkspaceKey,
				uniqueName, destFaceMediaItemKey
				from sourceMms.MMS_ExternalUniqueName
				where mediaItemKey = sourceFaceMediaItemKey;

			BLOCK3: BEGIN
				DECLARE physicalPathFinished INTEGER DEFAULT 0;
				DECLARE duplicateKey INTEGER DEFAULT 0;
				DECLARE physicalPathCursorIndex BIGINT UNSIGNED DEFAULT 0;

				DECLARE sourcePhysicalPathKey BIGINT UNSIGNED;
				DECLARE sourceDrm TINYINT;
				DECLARE sourceExternalReadOnlyStorage TINYINT;
				DECLARE sourceFileName VARCHAR (128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;
				DECLARE sourceRelativePath VARCHAR (256);
				DECLARE sourcePartitionNumber INT;
                        	DECLARE sourceSizeInBytes BIGINT UNSIGNED;
				DECLARE sourceEncodingProfileKey BIGINT UNSIGNED;
				DECLARE sourceDurationInMilliSeconds BIGINT;
				DECLARE sourceBitRate INT;
                        	DECLARE sourceDeliveryInfo JSON;
				DECLARE sourceIsAlias INT;
				DECLARE sourceCreationDate TIMESTAMP;
				DECLARE sourceRetentionInMinutes bigint unsigned;

				DECLARE destPhysicalPathKey BIGINT UNSIGNED;
				DECLARE sourceEncodingProfileLabel VARCHAR (64);
				DECLARE destEncodingProfileKey BIGINT UNSIGNED;

				-- cursor declaration and handler has to be
				-- after all the declarations
				DECLARE physicalPaths 
					CURSOR FOR 
						select physicalPathKey, drm,
							externalReadOnlyStorage, fileName,
							relativePath, partitionNumber,
                    					sizeInBytes, encodingProfileKey,
							durationInMilliSeconds, bitRate,
                    					deliveryInfo, isAlias, creationDate,
							retentionInMinutes
						from sourceMms.MMS_PhysicalPath
						where mediaItemKey = sourceFaceMediaItemKey;
				-- declare NOT FOUND handler
				DECLARE CONTINUE HANDLER FOR NOT FOUND SET physicalPathFinished = 1;
				DECLARE CONTINUE HANDLER FOR 1062 SET duplicateKey = 1;

				OPEN physicalPaths;

				getPhysicalPath: LOOP
					FETCH physicalPaths INTO sourcePhysicalPathKey,
						sourceDrm, sourceExternalReadOnlyStorage,
						sourceFileName, sourceRelativePath,
						sourcePartitionNumber, sourceSizeInBytes,
						sourceEncodingProfileKey,
						sourceDurationInMilliSeconds, sourceBitRate,
                        			sourceDeliveryInfo, sourceIsAlias,
						sourceCreationDate, sourceRetentionInMinutes;
					IF physicalPathFinished = 1 THEN 
						LEAVE getPhysicalPath;
					END IF;

					SELECT concat('M: physicalPathCursorIndex: ',
						physicalPathCursorIndex, ', ',
						sourcePhysicalPathKey);

					IF sourceEncodingProfileKey is not null THEN
						select label into sourceEncodingProfileLabel
							from sourceMms.MMS_EncodingProfile
							where encodingProfileKey = sourceEncodingProfileKey;
						SELECT concat('N: sourceEncodingProfileLabel: ',
							sourceEncodingProfileLabel);

						IF sourceEncodingProfileLabel = 'MMS_JPG_240' THEN
							SET sourceEncodingProfileLabel = 'MMS_JPG_W240';
						ELSEIF sourceEncodingProfileLabel = 'MMS_PNG_240' THEN
							SET sourceEncodingProfileLabel = 'MMS_PNG_W240';
						ELSEIF sourceEncodingProfileLabel = 'MMS_PNG_1280' THEN
							SET sourceEncodingProfileLabel = 'MMS_PNG_W1280';
						END IF;
						SELECT concat('O: sourceEncodingProfileLabel: ', sourceEncodingProfileLabel);
						select encodingProfileKey into destEncodingProfileKey
							from mms.MMS_EncodingProfile
							where label = sourceEncodingProfileLabel;
					ELSE
						SET destEncodingProfileKey = NULL;
					END IF;

					SELECT concat('P: destEncodingProfileKey: ',
						destEncodingProfileKey);

					SET duplicateKey = 0;
					insert into mms.MMS_PhysicalPath (physicalPathKey, 
						mediaItemKey, drm, externalReadOnlyStorage,
						fileName, relativePath,
						partitionNumber, sizeInBytes, encodingProfileKey,
						durationInMilliSeconds, bitRate, deliveryInfo,
						isAlias, creationDate, retentionInMinutes)
					values (NULL, destFaceMediaItemKey, sourceDrm,
                    				sourceExternalReadOnlyStorage, sourceFileName,
						sourceRelativePath, sourcePartitionNumber,
						sourceSizeInBytes, destEncodingProfileKey,
						sourceDurationInMilliSeconds,
						sourceBitRate, sourceDeliveryInfo, sourceIsAlias,
						sourceCreationDate, sourceRetentionInMinutes);
					IF duplicateKey = 1 THEN 
						ITERATE getPhysicalPath;
					END IF;

					SET destPhysicalPathKey = LAST_INSERT_ID();

					SELECT concat('Q: destPhysicalPathKey: ',
						destPhysicalPathKey);

					insert into mms.MMS_ImageItemProfile (
						physicalPathKey, width, height,
                    				format, quality)
					select destPhysicalPathKey, width, height,
                                        	format, quality
						from sourceMms.MMS_ImageItemProfile
						where physicalPathKey = sourcePhysicalPathKey;

					SET physicalPathCursorIndex = physicalPathCursorIndex + 1;
				END LOOP getPhysicalPath;

				CLOSE physicalPaths;
			END BLOCK3;

			insert into mms.MMS_CrossReference (
				sourceMediaItemKey, type, targetMediaItemKey, parameters)
			select destFaceMediaItemKey, type, destMediaItemKey, parameters
				from sourceMms.MMS_CrossReference cr
				where cr.sourceMediaItemKey = sourceFaceMediaItemKey
					and cr.targetMediaItemKey = sourceMediaItemKey;
		END IF;

		SET mediaItemCursorIndex = mediaItemCursorIndex + 1;
	END LOOP getMediaItem;

	CLOSE mediaItems;

END$$

DELIMITER ;

