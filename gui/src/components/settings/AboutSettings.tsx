import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import { getBuildInfo } from "@/lib/buildInfo";
import {
  getHostPlatformLabelKey,
  getHostShellLabelKey,
  getHostVersionLabelKey,
  listHostVersionEntries,
  resolveHostInfo,
  type HostInfo,
} from "@/lib/hostInfo";
import { getProjectUrls } from "@/lib/projectUrls";
import { useConnectionStore } from "@/stores/connectionStore";

function parseBackendVersion(output: string | undefined): string | null {
  if (!output) {
    return null;
  }
  const match = /Avar version:\s*(\S+)/i.exec(output);
  const trimmed = output.trim();
  return match?.[1] ?? (trimmed || null);
}

interface VersionRow {
  label: string;
  value: string;
  mono?: boolean;
}

export function AboutSettings() {
  const { t } = useTranslation();
  const build = getBuildInfo();
  const urls = getProjectUrls();
  const client = useConnectionStore((s) => s.client);
  const connection = useConnectionStore((s) => s.connection);
  const [backendVersion, setBackendVersion] = useState<string | null>(null);
  const [backendLoading, setBackendLoading] = useState(false);
  const [hostInfo, setHostInfo] = useState<HostInfo | null>(null);

  useEffect(() => {
    let cancelled = false;

    void resolveHostInfo().then((info) => {
      if (!cancelled) {
        setHostInfo(info);
      }
    });

    return () => {
      cancelled = true;
    };
  }, []);

  useEffect(() => {
    if (connection !== "connected" || !client) {
      setBackendVersion(null);
      return;
    }

    let cancelled = false;
    setBackendLoading(true);

    void client
      .cliExec(["avar", "--version"])
      .then((result) => {
        if (!cancelled) {
          setBackendVersion(parseBackendVersion(result.output));
        }
      })
      .catch(() => {
        if (!cancelled) {
          setBackendVersion(null);
        }
      })
      .finally(() => {
        if (!cancelled) {
          setBackendLoading(false);
        }
      });

    return () => {
      cancelled = true;
    };
  }, [client, connection]);

  const versionRows: VersionRow[] = [
    { label: t("settings.about.frontendVersion"), value: build.version },
    {
      label: t("settings.about.backendVersion"),
      value:
        connection !== "connected"
          ? t("settings.about.backendDisconnected")
          : backendLoading
            ? t("settings.about.backendLoading")
            : (backendVersion ?? t("settings.about.backendUnknown")),
    },
  ];

  if (build.date) {
    versionRows.push({ label: t("settings.buildDate"), value: build.date });
  }
  if (build.commit) {
    versionRows.push({
      label: t("settings.buildCommit"),
      value: build.commit,
      mono: true,
    });
  }

  const hostRows: VersionRow[] = [];
  if (hostInfo) {
    hostRows.push({
      label: t("settings.about.hostShell"),
      value: t(getHostShellLabelKey(hostInfo.shell)),
    });

    const platformLabelKey = getHostPlatformLabelKey(hostInfo.platform);
    if (platformLabelKey) {
      hostRows.push({
        label: t("settings.about.hostPlatform"),
        value: t(platformLabelKey),
      });
    }

    for (const entry of listHostVersionEntries(hostInfo)) {
      hostRows.push({
        label: t(getHostVersionLabelKey(entry.key)),
        value: entry.value,
        mono: true,
      });
    }
  }

  return (
    <div className="avar-about">
      <p className="avar-about__intro">{t("settings.about.intro")}</p>

      <section className="avar-about__section">
        <h3 className="avar-about__heading">{t("settings.about.versionTitle")}</h3>
        <table className="avar-about-version-table">
          <tbody>
            {versionRows.map((row) => (
              <tr key={row.label}>
                <th scope="row">{row.label}</th>
                <td className={row.mono ? "avar-about-version-table__mono" : undefined}>
                  {row.value}
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </section>

      {hostRows.length > 0 ? (
        <section className="avar-about__section">
          <h3 className="avar-about__heading">{t("settings.about.hostTitle")}</h3>
          <table className="avar-about-version-table">
            <tbody>
              {hostRows.map((row) => (
                <tr key={row.label}>
                  <th scope="row">{row.label}</th>
                  <td className={row.mono ? "avar-about-version-table__mono" : undefined}>
                    {row.value}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </section>
      ) : null}

      <section className="avar-about__section">
        <h3 className="avar-about__heading">{t("settings.about.authorTitle")}</h3>
        <p className="avar-settings-hint">{t("settings.about.authorText")}</p>
        <div className="avar-about__actions">
          <a
            className="avar-btn avar-btn--secondary avar-btn--md avar-about__link-btn"
            href={urls.author}
            target="_blank"
            rel="noopener noreferrer"
          >
            {t("settings.about.authorButton")}
          </a>
        </div>
      </section>

      <section className="avar-about__section">
        <h3 className="avar-about__heading">{t("settings.about.licenseTitle")}</h3>
        <p className="avar-settings-hint">{t("settings.about.licenseText")}</p>
        <div className="avar-about__actions">
          <a
            className="avar-btn avar-btn--secondary avar-btn--md avar-about__link-btn"
            href={urls.license}
            target="_blank"
            rel="noopener noreferrer"
          >
            {t("settings.about.licenseButton")}
          </a>
        </div>
      </section>

      <section className="avar-about__section avar-about__section--highlight">
        <h3 className="avar-about__heading">{t("settings.about.sponsorsTitle")}</h3>
        <p className="avar-settings-hint">{t("settings.about.sponsorsText")}</p>
        <div className="avar-about__actions">
          <a
            className="avar-btn avar-btn--primary avar-btn--md avar-about__link-btn"
            href={urls.sponsors}
            target="_blank"
            rel="noopener noreferrer"
          >
            {t("settings.about.sponsorsButton")}
          </a>
        </div>
      </section>

      <section className="avar-about__section">
        <h3 className="avar-about__heading">{t("settings.about.donateTitle")}</h3>
        <p className="avar-settings-hint">{t("settings.about.donateText")}</p>
        <div className="avar-about__actions">
          <a
            className="avar-btn avar-btn--primary avar-btn--md avar-about__link-btn"
            href={urls.sponsors}
            target="_blank"
            rel="noopener noreferrer"
          >
            {t("settings.about.donateButtonSponsor")}
          </a>
          <a
            className="avar-btn avar-btn--secondary avar-btn--md avar-about__link-btn"
            href={urls.repo}
            target="_blank"
            rel="noopener noreferrer"
          >
            {t("settings.about.donateButtonStar")}
          </a>
        </div>
      </section>

      <section className="avar-about__section">
        <h3 className="avar-about__heading">{t("settings.about.reportBugTitle")}</h3>
        <p className="avar-settings-hint">{t("settings.about.reportBugText")}</p>
        <div className="avar-about__actions">
          <a
            className="avar-btn avar-btn--secondary avar-btn--md avar-about__link-btn"
            href={urls.issues}
            target="_blank"
            rel="noopener noreferrer"
          >
            {t("settings.about.reportBugButton")}
          </a>
        </div>
      </section>
    </div>
  );
}
