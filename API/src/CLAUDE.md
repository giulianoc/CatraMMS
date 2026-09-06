# API/src — Servizio API + Delivery di CatraMMS

Questa directory compila **un unico binario, `api.fcgi`** (FastCGI, vedi `CMakeLists.txt`). Implementa due
servizi *logici* — "API" (endpoint di business/gestione) e "Delivery" (autorizzazione alla consegna dei
media) — ma questi vivono in **un solo processo, un'unica classe C++ (`API`), un'unica mappa di handler**.
Non esiste un binario delivery separato né uno split basato su prefisso URL tra i due.

## Modalità di processo (apiMain.cpp)

`apiMain.cpp` è il punto di ingresso. Il percorso di configurazione arriva dalla variabile d'ambiente
`MMS_CONFIGPATHNAME`. Un argomento CLI opzionale seleziona la modalità di deploy — è questo ciò che si
intende quando si parla di "servizio API" vs "servizio Delivery": è un **flag a tempo di deploy**, non una
decisione di routing per singola richiesta:

- *(nessun argomento)* — API + Delivery combinati in un unico processo (`isDeliveryAndAPIServerTogether = true`).
- `NoFileSystem` — nodo solo-API: nessun accesso al filesystem/ai media locali.
- `NoDatabase` — nodo Delivery standalone (es. un edge distribuito tipo CDN): nessun accesso al DB, tutte
  le richieste vengono autorizzate solo tramite path/token.

Indipendentemente dalla modalità, `apiMain.cpp` avvia `api.threadsNumber` istanze di `API` (thread),
ciascuna eseguendo il loop di accept condiviso `FastCGIAPI::operator()` (`FCGX_Accept_r`, serializzato
tramite un mutex condiviso). Girano inoltre due thread di monitoraggio in background a seconda della
modalità: `DeliveryServerBandwidthUsageThread` / `DeliveryServerCPUUsageThread` (modalità delivery) contro
le rispettive controparti in modalità API.

## Routing delle richieste — non basato sull'URL

`FastCGIAPI::handleRequest()` fa il dispatch esclusivamente sul parametro di query string
**`x-api-method`**, cercato in una `unordered_map<string, Handler>` popolata da circa 127 chiamate a
`registerHandler(...)` nel costruttore di `API`. Sia gli handler API che quelli Delivery vengono
registrati nella **stessa mappa**.

L'unico caso speciale basato sul path URL: `requestURI == "/catramms/delivery/authorization"` salta la
Basic Auth (è il target di callback `auth_request` di nginx). Una allow-list fissa di metodi
(`login`, `registerUser`, `confirmRegistration`, `createTokenToResetPassword`, `resetPassword`,
`deliveryAuthorizationThroughParameter`, `deliveryAuthorizationThroughPath`,
`manageHTTPStreamingManifest_authorizationThroughParameter`, `avgBandwidthUsage`, `status`) salta anch'essa
la Basic Auth in quanto endpoint pre-autenticazione/pubblici/di health check.

**Importante:** `api.fcgi` non serve mai direttamente i byte dei media. nginx serve i file
`.ts`/`.mp4`/`.m3u8` direttamente e richiama `api.fcgi` (tramite `auth_request` ed endpoint di riscrittura
dei manifest) solo per autorizzare/riscrivere. `api.fcgi` è il cervello di autorizzazione/orchestrazione,
non il motore che spinge i byte.

## Autenticazione / autorizzazione

- **Lato API**: HTTP Basic Auth, username = `userKey` numerico, password = API key, verificata tramite
  `MMSEngineDBFacade::checkAPIKey`. Restituisce un `Workspace` più circa 17 flag di capability (`admin`,
  `canIngestWorkflow`, `canDeliveryAuthorization`, `canEditMedia`, `canEditConfiguration`,
  `canKillEncoding`, `canEditEncodersPool`, `canEnableDeliveryServer`, ecc. — vedi
  `API::APIAuthorizationDetails` in `API.h`), controllati ad hoc all'inizio di ogni handler.
  `login` supporta anche opzionalmente il bind LDAP (`LdapWrapper`) in alternativa a email/password su DB.
- **Lato Delivery**: nessuna Basic Auth. Token con scadenza temporale, cifrati con OpenSSL
  (`Encrypt::opensslEncrypt` / `opensslDecrypt`, non JWT), generati/verificati tramite
  `MMSDeliveryAuthorization`. nginx richiama tramite `auth_request` gli handler
  `deliveryAuthorizationThroughParameter` / `deliveryAuthorizationThroughPath` con gli header
  `X-Original-URI`/`X-Original-Method`; i manifest HLS/DASH vengono riscritti al volo
  (`manageHTTPStreamingManifest_authorizationThroughParameter`) per apporre token freschi su ogni URI di
  segmento (patch via libxml2/XPath per DASH).
