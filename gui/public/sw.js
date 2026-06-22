const CACHE_VERSION = "__CACHE_VERSION__";
const CACHE_NAME = `avar-gui-${CACHE_VERSION}`;
const OFFLINE_URLS = ["./", "./index.html", "./icon.svg", "./manifest.webmanifest"];

function isNavigationRequest(request) {
  return request.mode === "navigate" || request.destination === "document";
}

function isStaticAsset(request) {
  const dest = request.destination;
  return (
    dest === "script" ||
    dest === "style" ||
    dest === "font" ||
    dest === "image" ||
    /\.(?:js|css|woff2?|png|svg|webp)$/i.test(new URL(request.url).pathname)
  );
}

function isCacheableResponse(response) {
  return response.ok && response.type !== "opaque";
}

async function cacheResponse(cache, request, response) {
  if (!isCacheableResponse(response)) {
    return;
  }
  await cache.put(request, response.clone());
}

async function networkFirstNavigation(request) {
  try {
    const response = await fetch(request);
    if (isCacheableResponse(response)) {
      const cache = await caches.open(CACHE_NAME);
      await cacheResponse(cache, request, response);
    }
    return response;
  } catch (error) {
    if (navigator.onLine) {
      throw error;
    }
    const cached = await caches.match(request);
    if (cached) {
      return cached;
    }
    throw error;
  }
}

async function staleWhileRevalidate(request) {
  const cache = await caches.open(CACHE_NAME);
  const cached = await caches.match(request);

  const networkPromise = fetch(request)
    .then(async (response) => {
      await cacheResponse(cache, request, response);
      return response;
    })
    .catch(() => undefined);

  if (cached) {
    void networkPromise;
    return cached;
  }

  const response = await networkPromise;
  if (response) {
    return response;
  }

  return Response.error();
}

self.addEventListener("install", (event) => {
  event.waitUntil(
    caches
      .open(CACHE_NAME)
      .then((cache) => cache.addAll(OFFLINE_URLS))
      .then(() => self.skipWaiting()),
  );
});

self.addEventListener("message", (event) => {
  if (event.data?.type === "SKIP_WAITING") {
    self.skipWaiting();
  }
});

self.addEventListener("activate", (event) => {
  event.waitUntil(
    caches
      .keys()
      .then((keys) =>
        Promise.all(keys.filter((key) => key !== CACHE_NAME).map((key) => caches.delete(key))),
      )
      .then(() => self.clients.claim()),
  );
});

self.addEventListener("fetch", (event) => {
  if (event.request.method !== "GET") {
    return;
  }

  const url = new URL(event.request.url);
  if (url.pathname.startsWith("/api") || url.pathname.endsWith("/sw.js")) {
    return;
  }

  if (isNavigationRequest(event.request)) {
    event.respondWith(networkFirstNavigation(event.request));
    return;
  }

  if (isStaticAsset(event.request)) {
    event.respondWith(staleWhileRevalidate(event.request));
    return;
  }

  event.respondWith(
    fetch(event.request).catch(async () => {
      const cached = await caches.match(event.request);
      return cached ?? Response.error();
    }),
  );
});

self.addEventListener("notificationclick", (event) => {
  event.notification.close();
  event.waitUntil(
    self.clients.matchAll({ type: "window", includeUncontrolled: true }).then((clients) => {
      for (const client of clients) {
        if ("focus" in client) {
          return client.focus();
        }
      }
      if (self.clients.openWindow) {
        return self.clients.openWindow("./");
      }
      return undefined;
    }),
  );
});

self.addEventListener("push", (event) => {
  if (!event.data) {
    return;
  }

  let payload = { title: "Avar", body: "" };
  try {
    payload = event.data.json();
  } catch {
    payload.body = event.data.text();
  }

  event.waitUntil(
    self.registration.showNotification(payload.title ?? "Avar", {
      body: payload.body,
      icon: "./icon.svg",
      badge: "./icon.svg",
      tag: payload.tag ?? "avar-push",
    }),
  );
});
