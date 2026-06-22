const AvarContextMenu = {
  IDS: {
    ROOT: "avar-root",
    DOWNLOAD_ALL: "avar-download-all",
    DOWNLOAD_SELECTED: "avar-download-selected",
    DOWNLOAD_LINK: "avar-download-link",
  },

  async removeMenus(menusApi) {
    for (const id of Object.values(this.IDS)) {
      try {
        await menusApi.remove(id);
      } catch {
        // Menu may not exist yet.
      }
    }
  },

  async rebuild(menusApi, hasSelectedLinks) {
    await this.removeMenus(menusApi);
    const pageContexts = ["page", "frame"];

    if (hasSelectedLinks) {
      await menusApi.create({
        id: this.IDS.ROOT,
        title: "Avar",
        contexts: pageContexts,
      });
      await menusApi.create({
        id: this.IDS.DOWNLOAD_SELECTED,
        parentId: this.IDS.ROOT,
        title: "Download selected item(s)",
        contexts: pageContexts,
      });
      await menusApi.create({
        id: this.IDS.DOWNLOAD_ALL,
        parentId: this.IDS.ROOT,
        title: "Download all media",
        contexts: pageContexts,
      });
    } else {
      await menusApi.create({
        id: this.IDS.DOWNLOAD_ALL,
        title: "Avar — Download all media",
        contexts: pageContexts,
      });
    }

    await menusApi.create({
      id: this.IDS.DOWNLOAD_LINK,
      title: "Download with Avar",
      contexts: ["link"],
    });
  },
};

globalThis.AvarContextMenu = AvarContextMenu;
