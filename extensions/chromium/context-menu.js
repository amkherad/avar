const AvarContextMenu = {
  IDS: {
    ROOT: "avar-root",
    DOWNLOAD_ALL: "avar-download-all",
    DOWNLOAD_SELECTED: "avar-download-selected",
    DOWNLOAD_ALL_SUB: "avar-download-all-sub",
    DOWNLOAD_LINK: "avar-download-link",
  },

  menusReady: false,

  isDownloadAllId(menuItemId) {
    return (
      menuItemId === this.IDS.DOWNLOAD_ALL || menuItemId === this.IDS.DOWNLOAD_ALL_SUB
    );
  },

  async removeMenus(menusApi) {
    for (const id of Object.values(this.IDS)) {
      try {
        await menusApi.remove(id);
      } catch {
        // Menu may not exist yet.
      }
    }
    this.menusReady = false;
  },

  async createMenuItem(menusApi, properties) {
    try {
      await menusApi.create(properties);
    } catch {
      // Item may already exist after a service worker restart.
    }
  },

  async ensureMenus(menusApi) {
    if (this.menusReady) {
      return;
    }

    const pageContexts = ["page", "frame"];

    await this.createMenuItem(menusApi, {
      id: this.IDS.ROOT,
      title: "Avar integration",
      contexts: pageContexts,
    });
    await this.createMenuItem(menusApi, {
      id: this.IDS.DOWNLOAD_SELECTED,
      parentId: this.IDS.ROOT,
      title: "Download selected item(s)",
      contexts: pageContexts,
      enabled: false,
    });
    await this.createMenuItem(menusApi, {
      id: this.IDS.DOWNLOAD_ALL_SUB,
      parentId: this.IDS.ROOT,
      title: "Download all media",
      contexts: pageContexts,
    });
    await this.createMenuItem(menusApi, {
      id: this.IDS.DOWNLOAD_LINK,
      title: "Download with Avar",
      contexts: ["link"],
    });

    this.menusReady = true;
  },

  async setHasSelectedLinks(menusApi, hasSelectedLinks) {
    await this.ensureMenus(menusApi);
    await menusApi.update(this.IDS.DOWNLOAD_SELECTED, { enabled: hasSelectedLinks });
    if (typeof menusApi.refresh === "function") {
      menusApi.refresh();
    }
  },

  async install(menusApi) {
    await this.removeMenus(menusApi);
    await this.ensureMenus(menusApi);
    await this.setHasSelectedLinks(menusApi, false);
  },
};

globalThis.AvarContextMenu = AvarContextMenu;
