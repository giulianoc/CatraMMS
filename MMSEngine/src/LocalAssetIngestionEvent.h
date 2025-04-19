/*
 Copyright (C) Giuliano Catrambone (giuliano.catrambone@catrasoftware.it)

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either
 version 2 of the License, or (at your option) any later
 version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

 Commercial use other than under the terms of the GNU General Public
 License is allowed only after express negotiation of conditions
 with the authors.
*/

#ifndef LocalAssetIngestionEvent_h
#define LocalAssetIngestionEvent_h

#include "Event2.h"
#include "MMSEngineDBFacade.h"
#include <iostream>

using namespace std;

#define MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT 2

class LocalAssetIngestionEvent : public Event2
{
  private:
	int64_t _ingestionJobKey;
	shared_ptr<Workspace> _workspace;
	MMSEngineDBFacade::IngestionType _ingestionType;

	bool _externalReadOnlyStorage;
	string _externalStorageMediaSourceURL;

	string _metadataContent;
	string _ingestionSourceFileName;
	string _mmsSourceFileName;
	string _forcedAvgFrameRate;
	bool _ingestionRowToBeUpdatedAsSuccess;

  public:
	void setMMSSourceFileName(string mmsSourceFileName) { _mmsSourceFileName = mmsSourceFileName; }
	string getMMSSourceFileName() { return _mmsSourceFileName; }

	void setExternalStorageMediaSourceURL(string externalStorageMediaSourceURL) { _externalStorageMediaSourceURL = externalStorageMediaSourceURL; }
	string getExternalStorageMediaSourceURL() { return _externalStorageMediaSourceURL; }

	void setIngestionType(MMSEngineDBFacade::IngestionType ingestionType) { _ingestionType = ingestionType; }
	MMSEngineDBFacade::IngestionType getIngestionType() { return _ingestionType; }

	void setMetadataContent(string metadataContent) { _metadataContent = metadataContent; }
	string getMetadataContent() { return _metadataContent; }

	void setIngestionSourceFileName(string ingestionSourceFileName) { _ingestionSourceFileName = ingestionSourceFileName; }
	string getIngestionSourceFileName() { return _ingestionSourceFileName; }

	void setIngestionJobKey(int64_t ingestionJobKey) { _ingestionJobKey = ingestionJobKey; }
	int64_t getIngestionJobKey() { return _ingestionJobKey; }

	void setExternalReadOnlyStorage(int64_t externalReadOnlyStorage) { _externalReadOnlyStorage = externalReadOnlyStorage; }
	int64_t getExternalReadOnlyStorage() { return _externalReadOnlyStorage; }

	void setIngestionRowToBeUpdatedAsSuccess(bool ingestionRowToBeUpdatedAsSuccess)
	{
		_ingestionRowToBeUpdatedAsSuccess = ingestionRowToBeUpdatedAsSuccess;
	}
	bool getIngestionRowToBeUpdatedAsSuccess() { return _ingestionRowToBeUpdatedAsSuccess; }

	void setWorkspace(shared_ptr<Workspace> workspace) { _workspace = workspace; }
	shared_ptr<Workspace> getWorkspace() { return _workspace; }

	void setForcedAvgFrameRate(string forcedAvgFrameRate) { _forcedAvgFrameRate = forcedAvgFrameRate; }
	string getForcedAvgFrameRate() { return _forcedAvgFrameRate; }
};

#endif
