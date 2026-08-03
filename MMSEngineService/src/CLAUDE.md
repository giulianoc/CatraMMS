# MMSEngineService/src — Motore dei Task e dei Workflow di CatraMMS

Questa directory implementa la classe `MMSEngineProcessor` (`MMSEngineProcessor.h` + `MMSEngineProcessor.cpp`
+ ~28 file `MMSEngineProcessor_*.cpp`), il motore che esegue i **Task** di un **Workflow** CatraMMS. La
documentazione di riferimento per il formato JSON di ogni Task (uno o più file per tipo, con esempi) vive in
`docs/TASK_NN_*.txt`.

## Struttura di un Workflow (Task, GroupOfTasks, eventi)

Un workflow inviato al sistema è un oggetto JSON `{"type": "Workflow", "variables": {...}, "task": <Task o
GroupOfTasks>}`. La validazione è in `MMSEngine/src/Validator.cpp` (`validateIngestedRootMetadata`).

- **Task**: `{ "label": "...", "type": "<Type-string>", "parameters": {...}, "onSuccess": {...},
  "onError": {...}, "onComplete": {...} }`. Il campo `type` è la stringa discriminante che seleziona il
  comportamento (vedi tabella sotto).
- **GroupOfTasks** (`docs/TASK_00_GroupOfTasks_JSON_Format.txt`): `{"type": "GroupOfTasks", "parameters":
  {"executionType": "parallel"|"sequential", "tasks": [...], "referencesOutput": [...]}}`.
  - `executionType` controlla se i Task figli girano in **parallelo** o **in sequenza** (uno dopo l'altro),
    validato in `Validator::validateGroupOfTasksMetadata`.
  - `referencesOutput` (opzionale) indica quali output dei figli propagare agli eventi `onSuccess`/
    `onError`/`onComplete` del gruppo; di default vengono propagati gli output dei figli di primo livello.
    Il gruppo attende che tutti gli output referenziati siano completati prima di proseguire con i propri
    eventi.
  - Anche un `GroupOfTasks` può avere i propri `onSuccess`/`onError`/`onComplete`.
- **Eventi `onSuccess` / `onError` / `onComplete`**: ciascuno ha la forma `{"task": <Task o
  GroupOfTasks>}`, annidato direttamente nel Task/GroupOfTasks padre (non un riferimento per label).
  Validati ricorsivamente in `Validator::validateEvents` (`MMSEngine/src/Validator.cpp`):
  - `onSuccess` — il task annidato gira solo se il padre ha avuto successo.
  - `onError` — il task annidato gira solo se il padre è fallito.
  - `onComplete` — il task annidato gira sempre, indipendentemente dall'esito del padre (ramo "finally").
  - Un task annidato in uno di questi eventi può omettere i propri `"references"`: viene automaticamente
    collegato al media prodotto dal padre.

## Come viene eseguito il grafo (a runtime)

L'espansione del DAG **non avviene qui**, ma a monte, in `API/src/API_Ingestion_1.cpp` al momento
dell'ingestion: l'intero albero Task/GroupOfTasks viene percorso e per ogni Task viene inserita una riga in
`MMS_IngestionJob`, più una riga di dipendenza per ogni arco del grafo (`MMSEngineDBFacade::addIngestionJob`
/ `addIngestionJobDependency`). Ogni riga di dipendenza codifica anche da quale evento nasce, tramite un
flag `dependOnSuccess`:
- `1` → il job nasce da un ramo `onSuccess`
- `0` → il job nasce da un ramo `onError`
- `-1` → il job nasce da un ramo `onComplete` (incondizionato)

`GroupOfTasks` con `executionType: "sequential"` è realizzato incatenando la dipendenza "di partenza" di
ogni figlio al figlio precedente; `"parallel"` fa partire tutti i figli con la stessa dipendenza di
partenza.

A runtime, `MMSEngineProcessor::handleCheckIngestionEvent()` (`MMSEngineProcessor_Ingestion_1.cpp`) esegue
periodicamente `MMSEngineDBFacade::getIngestionsToBeManaged(...)`, una query che restituisce solo i job la
cui condizione di dipendenza è soddisfatta (confrontando lo stato finale del job padre con il
`dependOnSuccess` della dipendenza). È quindi un modello a **coda di pronti / polling delle dipendenze**,
non un modello push/notify: il grafo è già tutto pre-espanso in righe di DB, `MMSEngineProcessor` si limita
a "raccogliere" i job diventati eseguibili.

Per ogni job pronto, si fa uno switch sul tipo (`IngestionType`, enum in `MMSEngine/src/MMSEngineDBFacade.h`,
mappato dalla stringa JSON in `MMSEngine/src/Validator.cpp`). Alcuni Task girano **in linea** sullo stesso
thread del processore (i vari `manage*Task`); molti altri lanciano un **thread dedicato "detached"** (es.
`removeContentThread`, `ftpDeliveryContentThread`, `httpCallbackThread`, `manageConcatThread`,
`manageCutMediaThread`, `postOnFacebookThread`), governato da un budget di thread
(`newThreadPermission(_processorsThreadsNumber)`) per non sovraccaricare il processo quando molti eventi
arrivano nello stesso ciclo di polling — gli eventi in eccesso vengono rimandati al ciclo successivo.

`GroupOfTasks` a runtime (`MMSEngineProcessor_GroupOfTasks.cpp::manageGroupOfTasks`) fa poco: quando gira,
tutti i suoi figli hanno già completato (sono job indipendenti già eseguiti); si limita a inoltrare i
`referencesOutput` come output del gruppo e a marcarsi `End_TaskSuccess` (il successo/fallimento reale dei
singoli figli è già gestito dai flag `dependOnSuccess` dei singoli archi, non dallo stato del gruppo).

**`Workflow-As-Library`** (TASK_35) non compare mai nello switch runtime di `MMSEngineProcessor`: viene
espanso interamente a tempo di ingestion in `API/src/API_Ingestion_1.cpp`, che preleva il Task/GroupOfTasks
salvato come libreria e lo incapsula in un `GroupOfTasks` sintetico — `MMSEngineService` vede solo i Task
concreti già espansi.

## Tabella dei Task disponibili nel Workflow

| Type (JSON) | Doc (`docs/TASK_NN_...`) | Descrizione | File/metodo C++ |
|---|---|---|---|
| `GroupOfTasks` | TASK_00 – GroupOfTasks | Contenitore di sotto-task eseguiti in parallelo o in sequenza, con eventi condivisi | `MMSEngineProcessor_GroupOfTasks.cpp :: manageGroupOfTasks` |
| `Add-Content` | TASK_01 – Add Content | Ingestion di un nuovo media (download http/ftp, move/copy locale, upload) | `MMSEngineProcessor_Ingestion_2.cpp` (+ `LocalAssetIngestionEvent`) |
| `Frame` | TASK_02 – Frame | Estrae un singolo frame da un video in un istante specifico | `MMSEngineProcessor_Frames.cpp :: manageGenerateFramesTask` |
| `Periodical-Frames` | TASK_03 – Periodical Frames | Estrae frame a intervalli periodici | `MMSEngineProcessor_Frames.cpp :: manageGenerateFramesTask` |
| `I-Frames` | TASK_04 – I Frames | Estrae tutti gli I-frame (key frame) di un video | `MMSEngineProcessor_Frames.cpp :: manageGenerateFramesTask` |
| `Motion-JPEG-by-Periodical-Frames` | TASK_05 – Motion JPEG by Periodical Frames | Costruisce un motion-JPEG da frame campionati periodicamente | `MMSEngineProcessor_Frames.cpp :: manageGenerateFramesTask` |
| `Motion-JPEG-by-I-Frames` | TASK_06 – Motion JPEG by I Frames | Costruisce un motion-JPEG dagli I-frame | `MMSEngineProcessor_Frames.cpp :: manageGenerateFramesTask` |
| `Concat-Demuxer` | TASK_07 – Concat Demuxer | Concatena più media di riferimento in un unico output | `MMSEngineProcessor_ConcatCut.cpp :: manageConcatThread` |
| `Cut` | TASK_08 – Cut | Taglia un intervallo temporale da un video | `MMSEngineProcessor_ConcatCut.cpp :: manageCutMediaThread` |
| `Slideshow` | TASK_09 – Slideshow | Costruisce un video da una sequenza di immagini (+ audio opzionale) | `MMSEngineProcessor_SlideShow.cpp :: manageSlideShowTask` |
| *(TASK_10, 16, 17, 18)* | Set/Video/Audio/Image encoding profile | **Non sono Type di Task**: sono schemi JSON dei profili di encoding, referenziati per label/key dai parametri del Task `Encode` | n/a (CRUD profili, dominio `API/src/API_Encoding.cpp`) |
| `Encode` | TASK_12 – Encode | Transcodifica un media con uno o più profili di encoding | `MMSEngineProcessor_Encode.cpp :: manageEncodeTask` |
| `Email-Notification` | TASK_14 – Email | Invia una notifica email | `MMSEngineProcessor_Email.cpp :: emailNotificationThread` |
| `Remove-Content` | TASK_15 – Remove | Rimuove media item / physical path da MMS | `MMSEngineProcessor_RemoveContent.cpp :: removeContentThread` |
| `Overlay-Image-On-Video` | TASK_19 – Overlay Image on Video | Sovrappone un'immagine a un video | `MMSEngineProcessor_Overlay.cpp :: manageOverlayImageOnVideoTask` |
| `Overlay-Text-On-Video` | TASK_20 – Overlay Text on Video | Incide del testo su un video | `MMSEngineProcessor_Overlay.cpp :: manageOverlayTextOnVideoTask` |
| `FTP-Delivery` | TASK_21 – FTP Delivery | Invia media/profilo a una destinazione FTP remota | `MMSEngineProcessor_Ftp.cpp :: ftpDeliveryContentThread` |
| `HTTP-Callback` | TASK_22 – HTTP Callback | Notifica un endpoint HTTP esterno sui media prodotti | `MMSEngineProcessor_HttpCallback.cpp :: httpCallbackThread` |
| `Local-Copy` | TASK_23 – Local Copy | Copia media/profilo su un percorso filesystem locale | `MMSEngineProcessor_CopyMove.cpp :: localCopyContentThread` |
| `Extract-Tracks` | TASK_24 – Extract Tracks | Estrae una traccia audio/video/sottotitoli specifica da un media | `MMSEngineProcessor_ExtractTracks.cpp :: extractTracksContentThread` |
| `Post-On-Facebook` | TASK_25 – Post On Facebook | Pubblica un media su una pagina/account Facebook | `MMSEngineProcessor_Social.cpp :: postOnFacebookThread` |
| `Post-On-YouTube` | TASK_26 – Post On YouTube | Pubblica un media su YouTube | `MMSEngineProcessor_Social.cpp :: postOnYouTubeThread` |
| `Face-Recognition` | TASK_27 – Face Recognition | Rileva/tagga volti in un video | `MMSEngineProcessor_FaceRecognitionAndIdentification.cpp :: manageFaceRecognitionMediaTask` |
| `Face-Identification` | TASK_28 – Face Identification | Identifica volti già rilevati confrontandoli con identità note | `MMSEngineProcessor_FaceRecognitionAndIdentification.cpp :: manageFaceIdentificationMediaTask` — ⚠️ l'esempio JSON nel doc usa per errore `"type": "Face-Recognition"` invece di `"Face-Identification"` |
| `Live-Recorder` | TASK_29 – Live Recorder | Registra un canale live configurato in media segmentati | `MMSEngineProcessor_LiveRecorder.cpp :: manageLiveRecorder` |
| `Change-File-Format` | TASK_30 – Change File Format | Ricontenitorizza un media in un formato diverso senza ri-encoding | `MMSEngineProcessor_ChangeFileFormat.cpp :: changeFileFormatThread` |
| `Video-Speed` | TASK_31 – Video Speed | Accelera/rallenta un video | `MMSEngineProcessor_VideoSpeed.cpp :: manageVideoSpeedTask` |
| `Media-Cross-Reference` | TASK_32 – Media Cross Reference | Combina/mette in relazione due media di riferimento | `MMSEngineProcessor_CrossReference.cpp :: manageMediaCrossReferenceTask` |
| `Picture-In-Picture` | TASK_33 – Picture In Picture | Compone un video inserito ("in finestra") dentro un altro | `MMSEngineProcessor_PictureInPicture.cpp :: managePictureInPictureTask` |
| `Live-Proxy` | TASK_34 – Live Proxy | Ri-trasmette/fa da proxy a un canale live | `MMSEngineProcessor_LiveProxy.cpp :: manageLiveProxy` |
| `Workflow-As-Library` | TASK_35 – Workflow As Library | Istanzia un workflow salvato come libreria, con eventuali parametri sovrascritti | Espanso a tempo di ingestion in `API/src/API_Ingestion_1.cpp` — nessun case runtime in `MMSEngineProcessor` |
| `Live-Cut` | TASK_36 – Live Cut | Taglia un segmento da una registrazione live in corso/registrata | `MMSEngineProcessor_LiveCut.cpp :: manageLiveCutThread_streamSegmenter` / `manageLiveCutThread_hlsSegmenter` |
| `Live-Grid` | TASK_37 – Live Grid | Compone più canali live in una griglia/mosaico | `MMSEngineProcessor_LiveGrid.cpp :: manageLiveGrid` |
| `Countdown` | TASK_38 – Countdown | Genera un video di countdown live da un'immagine/video di riferimento | `MMSEngineProcessor_Countdown.cpp :: manageCountdown` |
| `Intro-Outro-Overlay` | TASK_39 – Intro Outro Overlay | Antepone/accoda clip di intro e outro a un video principale | `MMSEngineProcessor_Overlay.cpp :: manageIntroOutroOverlayTask` |
| `VOD-Proxy` | TASK_40 – VOD Proxy | Costruisce un proxy di streaming on-demand (HLS/DASH) da un media | `MMSEngineProcessor_VODProxy.cpp :: manageVODProxy` |
| `Check-Streaming` | TASK_41 – Check Streaming | Verifica che uno stream live (per label o URL) sia attivo/sano | `MMSEngineProcessor_CheckStreaming.cpp :: checkStreamingThread` |
| `YouTube-Live-Broadcast` | TASK_42 – YouTube Live Broadcast | Crea/gestisce un live broadcast YouTube collegato a un canale/sorgente | `MMSEngineProcessor_Social.cpp :: youTubeLiveBroadcastThread` |
| `Facebook-Live-Broadcast` | TASK_43 – Facebook Live Broadcast | Crea/gestisce un live broadcast Facebook | `MMSEngineProcessor_Social.cpp :: facebookLiveBroadcastThread` |
| `Add-Silent-Audio` | TASK_44 – Add Silent Audio | Aggiunge una traccia audio silenziosa a un video che ne è privo (intera traccia, o a inizio/fine) | `MMSEngineProcessor_SilentTrack.cpp :: manageAddSilentAudioTask` — ⚠️ l'esempio JSON nel doc ha il typo `"Add-Silend-Audio"` invece di `"Add-Silent-Audio"` |

Numeri di task mancanti (11, 13): nessun file doc e nessun valore enum orfano corrispondente — sembrano
numerazioni saltate/ritirate, non funzionalità rimosse.

## Note aggiuntive

- `IngestionType::ContentUpdate` e `IngestionType::ContentRemove` esistono come valori enum (con
  `toString`/`fromString`) in `MMSEngine/src/MMSEngineDBFacade.h` ma non hanno alcun `type == "..."` in
  `Validator.cpp` né un `case` nello switch di `MMSEngineProcessor_Ingestion_1.cpp`: sembrano valori
  legacy non raggiungibili da un workflow JSON client-side.
- File di supporto non legati a un Type di Task: `MMSEngineProcessor_CopyMove.cpp` (usato internamente
  da `Add-Content` per le modalità move/copy della sorgente), `MMSEngineProcessor_Retention.cpp`
  (manutenzione in background: retention dei contenuti e dei dati di DB, nessun collegamento al Type
  dispatch).

## File principali della directory

- `MMSEngineProcessor.h` / `.cpp` — classe principale, ciclo di polling (`operator()`), gestione thread,
  configurazione (Facebook/YouTube Graph API, delivery, email, ecc.).
- `MMSEngineProcessor_Ingestion_1.cpp` / `_Ingestion_2.cpp` — dispatch principale sul tipo di
  `IngestionType` e gestione del download/upload dei media sorgente.
- Un file `MMSEngineProcessor_<Dominio>.cpp` per ciascuna famiglia funzionale elencata in tabella sopra
  (Frames, ConcatCut, SlideShow, Encode, Email, RemoveContent, Overlay, Ftp, HttpCallback, CopyMove,
  ExtractTracks, Social, FaceRecognitionAndIdentification, LiveRecorder, ChangeFileFormat, VideoSpeed,
  CrossReference, PictureInPicture, LiveProxy, LiveCut, LiveGrid, Countdown, VODProxy, CheckStreaming,
  SilentTrack, GroupOfTasks, Retention).
