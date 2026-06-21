import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import { getBuildInfo } from "@/lib/buildInfo";
import { useConnectionStore } from "@/stores/connectionStore";

const GITHUB_REPO = "https://github.com/amkherad/avar";
const GITHUB_AUTHOR = "https://github.com/amkherad";
const GITHUB_SPONSORS = "https://github.com/sponsors/amkherad";

function parseBackendVersion(output: string | undefined): string | null {
  if (!output) {
    return null;
  }
  const match = /Avar version:\s*(\S+)/i.exec(output);
  const trimmed = output.trim();
  return match?.[1] ?? (trimmed || null);
}

export function AboutSettings() {
  const { t } = useTranslation();
  const build = getBuildInfo();
  const client = useConnectionStore((s) => s.client);
  const connection = useConnectionStore((s) => s.connection);
  const [backendVersion, setBackendVersion] = useState<string | null>(null);
  const [backendLoading, setBackendLoading] = useState(false);

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

  return (
    <div className="avar-about">
      <p className="avar-about__intro">{t("settings.about.intro")}</p>

      <section className="avar-about__section">
        <h3 className="avar-about__heading">{t("settings.about.versionTitle")}</h3>
        <dl className="avar-settings-build__list">
          <div>
            <dt>{t("settings.about.frontendVersion")}</dt>
            <dd>{build.version}</dd>
          </div>
          <div>
            <dt>{t("settings.about.backendVersion")}</dt>
            <dd>
              {connection !== "connected"
                ? t("settings.about.backendDisconnected")
                : backendLoading
                  ? t("settings.about.backendLoading")
                  : (backendVersion ?? t("settings.about.backendUnknown"))}
            </dd>
          </div>
          {build.date ? (
            <div>
              <dt>{t("settings.buildDate")}</dt>
              <dd>{build.date}</dd>
            </div>
          ) : null}
          {build.commit ? (
            <div>
              <dt>{t("settings.buildCommit")}</dt>
              <dd className="avar-settings-build__mono">{build.commit}</dd>
            </div>
          ) : null}
        </dl>
      </section>

      <section className="avar-about__section">
        <h3 className="avar-about__heading">{t("settings.about.authorTitle")}</h3>
        <p className="avar-settings-hint">{t("settings.about.authorText")}</p>
        <div className="avar-about__actions">
          <a
            className="avar-btn avar-btn--secondary avar-btn--md avar-about__link-btn"
            href={GITHUB_AUTHOR}
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
            href={`${GITHUB_REPO}#license`}
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
            href={GITHUB_SPONSORS}
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
            href={GITHUB_SPONSORS}
            target="_blank"
            rel="noopener noreferrer"
          >
            {t("settings.about.donateButtonSponsor")}
          </a>
          <a
            className="avar-btn avar-btn--secondary avar-btn--md avar-about__link-btn"
            href={GITHUB_REPO}
            target="_blank"
            rel="noopener noreferrer"
          >
            {t("settings.about.donateButtonStar")}
          </a>
        </div>
      </section>
    </div>
  );
}
