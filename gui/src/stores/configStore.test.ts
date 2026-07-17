import { beforeEach, describe, expect, it } from "vitest";
import { defaultGuiConfig, GUI_CONFIG_KEY, GUI_CONFIG_VERSION } from "@/config/defaults";
import { useConfigStore, waitForConfigHydration } from "@/stores/configStore";

describe("configStore persistence", () => {
  beforeEach(() => {
    localStorage.clear();
    useConfigStore.persist.clearStorage();
    useConfigStore.setState({
      config: defaultGuiConfig(),
      sessionSecrets: {},
    });
  });

  it("restores GUI preferences from localStorage after rehydrate", async () => {
    localStorage.setItem(
      GUI_CONFIG_KEY,
      JSON.stringify({
        state: {
          config: {
            ...defaultGuiConfig(),
            theme: "dark",
            downloadPageSize: 250,
          },
          sessionSecrets: {},
        },
        version: GUI_CONFIG_VERSION,
      }),
    );

    await useConfigStore.persist.rehydrate();
    await waitForConfigHydration();

    const { config } = useConfigStore.getState();
    expect(config.theme).toBe("dark");
    expect(config.downloadPageSize).toBe(250);
  });

  it("migrates legacy flat GUI config blobs", async () => {
    localStorage.setItem(
      GUI_CONFIG_KEY,
      JSON.stringify({
        ...defaultGuiConfig(),
        theme: "queen-mode",
        version: GUI_CONFIG_VERSION,
      }),
    );

    await useConfigStore.persist.rehydrate();
    await waitForConfigHydration();

    expect(useConfigStore.getState().config.theme).toBe("queen-mode");
  });

  it("persists GUI preferences after updateConfig", async () => {
    await useConfigStore.persist.rehydrate();
    await waitForConfigHydration();

    useConfigStore.getState().updateConfig({ theme: "dark", downloadPageSize: 250 });
    await new Promise((resolve) => setTimeout(resolve, 0));

    const raw = localStorage.getItem(GUI_CONFIG_KEY);
    expect(raw).toBeTruthy();
    const parsed = JSON.parse(raw!) as {
      state: { config: { theme?: string; downloadPageSize?: number } };
    };
    expect(parsed.state.config.theme).toBe("dark");
    expect(parsed.state.config.downloadPageSize).toBe(250);
  });
});
