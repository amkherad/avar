# Electron → Qt parity

Last audit: `2026-09-27T14:37:38.034064+00:00`

**Score: 48/61 (78.7%)** — auto-updated by `scripts/parity_audit.py`.

## Gaps (Electron yes, Qt no)

- [ ] `settings:about` — nav only
- [ ] `settings:browser` — nav only
- [ ] `settings:daemon` — nav only
- [ ] `settings:downloads` — nav only
- [ ] `settings:general` — nav only
- [ ] `settings:queues` — nav only
- [ ] `settings:shortcuts` — nav only
- [ ] `daemon:addBookmark`
- [ ] `daemon:hasBookmark`
- [ ] `daemon:listBookmarks`
- [ ] `daemon:removeBookmark`
- [ ] `i18n:locale-en` — electron=550 keys, qt≈0 hardcoded
- [ ] `desktop:window-controls` — native OS title bar (minimize/close)

## Matched

- [x] `page:AddDownloadPopupPage`
- [x] `page:BatchAddDownloadsPopupPage`
- [x] `page:ConfirmDialogPopupPage`
- [x] `page:DashboardPage`
- [x] `page:DownloadDetailPopupPage`
- [x] `page:HelpPage`
- [x] `page:SettingsPage`
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
- [x] `theme:tokens`
- [x] `layout:footer`
- [x] `layout:console`
- [x] `layout:session-selector`
- [x] `sync:websocket-sse`
- [x] `sync:snapshot-parser`
- [x] `desktop:tray`
- [x] `extension:bridge-parity`
- [x] `ui:component-surface`

## Loop

Run `./scripts/parity_audit.py` after each parity pass. 
Agent loop: fix top gap, re-audit, repeat until score is 100%.
