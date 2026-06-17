import { useEffect, useRef, useState } from "react";
import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import { faChevronDown, faPen, faTrash } from "@fortawesome/free-solid-svg-icons";
import { createDaemonClient } from "@/api/daemon";
import { Button } from "@/components/ui/Button";
import { Input } from "@/components/ui/Input";
import { Modal } from "@/components/ui/Modal";
import { useConfigStore } from "@/stores/configStore";
import { useConnectionStore } from "@/stores/connectionStore";
import { showConfirmDialog } from "@/lib/popup";

function newSessionId(): string {
  return `session-${Date.now().toString(36)}`;
}

export function SessionSelector() {
  const { t } = useTranslation();
  const config = useConfigStore((s) => s.config);
  const updateConfig = useConfigStore((s) => s.updateConfig);
  const setConfig = useConfigStore((s) => s.setConfig);
  const setSessionAuthToken = useConfigStore((s) => s.setSessionAuthToken);
  const getSessionWithSecrets = useConfigStore((s) => s.getSessionWithSecrets);
  const connection = useConnectionStore((s) => s.connection);

  const [menuOpen, setMenuOpen] = useState(false);
  const [modalOpen, setModalOpen] = useState(false);
  const [editingId, setEditingId] = useState<string | null>(null);
  const [label, setLabel] = useState("");
  const [baseUrl, setBaseUrl] = useState("http://127.0.0.1:8000");
  const [authToken, setAuthToken] = useState("");
  const [useRelativeApi, setUseRelativeApi] = useState(false);
  const [testResult, setTestResult] = useState<"idle" | "ok" | "fail">("idle");

  const containerRef = useRef<HTMLDivElement>(null);

  const activeSession =
    config.sessions.find((s) => s.id === config.activeSessionId) ?? config.sessions[0];

  const connectionLabel =
    connection === "connected"
      ? t("session.connected")
      : connection === "connecting"
        ? t("session.connecting")
        : t("session.disconnected");

  useEffect(() => {
    if (!menuOpen) {
      return;
    }

    function handleClickOutside(event: MouseEvent) {
      if (containerRef.current && !containerRef.current.contains(event.target as Node)) {
        setMenuOpen(false);
      }
    }

    function handleEscape(event: KeyboardEvent) {
      if (event.key === "Escape") {
        setMenuOpen(false);
      }
    }

    document.addEventListener("mousedown", handleClickOutside);
    document.addEventListener("keydown", handleEscape);
    return () => {
      document.removeEventListener("mousedown", handleClickOutside);
      document.removeEventListener("keydown", handleEscape);
    };
  }, [menuOpen]);

  function openAdd() {
    setEditingId(null);
    setLabel("");
    setBaseUrl("http://127.0.0.1:8000");
    setAuthToken("");
    setUseRelativeApi(false);
    setTestResult("idle");
    setMenuOpen(false);
    setModalOpen(true);
  }

  function openEdit(id: string) {
    const session = getSessionWithSecrets(
      config.sessions.find((s) => s.id === id) ?? config.sessions[0],
    );
    if (!session || session.builtin) {
      return;
    }
    setEditingId(id);
    setLabel(session.label);
    setBaseUrl(session.baseUrl);
    setAuthToken(session.authToken ?? "");
    setUseRelativeApi(session.useRelativeApi ?? false);
    setTestResult("idle");
    setMenuOpen(false);
    setModalOpen(true);
  }

  function saveSession() {
    const sessionId = editingId ?? newSessionId();
    const payload = {
      id: sessionId,
      label: label.trim() || "Session",
      baseUrl: baseUrl.trim(),
      useRelativeApi,
    };

    const sessions = editingId
      ? config.sessions.map((s) => (s.id === editingId ? { ...s, ...payload } : s))
      : [...config.sessions, payload];

    setSessionAuthToken(sessionId, authToken);
    setConfig({
      ...config,
      sessions,
      activeSessionId: editingId ?? sessionId,
    });
    setModalOpen(false);
  }

  async function removeSession(id: string) {
    const session = config.sessions.find((s) => s.id === id);
    if (!session || session.builtin) {
      return;
    }

    const result = await showConfirmDialog({
      title: t("session.remove"),
      message: t("session.removeConfirm", { name: session.label }),
      confirmLabel: t("session.remove"),
      cancelLabel: t("common.cancel"),
    });
    if (!result.confirmed) {
      return;
    }

    const sessions = config.sessions.filter((s) => s.id !== id);
    if (sessions.length === 0) {
      return;
    }

    setSessionAuthToken(id, undefined);
    setConfig({
      ...config,
      sessions,
      activeSessionId:
        config.activeSessionId === id ? sessions[0].id : config.activeSessionId,
    });
    setMenuOpen(false);
  }

  function selectSession(id: string) {
    updateConfig({ activeSessionId: id });
    setMenuOpen(false);
  }

  async function testConnection() {
    setTestResult("idle");
    const client = createDaemonClient({
      baseUrl,
      authToken: authToken || undefined,
      useRelativeApi,
    });
    const ok = await client.ping();
    setTestResult(ok ? "ok" : "fail");
  }

  if (!activeSession) {
    return null;
  }

  return (
    <div className="avar-session-selector" ref={containerRef}>
      <button
        type="button"
        className="avar-session-selector__trigger"
        aria-expanded={menuOpen}
        aria-haspopup="listbox"
        onClick={() => setMenuOpen((open) => !open)}
      >
        <span
          className={`avar-connection__dot ${connection === "connected" ? "avar-connection__dot--ok" : ""}`}
        />
        <span className="avar-session-selector__info">
          <span className="avar-session-selector__label">{activeSession.label}</span>
          <span className="avar-session-selector__status">{connectionLabel}</span>
        </span>
        <span className={`avar-session-selector__chevron ${menuOpen ? "avar-session-selector__chevron--open" : ""}`}>
          <FontAwesomeIcon icon={faChevronDown} />
        </span>
      </button>

      {menuOpen ? (
        <div className="avar-session-selector__menu" role="listbox" aria-label={t("session.title")}>
          <ul className="avar-list avar-session-list">
            {config.sessions.map((session) => (
              <li key={session.id}>
                <div
                  className={`avar-list__item ${config.activeSessionId === session.id ? "avar-list__item--active" : ""}`}
                  style={{ cursor: "default" }}
                >
                  <div className="avar-list__item-row">
                    <button
                      type="button"
                      className="avar-list__item-main"
                      role="option"
                      aria-selected={config.activeSessionId === session.id}
                      onClick={() => selectSession(session.id)}
                    >
                      <span className="avar-list__title">{session.label}</span>
                      <span className="avar-list__meta">
                        {session.useElectronProxy ? t("session.electronTunnel") : session.baseUrl}
                      </span>
                    </button>
                    <div className="avar-list__item-actions">
                      {!session.builtin ? (
                        <>
                          <Button size="sm" variant="ghost" onClick={() => openEdit(session.id)}>
                            <FontAwesomeIcon icon={faPen} />
                          </Button>
                          <Button
                            size="sm"
                            variant="ghost"
                            aria-label={t("session.remove")}
                            title={t("session.remove")}
                            onClick={() => void removeSession(session.id)}
                          >
                            <FontAwesomeIcon icon={faTrash} />
                          </Button>
                        </>
                      ) : null}
                    </div>
                  </div>
                </div>
              </li>
            ))}
          </ul>
          <Button size="sm" variant="secondary" className="avar-session-selector__add" onClick={openAdd}>
            {t("session.add")}
          </Button>
        </div>
      ) : null}

      <Modal
        open={modalOpen}
        title={editingId ? t("session.edit") : t("session.add")}
        onClose={() => setModalOpen(false)}
        footer={
          <>
            <Button variant="secondary" onClick={() => setModalOpen(false)}>
              {t("common.cancel")}
            </Button>
            <Button variant="secondary" onClick={() => void testConnection()}>
              {t("session.test")}
            </Button>
            <Button onClick={saveSession}>{t("common.save")}</Button>
          </>
        }
      >
        <Input label={t("session.label")} value={label} onChange={(e) => setLabel(e.target.value)} />
        <Input
          label={t("session.baseUrl")}
          value={baseUrl}
          onChange={(e) => setBaseUrl(e.target.value)}
          disabled={useRelativeApi}
        />
        <Input
          label={t("session.authToken")}
          type="password"
          value={authToken}
          onChange={(e) => setAuthToken(e.target.value)}
          autoComplete="off"
        />
        <label className="avar-checkbox-row">
          <input
            type="checkbox"
            checked={useRelativeApi}
            onChange={(e) => setUseRelativeApi(e.target.checked)}
          />
          {t("session.useProxy")}
        </label>
        {testResult === "ok" ? (
          <span style={{ color: "var(--avar-success)" }}>{t("session.connected")}</span>
        ) : null}
        {testResult === "fail" ? (
          <span style={{ color: "var(--avar-danger)" }}>{t("session.disconnected")}</span>
        ) : null}
      </Modal>
    </div>
  );
}
