# NewLifeItaly Viewer — installer Windows 64 bit

Questo repository produce un vero installer Windows (`Setup.exe`) del viewer
ufficiale NewLifeItaly, basato su Firestorm OpenSim.

## Creare l'installer

1. Aprire la scheda **Actions** del repository GitHub.
2. Selezionare **Build NewLifeItaly Viewer for Windows**.
3. Premere **Run workflow** e confermare.
4. Attendere il completamento del job **Windows 64-bit Setup**.
5. Aprire il risultato e scaricare l'artefatto
   **NewLifeItaly-Viewer-Windows-x64**.
6. Estrarre lo ZIP e avviare il file `NewLifeItaly_Viewer_Windows_x64-*_Setup.exe`.

L'artefatto contiene anche il file `.sha256`, utilizzabile per verificare che
il download non sia stato alterato.

## Configurazione incorporata

- prodotto: **NewLifeItaly Viewer**;
- piattaforma: Windows 64 bit;
- base: Firestorm OpenSim 7.2.4;
- grid unica: `http://newlifeitaly.com:8002`;
- Hypergrid mantenuto;
- pannello regionale Cam, Microfono e Multiview;
- servizio media: `https://newlifeitaly.com/viewer-media/`.

## Firma Windows

Le prime build di collaudo non sono firmate digitalmente e Windows SmartScreen
può mostrare un avviso. Prima della distribuzione pubblica va collegato un
certificato di firma codice intestato al distributore NewLifeItaly.

Il progetto conserva le licenze e le attribuzioni del codice Firestorm da cui
deriva. Le modifiche NewLifeItaly restano nello stesso repository pubblico per
rispettare i termini LGPL applicabili.
