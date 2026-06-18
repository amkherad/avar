import i18n from "i18next";
import { initReactI18next } from "react-i18next";
import en from "./locales/en.json";
import fa from "./locales/fa.json";

export const supportedLocales = ["en", "fa"] as const;
export type SupportedLocale = (typeof supportedLocales)[number];

export const rtlLocales: SupportedLocale[] = ["fa"];

export function isRtlLocale(locale: string): boolean {
  return rtlLocales.includes(locale as SupportedLocale);
}

void i18n.use(initReactI18next).init({
  resources: {
    en: { translation: en },
    fa: { translation: fa },
  },
  lng: "en",
  fallbackLng: "en",
  interpolation: { escapeValue: false },
});

export default i18n;
