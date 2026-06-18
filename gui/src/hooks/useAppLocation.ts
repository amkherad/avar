import { useCallback, useSyncExternalStore } from "react";
import {
  buildAppHash,
  defaultAppLocation,
  navigateAppLocation,
  readAppLocation,
  subscribeAppLocation,
  type AppLocation,
  type DashboardAction,
} from "@/lib/appRouting";
import type { AppPage } from "@/components/layout/Sidebar";
import type { SettingsCategory } from "@/pages/SettingsPage";

function getServerSnapshot(): AppLocation {
  return defaultAppLocation();
}

export function useAppLocation(): AppLocation {
  return useSyncExternalStore(subscribeAppLocation, readAppLocation, getServerSnapshot);
}

export function useAppNavigation() {
  const location = useAppLocation();

  const navigate = useCallback((next: AppLocation, replace = false) => {
    navigateAppLocation(next, replace);
  }, []);

  const goToPage = useCallback(
    (page: AppPage) => {
      navigate({
        ...location,
        page,
        dashboardAction: page === "dashboard" ? location.dashboardAction : null,
      });
    },
    [location, navigate],
  );

  const openSettings = useCallback(
    (category: SettingsCategory) => {
      navigate({
        page: "settings",
        settingsCategory: category,
        helpTopicId: location.helpTopicId,
        dashboardAction: null,
      });
    },
    [location.helpTopicId, navigate],
  );

  const setHelpTopic = useCallback(
    (helpTopicId: string) => {
      navigate({
        page: "help",
        settingsCategory: location.settingsCategory,
        helpTopicId,
        dashboardAction: null,
      });
    },
    [location.settingsCategory, navigate],
  );

  const setSettingsCategory = useCallback(
    (settingsCategory: SettingsCategory) => {
      navigate({
        page: "settings",
        settingsCategory,
        helpTopicId: location.helpTopicId,
        dashboardAction: null,
      });
    },
    [location.helpTopicId, navigate],
  );

  const setDashboardAction = useCallback(
    (dashboardAction: DashboardAction | null) => {
      navigate({
        page: "dashboard",
        settingsCategory: location.settingsCategory,
        helpTopicId: location.helpTopicId,
        dashboardAction,
      });
    },
    [location.helpTopicId, location.settingsCategory, navigate],
  );

  return {
    location,
    hash: buildAppHash(location),
    navigate,
    goToPage,
    openSettings,
    setHelpTopic,
    setSettingsCategory,
    setDashboardAction,
  };
}