- **Firma CDN**, instradata in base a una colonna `cdnName` letta dalla configurazione del canale RTMP:
  `aws` → `AWSSigner` (firma canned-policy per CloudFront, implementazione in-house, non l'SDK AWS),
  `medianova` → `MMSDeliveryAuthorization::getMedianovaSignedTokenURL`, `cdn77` → `getSignedCDN77URL`.

## Database

PostgreSQL via libpqxx è il backend attivo (`#ifdef __POSTGRES__`); esiste un percorso MySQL legacy dietro
`DBCONNECTION_MYSQL`. Pool di connessione master/slave separati, con dimensioni configurabili
(`postgres.master.apiPoolSize` / `postgres.slave.apiPoolSize`). Tutto l'SQL vive dietro
`MMSEngineDBFacade` (in `../MMSEngine/src`) — i file in questa directory non scrivono mai SQL grezzo. Le
tabelle usano il prefisso `MMS_` (es. `MMS_Conf_FTP`, `MMS_Conf_RTMPChannel`, `MMS_IngestionJob`,
`MMS_SignedURL`).

## Integrazioni esterne

| Integrazione | Utilizzo |
|---|---|
| AWS CloudFront (`AWSSigner`) | URL firmati per delivery VOD/live |
| Medianova / CDN77 | CDN alternative con URL firmati |
| SMTP (via `CurlWrapper`) | Richieste di supporto, email di registrazione/reset password |
| LDAP (`LdapWrapper`) | Autenticazione esterna opzionale per `login` |
| FTP | Configurazioni di destinazione push-delivery salvate (solo CRUD in questo file) |
| Nodi encoder ffmpeg remoti (`CurlWrapper`, Basic Auth) | Kill di job di encoding, cambio playlist/overlay text del live-proxy — `api.fcgi` è il control plane, non il transcoder |
| Reporting statistiche delivery-server | In modalità split-process (`NoDatabase`), i thread bandwidth/CPU fanno HTTP-PUT delle statistiche verso gli endpoint REST del server API; se co-locati, scrivono direttamente sul DB |

Nessun SDK S3, nessun JWT, nessuna coda di messaggi (RabbitMQ/Kafka) in uso in questa directory.

## Mappa handler file per file

| File | Dominio | Handler principali |
|---|---|---|
| `API_Configuration.cpp` | CRUD per configurazioni di destinazione streaming (multi-tenant) | `add/modify/remove/list` per YouTube, Facebook, Twitch, TikTok, Stream generico, SourceTVStream, canale RTMP/SRT/HLS, FTP, configurazioni Email |
| `API_DeliveryAuthorization.cpp` | Emissione/validazione dei token di delivery — nucleo del servizio "Delivery" | `createDeliveryAuthorization`, `createBulkOfDeliveryAuthorization`, `binaryAuthorization`, `deliveryAuthorizationThroughParameter`, `deliveryAuthorizationThroughPath`, `getSignedURL` |
| `API_DeliveryServer.cpp` | CRUD amministrativo dell'inventario dei nodi delivery e relative statistiche | `add/modify/remove/list DeliveryServer`, `updateDeliveryServerBandwidthStats`, `updateDeliveryServerCPUUsageStats`, associazioni workspace↔server |
| `API_EncodersPool.cpp` | CRUD amministrativo dell'inventario dei nodi encoder e dei pool | `add/modify/remove/list Encoder(sPool)`, `updateEncoderBandwidthStats`, `updateEncoderCPUUsageStats`, associazioni workspace↔encoder |
| `API_Encoding.cpp` | Gestione dei job di transcodifica e del catalogo dei profili di encoding | `encodingJobsStatus`, `encodingJobPriority`, `killOrCancelEncodingJob`, `encodingProfiles(Sets)List`, `addEncodingProfile`, `addUpdateEncodingProfilesSet` |
| `API_Ingestion_1.cpp` / `API_Ingestion_2.cpp` | Superficie HTTP del motore di ingestion dei workflow (l'area più grande e complessa) | `ingestion` (invio workflow), `uploadedBinary`, `ingestionJobsStatus`, `cancelIngestionJob`, `updateIngestionJob`, `ingestionJobSwitchToEncoder`, `changeLiveProxyPlaylist`, `changeLiveProxyOverlayText`, `ingestionRootsStatus`, `ingestionRootMetaDataContent` |
| `API_Invoice.cpp` | Record di fatturazione | `addInvoice`, `invoiceList` |
| `API_MediaItem.cpp` | Catalogo degli asset media | `updateMediaItem`, `updatePhysicalPath`, `mediaItemsList`, `tagsList` |
| `API_Statistic.cpp` | Reporting di utilizzo/analytics | `addRequestStatistic`, `requestStatisticList` (per contenuto/utente/mese/giorno/ora/nazione), `loginStatisticList` |
| `API_UserWorkspace.cpp` | Ciclo di vita account utente, autenticazione, gestione workspace (tenant) | `registerUser`, `login`, `confirmRegistration`, `createTokenToResetPassword`/`resetPassword`, `createWorkspace`, `shareWorkspace`, `workspaceList`, `updateUser` |
| `API_WorkflowLibrary.cpp` | Template di workflow di ingestion riutilizzabili con nome | `workflowsAsLibraryList`, `workflowAsLibraryContent`, `saveWorkflowAsLibrary`, `removeWorkflowAsLibrary` |
| `DeliveryServerBandwidthUsageThread.cpp/.h` | Thread in background che riporta la banda di rete (percentile-smoothed) per un nodo delivery | — |
| `DeliveryServerCPUUsageThread.cpp/.h` | Thread in background che riporta l'utilizzo CPU per un nodo delivery | — |
| `echo.cpp` | Esempio generico FastCGI vendorizzato; **non compilato** (escluso dalle sorgenti in `CMakeLists.txt`) | — |

## Build

CMake, target unico `api.fcgi` (`install(... DESTINATION bin/cgi)`). Collega le librerie interne al repo
(`FastCGIAPI`, `MMSDelivery`, `MMSStorage`, `MMSEngine`, `CurlWrapper`, `Encrypt`, `LdapWrapper`,
`FFMpegWrapper`, ...) più `pqxx`/`pq` (Postgres), `xml2` (riscrittura XML dei manifest), `curl`, `fcgi`,
OpenSSL (`crypto`/`crypt`) e — poiché il binario collega l'intero stack di transcodifica/codec — OpenCV,
FFmpeg, ImageMagick e varie librerie di codec (`x264`, `x265`, `srt`, `fdk-aac`, ecc.) nel percorso di
build Ubuntu.
