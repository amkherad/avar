# Electron → Qt parity

Last audit: `2026-09-27T11:28:42.234002+00:00`

**Score: 57/57 (100.0%)** — auto-updated by `scripts/parity_audit.py`.

## Gaps (Electron yes, Qt no)


## Matched

- [x] `page:AddDownloadPopupPage`
- [x] `page:BatchAddDownloadsPopupPage`
- [x] `page:ConfirmDialogPopupPage`
- [x] `page:DashboardPage`
- [x] `page:DownloadDetailPopupPage`
- [x] `page:HelpPage`
- [x] `page:SettingsPage`
- [x] `settings:about`
- [x] `settings:browser`
- [x] `settings:daemon`
- [x] `settings:downloads`
- [x] `settings:general`
- [x] `settings:queues`
- [x] `settings:shortcuts`
- [x] `daemon:addDownload`
- [x] `daemon:addQueue`
- [x] `daemon:browseDirectory`
- [x] `daemon:cliExec`
- [x] `daemon:computeDownloadChecksum`
- [x] `daemon:dismissResumePrompt`
- [x] `daemon:editQueue`
- [x] `daemon:getConfig`
- [x] `daemon:getDownloadDetails`
- [x] `daemon:getLogs`
- [x] `daemon:health`
- [x] `daemon:listDownloads`
- [x] `daemon:listQueues`
- [x] `daemon:pauseDownload`
- [x] `daemon:ping`
- [x] `daemon:probeDownloadUrl`
- [x] `daemon:removeDownload`
- [x] `daemon:removeQueue`
- [x] `daemon:resolveDownloadPath`
- [x] `daemon:restartDownload`
- [x] `daemon:resumeDownload`
- [x] `daemon:setConfig`
- [x] `daemon:setDownloadSource`
- [x] `daemon:setDownloadUrl`
- [x] `daemon:skipLogCursor`
- [x] `daemon:startDownload`
- [x] `daemon:startQueue`
- [x] `daemon:stopDownload`
- [x] `daemon:stopQueue`
- [x] `daemon:systemStats`
- [x] `daemon:unwatchDownloadProgress`
- [x] `daemon:watchDownloadProgress`
- [x] `i18n:locale-en`
- [x] `theme:tokens`
- [x] `layout:footer`
- [x] `layout:console`
- [x] `layout:session-selector`
- [x] `sync:websocket-sse`
- [x] `sync:snapshot-parser`
- [x] `desktop:tray`
- [x] `desktop:window-controls`
- [x] `extension:bridge-parity`
- [x] `ui:component-surface`

## Loop

Run `./scripts/parity_audit.py` after each parity pass. 
Agent loop: fix top gap, re-audit, repeat until score is 100%.
