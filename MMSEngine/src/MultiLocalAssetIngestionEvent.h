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

#ifndef MultiLocalAssetIngestionEvent_h
#define MultiLocalAssetIngestionEvent_h

#include "Event2.h"
#include "MMSEngineDBFacade.h"
#include <iostream>

using namespace std;

#define MMSENGINE_EVENTTYPEIDENTIFIER_MULTILOCALASSETINGESTIONEVENT 5

class MultiLocalAssetIngestionEvent : public Event2
{
  private:
	int64_t _ingestionJobKey;
	int64_t _encodingJobKey;
	shared_ptr<Workspace> _workspace;
	json _parametersRoot;

  public:
	void setIngestionJobKey(int64_t ingestionJobKey) { _ingestionJobKey = ingestionJobKey; }
	int64_t getIngestionJobKey() { return _ingestionJobKey; }

	void setEncodingJobKey(int64_t encodingJobKey) { _encodingJobKey = encodingJobKey; }
	int64_t getEncodingJobKey() { return _encodingJobKey; }

	void setWorkspace(shared_ptr<Workspace> workspace) { _workspace = workspace; }
	shared_ptr<Workspace> getWorkspace() { return _workspace; }

	void setParametersRoot(json parametersRoot) { _parametersRoot = parametersRoot; }
	json getParametersRoot() { return _parametersRoot; }
};

#endif
